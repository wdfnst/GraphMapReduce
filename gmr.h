/*************************************************************************/
/*! 此文件主要用来定义与业务逻辑计算相关的函数，如:
 * map、reduce函数, 调度入口函数computing()
*/
/**************************************************************************/

/* 判断是否在控制台打印调试信息 */
#define INFO   false 
#define DEBUG  false 

/* 记录一次迭代中,各个步骤消耗的时间 */
std::map<std::string, double> timeRecorder;
/* 记录程序运行过程中的接收的字节数 */
long recvBytes = 0;

/* 迭代结束标志:0表示还有元素没有达到迭代结束条件, 
 * 默认表示都达到迭代结束条件 */
int iterationCompleted = 0;
/* 迭代要求精度 */
float threshold = 0.0001;
/* 当前迭代残余误差 */
float remainDeviation = FLT_MAX;
/* 迭代次数计数器 */
int iterNum = 0;

/* Map/Reduce编程模型中的键值对,用于作为Map输出和Reduce输入输出*/
struct KV {
    int key; float value;
    bool operator==(int key) {
        return this->key == key;
    }
};

/* 记录当前程序执行到当前位置的MPI_Wtime */
void recordTick(std::string tickname) {
    timeRecorder[tickname] = MPI_Wtime();
}

/* 用于从csr(Compressed Sparse Row)中生成Vertex顶点进行map/reduce */
/* 用于业务逻辑计算，而非图的表示 */
struct Vertex {
    int id, loc, neighborSize, neighbors[512], neighborsloc[512];
    float value, edgewgt[512];
    bool operator==(int key) {
        return this->id == key;
    }
    bool operator<(int key) {
        return id < key;
    }
};

/* 将图中指定id的顶点的值进行更新, 并返回迭代是否结束 */
int updateGraph(graph_t *graph, std::list<KV> &reduceResult) {
    iterationCompleted = 0;
    int i = 0;
    float currentMaxDeviation = 0.0;
    auto iter = reduceResult.begin();
    while (i < graph->nvtxs && iter != reduceResult.end() ) {
        /* 因为reduceResult中的Key包含了所有的子图中的vertex id
         * 所以一旦不等则是reduceResult中过多,直接进行递增即可 */
        if (iter->key != graph->ivsizes[i]) iter++;
        else {
            /* 计算误差，并和老值进行比较, 判断迭代是否结束 */
            float deviation = fabs(iter->value - graph->fvwgts[i]);
            if (deviation > threshold) {
                if (deviation > currentMaxDeviation) currentMaxDeviation = deviation;
                if(INFO) printf("迭代误差: fasb(%f - %f) = %f\n", iter->value,
                        graph->fvwgts[i], fabs(iter->value - graph->fvwgts[i]));
                iterationCompleted = 1;
            }
            graph->fvwgts[i] = iter->value;
            i++; iter++;
        }
    }

    /* 如果当前误差小于全局残余误差则更新全局残余误差 */
    if (currentMaxDeviation < remainDeviation) remainDeviation = currentMaxDeviation;
    if(INFO) printf("迭代残余误差: %f\n", remainDeviation);

    return iterationCompleted;
}

/* Map/Reduce编程模型中的Map函数 */
void map(Vertex &v, std::list<KV> &kvs) {
    if(DEBUG) printf("get in the map function.\n");
    float value = v.value / v.neighborSize;
    for (int i = 0; i < v.neighborSize; i++) {
        kvs.push_back({v.neighbors[i], value});
        if(DEBUG) printf("%d %f\n", v.neighbors[i], value);
    }
}

/* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
void sort(std::list<KV> &kvs) { }

/* Map/Reduce编程模型中的Reduce函数 */
KV reduce(std::list<KV> &kvs) {
    float sum = 0.0;
    for (auto kv : kvs) {
        sum += kv.value;
    }
    /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
    sum = 0.5 * sum + (1 - 0.5) / graphs[testgraph].nvtx; 
    if (DEBUG) printf("reduce result: %d %f\n", kvs.front().key, sum);
    return {kvs.front().key, sum};
}

/*将单个节点内的顶点映射为Key/value, 对Key排序后，再进行规约*/
void computing(int rank, graph_t *graph, char *rb, int recvbuffersize) {
    iterationCompleted = 0;
    std::list<KV> kvs;
    Vertex vertex;

    recordTick("bgraphmap");
    for (int i=0; i<graph->nvtxs; i++) {
        vertex.id = graph->ivsizes[i];
        vertex.value = graph->fvwgts[i];
        vertex.neighborSize = graph->xadj[i+1] - graph->xadj[i];
        int neighbor_sn = 0;
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            /* graph的邻居点数组中实际上存储的是以0开始的顶点,实际使用是以1开始
             * 所以需要在顶点原值的基础上加1 */
            vertex.neighbors[neighbor_sn++] = graph->adjncy[j] + 1;
        }
        map(vertex, kvs);
    }
    recordTick("egraphmap");

    /* 由接收缓冲区数据构造一个个顶点(vertex), 再交给map处理 */
    /* 产生的Key/value，只记录本节点的顶点的,采用Bloom Filter验证 */
    recordTick("brecvbuffermap");
    for (int i = 0 ; i < recvbuffersize; ) {
        int vid, eid, location, eloc, edgenum = 0;
        float fvwgt, fewgt;
        memcpy(&vid, rb + i, sizeof(int));
        memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
        memcpy(&fvwgt, rb + (i += sizeof(int)), sizeof(float));
        memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
        if(DEBUG) printf(" %d %d %f %d ", vid, location, fvwgt, edgenum);
        vertex.id = vid;
        vertex.loc = location;
        vertex.value = fvwgt;
        vertex.neighborSize = edgenum;
        i += sizeof(int);
        for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
            memcpy(&eid, rb + i, sizeof(int));
            if(DEBUG) printf(" %d", eid + 1);
            vertex.neighbors[j] = eid + 1;
        }
        for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
            memcpy(&eloc, rb + i, sizeof(int));
            if(DEBUG) printf(" %d", eloc);
            vertex.neighborsloc[j] = eloc;
        }
        for (int j = 0; j < edgenum; j++, i += sizeof(float)) {
            memcpy(&fewgt, rb + i, sizeof(float));
            vertex.edgewgt[j] = fewgt;
            if(DEBUG) printf(" %f", fewgt);
        }
        if(DEBUG) printf("\n");
        map(vertex, kvs);
    }
    recordTick("erecvbuffermap");

    /* 对map产生的key/value list进行排序 */
    recordTick("bsort");
    kvs.sort([](KV &kv1, KV &kv2){return kv1.key < kv2.key;});
    recordTick("esort");

    recordTick("breduce");
    std::list<KV> sameKeylist;
    std::list<KV> reduceResult;
    for (KV kv : kvs) {
        if(sameKeylist.size() > 0 && kv.key != sameKeylist.front().key) {
            KV tmpkv = reduce(sameKeylist);
            reduceResult.push_back(tmpkv);
            sameKeylist.clear();
        }
        sameKeylist.push_back(kv);
    }
    reduceResult.push_back(reduce(sameKeylist));
    sameKeylist.clear();
    recordTick("ereduce");

    /* 将最终迭代的结果进行更新到子图上, 并判断迭代是否结束 */
    recordTick("bupdategraph");
    updateGraph(graph, reduceResult);
    recordTick("eupdategraph");
}

/* 返回迭代是否结束 */
int isCompleted(int rank) {
    return iterationCompleted;    
}

/* 打印计算的过程中的信息: 迭代次数, 各个步骤耗时 */
void printTimeConsume() {
    printf("迭代次数:%d, 迭代残余误差:%f, 本次迭代耗时:%f:(%f[exdata] & %f[map] & %f"
            "[reduce] & %f[updategraph] & %f[computing] & %f[exiterfinish])\n", 
            iterNum, remainDeviation,
            timeRecorder["eexchangeiterfinish"] - timeRecorder["bexchangecounts"],
            timeRecorder["eexchangedata"] - timeRecorder["bexchangedata"],
            timeRecorder["erecvbuffermap"] - timeRecorder["bgraphmap"],
            timeRecorder["ereduce"] - timeRecorder["breduce"], 
            timeRecorder["eupdategraph"] - timeRecorder["bupdategraph"], 
            timeRecorder["ecomputing"] - timeRecorder["bcomputing"], 
            timeRecorder["exiterfinish"] - timeRecorder["exiterfinish"]);
}

/*************************************************************************/
/*! 此文件主要用来定义调度业务逻辑的调度函数:
 * computing(), updateGraph()
*/
/**************************************************************************/

/* 判断是否在控制台打印调试信息 */
#define INFO   false 
#define DEBUG  false 

/* timerRecorder:  迭代步的各个子过程耗时
 * totalRecvBytes: 目前为止, 接收到的字节数
 * maxMemory:      目前为止, 消耗的最大内存 */
std::map<std::string, double> timeRecorder;
long totalRecvBytes = 0;
long totalMaxMem    = 0;

/* threahold       : 迭代精度
 * remainDeviation : 当前迭代步的残余误差
 * iterNum         : 当目前为止, 迭代的次数
 * MAX_ITERATION   : 系统允许的最大迭代次数
 * convergentVertex: 本子图已经收敛的顶点个数 */
float threshold         = 0.0001;
float remainDeviation   = FLT_MAX;
int iterNum             = 0;
int MAX_ITERATION       = 10;
size_t convergentVertex = 0;

/* Map/Reduce编程模型中的键值对,用于作为Map输出和Reduce输入输出 */
struct KV {
    int key; float value;
    /* skey, svalue作为结构体KV中辅助存储键值的单元 */
    int skey; std::list<int> svalue;
    KV() {}
    KV(int key, float value) : key(key), value(value), skey(-1) {}
    KV(int key, int skey, float value) : key(key), value(value), skey(skey) {}
};

/* 用于对KV的key进行排序的lambda函数 */
auto KVComp = [](KV &kv1, KV &kv2) {
    if (kv1.key < kv2.key)
        return true;
    else if (kv1.key == kv2.key) {
        if (kv1.skey < kv2.skey)
            return true;
        else
            return false;
    }
    else
        return false;
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

/****************************************************
 * 该类用于定于MapReduce操作的基类
 * *************************************************/
class GMR {
public:
    /* 根据不同的算法, 对图进行初始化 */
    virtual void initGraph(graph_t *graph) = 0;
    /* Map/Reduce编程模型中的Map函数 */
    virtual void map(Vertex &v, std::list<KV> &kvs) = 0;

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    virtual void sort(std::list<KV> &kvs) = 0;

    /* Map/Reduce编程模型中的Reduce函数 */
    virtual KV reduce(std::list<KV> &kvs) = 0;

    /* 比较Key/Value的key */
    virtual int keyComp(KV &kv1, KV &kv2) {
        if (kv1.key == kv2.key)
            return 0;
        else if (kv1.key < kv2.key)
            return -1;
        else
            return 1;
    }

    /* 输出算法的执行结果 */
    virtual void printResult(graph_t *graph) { }

    /* 算法需要迭代的次数, 默认为系统设置的最大迭代次数 */
    static size_t algoIterNum;
};
size_t GMR::algoIterNum = INT_MAX;

/* 将图中指定id的顶点的值进行更新, 并返回迭代是否结束 */
void updateGraph(graph_t *graph, std::list<KV> &reduceResult) {
    int i = 0;
    float currentMaxDeviation = 0.0;
    auto iter = reduceResult.begin();
    /* 将reduceResult中id与graph中vertex.id相同key值更新到vertex.value */
    while (i < graph->nvtxs && iter != reduceResult.end() ) {
        if (iter->key > graph->ivsizes[i]) i++;
        else if (graph->ivsizes[i] > iter->key) iter++;
        else {
            /* 计算误差, 并和老值进行比较, 判断迭代是否结束 */
            float deviation = fabs(iter->value - graph->fvwgts[i]);
            /* 与老值进行比较, 如果变化小于threhold,则将vertex.status设置为inactive */
            if (deviation > threshold) {
                if (deviation > currentMaxDeviation) currentMaxDeviation = deviation;
                if(INFO) printf("迭代误差: fabs(%f - %f) = %f\n", iter->value,
                        graph->fvwgts[i], deviation);
                if(graph->status[i] == inactive) {
                    graph->status[i] = active;
                    convergentVertex--;
                }
            }
            else {
                if(graph->status[i] == active) {
                    convergentVertex++;
                    graph->status[i] = inactive;
                }
            }
            graph->fvwgts[i] = iter->value;
            i++; iter++;
        }
    }

    /* 如果当前误差小于全局残余误差则更新全局残余误差 */
    if (currentMaxDeviation < remainDeviation) remainDeviation = currentMaxDeviation;
    if(INFO) printf("迭代残余误差: %f\n", remainDeviation);
}

/*将单个节点内的顶点映射为Key/value, 对Key排序后，再进行规约*/
void computing(int rank, graph_t *graph, char *rb, int recvbuffersize, GMR *gmr) {
    std::list<KV> kvs;
    Vertex vertex;

    recordTick("bgraphmap");
    for (int i=0; i<graph->nvtxs; i++) {
        vertex.id = graph->ivsizes[i];
        vertex.value = graph->fvwgts[i];
        vertex.neighborSize = graph->xadj[i+1] - graph->xadj[i];
        int neighbor_sn = 0;
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++, neighbor_sn++) {
            /* graph的邻居点数组中实际上存储的是以0开始的顶点,实际使用是以1开始
             * 所以需要在顶点原值的基础上加1 */
            vertex.neighbors[neighbor_sn] = graph->adjncy[j] + 1;
            /* 将边的权重从图中顶点拷贝到vertex.edgewgt[k++]中 */
            vertex.edgewgt[neighbor_sn] = graph->fadjwgt[j];
        }
        gmr->map(vertex, kvs);
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
        gmr->map(vertex, kvs);
    }
    recordTick("erecvbuffermap");

    /* 对map产生的key/value list进行排序 */
    recordTick("bsort");
    kvs.sort(KVComp);
    recordTick("esort");

    recordTick("breduce");
    std::list<KV> sameKeylist;
    std::list<KV> reduceResult;
    for (KV kv : kvs) {
        if(sameKeylist.size() > 0 && gmr->keyComp(kv, sameKeylist.front()) != 0) {
            KV tmpkv = gmr->reduce(sameKeylist);
            reduceResult.push_back(tmpkv);
            sameKeylist.clear();
        }
        sameKeylist.push_back(kv);
    }
    reduceResult.push_back(gmr->reduce(sameKeylist));
    sameKeylist.clear();
    recordTick("ereduce");

    /* 将最终迭代的结果进行更新到子图上, 并判断迭代是否结束 */
    recordTick("bupdategraph");
    updateGraph(graph, reduceResult);
    recordTick("eupdategraph");
}

/* 打印计算的过程中的信息: 迭代次数, 各个步骤耗时 */
void printTimeConsume() {
    printf("迭代次数:%d(%-6.2f\%%), 迭代残余误差:%ef, 本次迭代耗时:%f=(%f[exdata]"
            " + %f[map] + %f" "[reduce] + %f[updategraph] + %f[computing])\n", 
            iterNum, convergentVertex * 1.0 / ntxs * 100 , remainDeviation,
            timeRecorder["eiteration"] - timeRecorder["bexchangecounts"],
            timeRecorder["eexchangedata"] - timeRecorder["bexchangedata"],
            timeRecorder["erecvbuffermap"] - timeRecorder["bgraphmap"],
            timeRecorder["ereduce"] - timeRecorder["breduce"], 
            timeRecorder["eupdategraph"] - timeRecorder["bupdategraph"], 
            timeRecorder["ecomputing"] - timeRecorder["bcomputing"]);
}

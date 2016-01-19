/*************************************************************************/
/*! 此文件主要用来定义与业务逻辑计算相关的函数，如:
 * map、reduce函数, 调度入口函数computing()
*/
/**************************************************************************/

/* 迭代结束标志:0表示还有元素没有达到迭代结束条件, 
 * 默认表示都达到迭代结束条件 */
int iterationCompleted = 0;
/* 迭代精度 */
float threshold = 0.0001;

struct KV {
    int key; float value;
    bool operator==(int key) {
        return this->key == key;
    }
};

struct Vertex {
    int id, loc, neighborSize, neighbors[512]; float value;
    bool operator==(int key) {
        return this->id == key;
    }
    bool operator<(int key) {
        return id < key;
    }
};

void map(Vertex &v, std::list<KV> &kvs) {
    int neighbor_count = 0;
    while(v.neighbors[neighbor_count] != 0)neighbor_count++;
    float value = v.value / neighbor_count;
    for (int i = 0; i < neighbor_count; i++)
        kvs.push_back({v.neighbors[i], value});
}

void sort(std::list<KV> &kvs) { }

KV reduce(std::list<KV> &kvs) {
    float sum = 0.0;
    for (auto kv : kvs) {
        sum += kv.value;
    }
    /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
    sum = 0.5 * sum + (1 - 0.5) / GRAPH_SIZE; 
    return {kvs.front().key, sum};
}

/*将单个节点内的顶点映射为Key/value, 对Key排序后，再进行规约*/
void computing(int rank, gk_graph_t *graph, char *rb, int recvbuffersize) {
    iterationCompleted = 0;
    std::list<KV> kvs;
    Vertex vertex;

    /* 由子图中信息构造一个个顶点(vertex),然后交给map处理 */
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
    /* 由接收缓冲区数据构造一个个顶点(vertex), 再交给map处理 */
    for (int i = 0 ; i < recvbuffersize; ) {
        int vid, eid, location, eweight, edgenum = 0;
        float vweight;
        memcpy(&vid, rb + i, sizeof(int));
        memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
        memcpy(&vweight, rb + (i += sizeof(int)), sizeof(float));
        memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
        printf(" %d %d %f %d ", vid, location, vweight, edgenum);
        vertex.id = vid;
        vertex.loc = location;
        vertex.value = vweight;
        vertex.neighborSize = edgenum;
        i += sizeof(int);
        for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
            memcpy(&eid, rb + i, sizeof(int));
            printf(" %d", eid + 1);
            vertex.neighbors[j] = eid + 1;
        }
        for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
            memcpy(&eweight, rb + i, sizeof(int));
            printf(" %d", eweight);
        }
        printf("\n");
        map(vertex, kvs);
    }

    /* 对map产生的key/value list进行排序 */
    kvs.sort([](KV &kv1, KV &kv2){return kv1.key < kv2.key;});

    std::list<KV> sameKeylist;
    for (KV kv : kvs) {
        if(sameKeylist.size() > 0 && kv.key != sameKeylist.front().key) {
            KV tmpKV = reduce(sameKeylist);
//             /*将reduce的新值更新到子图中; TODO: 改为批量更新*/
//             auto iter = getVertexIter(sg.begin(), sg.neighbors(), tmpKV.key);
//             if (iter != sg.neighbors()) {
//                 /*和老值进行比较, 判断迭代是否结束*/
//                 if (iter->value - tmpKV.value > threshold) {
//                     iterationCompleted = 1;
//                 }
//                 iter->value = tmpKV.value;
//             }
            sameKeylist.clear();
        }
        sameKeylist.push_back(kv);
    }
    KV kv = reduce(sameKeylist);
//     /*将reduce的新值更新到子图中; TODO: 改为批量更新*/
//     auto iter = getVertexIter(sg.begin(), sg.neighbors(), kv.key);
//     if (iter != sg.neighbors()) {
//         if (iter->value - kv.value > threshold)
//             iterationCompleted = 1;
//         iter->value = kv.value;
//     }
}

int isCompleted(int rank) {
    return iterationCompleted;    
}

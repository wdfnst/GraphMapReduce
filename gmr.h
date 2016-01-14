#include "graph.h"

void map(Vertex &v, std::list<KV> &kvs){
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
    sum = 0.5 * sum + (1 - 0.5) / (sizeof(vs) / sizeof(Vertex) - 1); 
    return {kvs.front().key, sum};
}

/*将单个节点内的顶点映射为Key/value, 对Key排序后，再进行规约*/
void computing(int rank, SubGraph &sg) {
    iterationCompleted = 0;
    std::list<KV> kvs;
    for (auto iter = sg.begin(); iter != sg.end(); iter++) {
        map(*iter, kvs);
    }

    kvs.sort([](KV &kv1, KV &kv2){return kv1.key < kv2.key;});

    std::list<KV> sameKeylist;
    for (KV kv : kvs) {
        if(sameKeylist.size() > 0 && kv.key != sameKeylist.front().key) {
            KV tmpKV = reduce(sameKeylist);
            /*将reduce的新值更新到子图中; TODO: 改为批量更新*/
            auto iter = getVertexIter(sg.begin(), sg.neighbors(), tmpKV.key);
            if (iter != sg.neighbors()) {
                /*和老值进行比较, 判断迭代是否结束*/
                if (iter->value - tmpKV.value > threshold) {
                    iterationCompleted = 1;
                }
                iter->value = tmpKV.value;
            }
            sameKeylist.clear();
        }
        sameKeylist.push_back(kv);
    }
    KV kv = reduce(sameKeylist);
    /*将reduce的新值更新到子图中; TODO: 改为批量更新*/
    auto iter = getVertexIter(sg.begin(), sg.neighbors(), kv.key);
    if (iter != sg.neighbors()) {
        if (iter->value - kv.value > threshold)
            iterationCompleted = 1;
        iter->value = kv.value;
    }
}

int isCompleted(int rank) {
    return iterationCompleted;    
}

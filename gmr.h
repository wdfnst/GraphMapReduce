struct Vertex {
    int id; int loc; int neighbors[512]; float value;
    bool operator==(int key) {
        return this->id == key;
    }
};
struct KV {
    int key; float value;
    bool operator==(int key) {
        return this->key == key;
    }
};
struct SubGraph {
    std::list<Vertex> vs;
    int innerOffset;
    int borderOffset;
    int neighborOffset;
    std::list<Vertex>::iterator begin() {
        return vs.begin();
    }
    std::list<Vertex>::iterator inners() {
        return vs.begin();
    }
    std::list<Vertex>::iterator borders() {
        auto iter = vs.begin();
        advance(iter, innerOffset);
        return iter;
    }
    std::list<Vertex>::iterator neighbors() {
        auto iter = vs.begin();
        advance(iter, borderOffset);
        return iter;
    }
    std::list<Vertex>::iterator end() {
        return vs.end();
    }
};
void define_new_type(MPI_Datatype* ctype)
{
    int blockcounts[4];
    MPI_Datatype oldtypes[4];
    MPI_Aint offsets[4];

    blockcounts[0]=1;
    blockcounts[1]=1;
    blockcounts[2]=512;
    blockcounts[3]=1;

    offsets[0]=0;
    offsets[1]=sizeof(int);
    offsets[2]=sizeof(int) + sizeof(int);
    offsets[3]=sizeof(int) + sizeof(int) + 512 * sizeof(int);

    oldtypes[0]=MPI_INT;
    oldtypes[1]=MPI_INT;
    oldtypes[2]=MPI_INT;
    oldtypes[3]=MPI_FLOAT;

    MPI_Type_struct(4,blockcounts,offsets,oldtypes,ctype);
    MPI_Type_commit(ctype);
}


/*迭代结束标志:0表示还有元素没有达到迭代结束条件, 默认表示都达到迭代结束条件*/
int iterationCompleted = 1;
float threshold = 0.0001;
Vertex vs[12] = {{}, {1, 0, {2, 3, 4, 5, 10}, 1.0}, {2, 1, {1, 3, 6, 7}, 1.0}, 
                     {3, 2, {1, 2, 8, 9}, 1.0},
                     {4, 0, {1}, 1.0}, {5, 0, {1}, 1.0}, {6, 1, {2}, 1.0}, {7, 1, {2}, 1.0},
                     {8, 2, {3}, 1.0}, {9, 2, {3}, 1.0}, {10, 2, {1}, 1.0}};
SubGraph subgraphs[3];

void partitionGraph() {
    SubGraph sg0, sg1, sg2;
    sg0.vs.push_back(vs[4]); sg0.vs.push_back(vs[5]);
    sg0.vs.push_back(vs[1]);
    sg0.vs.push_back(vs[2]); sg0.vs.push_back(vs[3]); sg0.vs.push_back(vs[10]);
    sg0.innerOffset = 2; sg0.borderOffset = 3; sg0.neighborOffset = 7;

    sg1.vs.push_back(vs[6]); sg1.vs.push_back(vs[7]);
    sg1.vs.push_back(vs[2]);
    sg1.vs.push_back(vs[1]); sg1.vs.push_back(vs[3]);
    sg1.innerOffset = 2; sg1.borderOffset = 3; sg1.neighborOffset = 5;

    sg2.vs.push_back(vs[8]); sg2.vs.push_back(vs[9]);
    sg2.vs.push_back(vs[3]); sg2.vs.push_back(vs[10]);
    sg2.vs.push_back(vs[1]); sg2.vs.push_back(vs[2]);
    sg2.innerOffset = 2; sg2.borderOffset = 4; sg2.neighborOffset = 6;

    subgraphs[0] = sg0;
    subgraphs[1] = sg1;
    subgraphs[2] = sg2;
}

void partitionGraph(int partitions) { }

void displaySubgraphs() {
    for (int i = 0; i < sizeof(subgraphs) / sizeof(SubGraph); i++)
        std::cout << "subgraph[" << i << "] info:" << subgraphs[i].innerOffset << " inns\t" << 
            subgraphs[i].borderOffset - subgraphs[i].innerOffset << " bors\t" << 
            subgraphs[i].neighborOffset - subgraphs[i].borderOffset << " neighbors\n";
}

void displayGraph(int iterNum, SubGraph &sg) {
    std::cout << iterNum << "th:";
    for (auto iter = sg.begin(); iter != sg.neighbors(); iter++)
        printf("%d(%6.4f) ", iter->id, iter->value);
    std::cout << std::endl;
}

std::list<Vertex>::iterator getVertexIter(const std::list<Vertex>::iterator &begin, 
        const std::list<Vertex>::iterator &end, int vertexId) {
    return find(begin, end, vertexId);
}

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
    iterationCompleted = 1;
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
                    iterationCompleted = 0;
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
            iterationCompleted = 0;
        iter->value = kv.value;
    }
}

int isCompleted(int rank) {
    return iterationCompleted;    
}

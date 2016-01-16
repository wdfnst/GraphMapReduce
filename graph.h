struct Vertex {
    int id; int loc; int neighbors[512]; float value;
    bool operator==(int key) {
        return this->id == key;
    }
    bool operator<(int key) {
        return id < key;
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

Vertex vs[12] = {{}, {1, 0, {2, 3, 4, 5, 10}, 1.0}, {2, 1, {1, 3, 6, 7}, 1.0}, 
                     {3, 2, {1, 2, 8, 9}, 1.0},
                     {4, 0, {1}, 1.0}, {5, 0, {1}, 1.0}, {6, 1, {2}, 1.0}, {7, 1, {2}, 1.0},
                     {8, 2, {3}, 1.0}, {9, 2, {3, 10}, 1.0}, {10, 2, {1, 9}, 1.0}};
SubGraph subgraphs[3];

void partitionGraph() {
    SubGraph sg0, sg1, sg2;
    sg0.vs.push_back(vs[4]); sg0.vs.push_back(vs[5]);
    sg0.vs.push_back(vs[1]);
    sg0.vs.push_back(vs[2]); sg0.vs.push_back(vs[3]); sg0.vs.push_back(vs[10]);
    sg0.innerOffset = 2; sg0.borderOffset = 3; sg0.neighborOffset = 6;

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

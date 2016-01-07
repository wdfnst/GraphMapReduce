struct Vertex {
    int id; int loc; int neighbors[512]; float value;
};
struct SubGraph {
    std::vector<Vertex> inners;
    std::vector<Vertex> borders;
    std::vector<Vertex> neighbors;
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

Vertex vs[12] = {{}, {1, 0, {2, 3, 4, 5}, 1.1}, {2, 1, {1, 3, 6, 7}, 2.2}, 
                     {3, 2, {1, 2, 8, 9}, 3.3},
                     {4, 0, {1}, 1.1}, {5, 0, {1}, 2.2}, {6, 1, {2}, 3.3}, {7, 1, {2}, 1.1},
                     {8, 2, {3}, 2.2}, {9, 2, {3}, 3.3}, {10, 2, {1}, 10.1}, 
                     {11, 1, {1}, 11.8}};
static SubGraph subgraphs[3];

void partitionGraph() {
    SubGraph sg0, sg1, sg2;
    std::vector<Vertex> inners, borders, neighbors;
    inners.push_back(vs[4]); inners.push_back(vs[5]);
    borders.push_back(vs[1]);
    neighbors.push_back(vs[2]); neighbors.push_back(vs[3]); neighbors.push_back(vs[10]);
    neighbors.push_back(vs[11]);
    sg0 = {inners, borders, neighbors};

    inners.clear(); borders.clear(); neighbors.clear();
    inners.push_back(vs[6]); inners.push_back(vs[7]);
    borders.push_back(vs[2]); borders.push_back(vs[11]);
    neighbors.push_back(vs[1]); neighbors.push_back(vs[3]);
    sg1 = {inners, borders, neighbors};

    inners.clear(); borders.clear(); neighbors.clear();
    inners.push_back(vs[8]); inners.push_back(vs[9]);
    borders.push_back(vs[3]); borders.push_back(vs[10]);
    neighbors.push_back(vs[2]); neighbors.push_back(vs[1]);
    sg2 = {inners, borders, neighbors};

    subgraphs[0] = sg0;
    subgraphs[1] = sg1;
    subgraphs[2] = sg2;
}

void displaySubgraphs() {
    for (int i = 0; i < sizeof(subgraphs); i++)
        std::cout << "subgraph[" << i << "] info:" << subgraphs[i].inners.size() << "\t" << 
            subgraphs[i].borders.size() << "\t" << subgraphs[i].neighbors.size() << std::endl;
}

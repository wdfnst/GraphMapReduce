#include <iostream>
#include <vector>
#include <list>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <stdlib.h> 
#include <stdio.h> 
#include <string.h> 
#include <errno.h> 
#include "mpi.h"
#include "gmr.h"

using namespace std;

const int process = 3;

int main(int argc, char *argv[]) {
    int rank, size, i, j, iterNum = 0;
    Vertex *sb,*rb;
    MPI_Datatype Vertex_Type;

    MPI_Init(&argc,&argv);
    define_new_type(&Vertex_Type);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    partitionGraph();
    if (rank == 0)
        displaySubgraphs();

    int endflag = 1;
    while(endflag){
        int sendcounts[process] = {0}, recvcounts[process] = {0}; 
        int sdispls[process] = {0}, rdispls[process] = {0};
        vector<vector<Vertex>> sendvectors(size);
        for (auto v = subgraphs[rank].borders(); v != subgraphs[rank].neighbors(); v++) {
            /*sendedflag用于判断当前遍历的边界节点是否已经放到发送给w.loc号进程*/
            bitset<process> sendedflag;
            for (auto w = subgraphs[rank].neighbors(); w != subgraphs[rank].end(); w++) {
                /*如果当前边界顶点已经放入了发送到目标进程的队列，则跳过*/
                if (sendedflag.test(w->loc)) continue;
                /*如果当前邻居与当前边界顶点相连,则将此边界顶点发送给当前邻居所在的进程*/
                if (find(v->neighbors, v->neighbors + sizeof(v->neighbors) / sizeof(int), 
                            w->id) != v->neighbors + sizeof(v->neighbors) / sizeof(int)) {
                    sendvectors[w->loc].push_back(*v);
                    sendcounts[w->loc]++;
                    sendedflag.set(w->loc);
                }
            }
        }

        /*根据子图的邻居,申请发送缓存和接收缓存, 并计算发送接收缓冲区偏移*/
        for (auto v = subgraphs[rank].neighbors(); v != subgraphs[rank].end(); v++) {
            recvcounts[v->loc]++;
        }
        rb = new Vertex[accumulate(recvcounts, recvcounts + size, 0)];    
        if ( !rb ) {
            perror( "can't allocate recv buffer");
            free(sb); MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        sb = new Vertex[accumulate(sendcounts, sendcounts + size, 0)];    
        if ( !sb ) {
            perror( "can't allocate send buffer" );
            MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE); 
        }
        for (int i = 1; i != size; i++) {
            sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /*将发送缓冲向量中的数据复制到发送缓存*/
        int sendbuffersize = accumulate(sendcounts, sendcounts + size, 0);
        int index = 0;
        for (int i = 0; i < size; i++) {
            for (int j = 0; j < sendvectors[i].size() && index < sendbuffersize; j++, index++) { 
                sb[index] = sendvectors[i][j];
                //printf("myid=%d,send to id=%d, data[%d]=%d\n",rank,i,j,sb[index].id);
            }
        }

        /* 执行MPI_Alltoall 调用*/
        MPI_Alltoallv(sb,sendcounts,sdispls,Vertex_Type,rb,recvcounts,rdispls,Vertex_Type, 
                MPI_COMM_WORLD);

        /*从其他子图传过来的子图,应该更新到本子图上,然后计算本子图信息*/
        /*处理从别的节点传过来的数据(邻居节点), 并更新本地数据*/
        //cout << "Process " << rank << " recv: ";
        for ( i=0 ; i < size ; i++ ) {
            //cout << "(P" << i << ")->";
            for (j = 0; j < recvcounts[i]; j++) {
                //cout << rb[rdispls[i] + j].id << "(" << rb[rdispls[i] + j].value << "), ";
                /*使用从别的节点传递过来顶点信息更新本子图的neighbors节点*/
                auto iter = getVertexIter(subgraphs[rank].neighbors(), subgraphs[rank].end(), 
                        rb[rdispls[i] + j].id);
                if (iter != subgraphs[rank].end())
                    iter->value = rb[rdispls[i] + j].value;
            }
            //cout << ";\t";
        }
        //cout << "\n";

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        computing(rank, subgraphs[rank]);
        displayGraph(iterNum++, subgraphs[rank]);
        free(sb); free(rb);
        MPI_Barrier(MPI_COMM_WORLD);

        /*判断迭代是否结束*/
        int iterationCompleted = isCompleted(rank);
        int *rbuf = (int *)malloc(size * sizeof(int));
        MPI_Allgather(&iterationCompleted, 1, MPI_INT, rbuf, 1, MPI_INT, MPI_COMM_WORLD);
        endflag = accumulate(rbuf, rbuf + size, 0);
        cout << "=============endflag:" << endflag << endl;
    }
    MPI_Finalize();
}

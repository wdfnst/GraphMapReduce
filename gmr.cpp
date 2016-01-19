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
#include <GKlib.h>
#include "mpi.h"
#include "graph.h"
#include "gmr.h"

using namespace std;

int main(int argc, char *argv[]) {
    int rank, size, i, j, iterNum = 0;
    char *sb = nullptr, *rb = nullptr;
    char subgraphfilename[256];
    gk_graph_t *graph;

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    /* 读入子图subgraph-rank */
    sprintf(subgraphfilename, "graph/small.graph.subgraph.%d", rank);
    graph = gk_graph_Read(subgraphfilename, GK_GRAPH_FMT_METIS, 0, 1, 0);
    printf("%d 节点和边数: %d %zd\n", rank, graph->nvtxs, graph->xadj[graph->nvtxs]);

    int hasvwgts, hasvsizes, hasewgts;
    hasewgts  = (graph->iadjwgt || graph->fadjwgt);
    hasvwgts  = (graph->ivwgts || graph->fvwgts);
    hasvsizes = (graph->ivsizes || graph->fvsizes);

    // displayGraph(graph);

    int endflag = 1;
    while(endflag){
        /* 定义用于缓存向各节点发送数据数量和偏移的数组 */
        int *sendcounts = getSendBufferSize(graph, rank);
        int *recvcounts = (int*)malloc(size * sizeof(int));
        int *sdispls = (int*)malloc(size * sizeof(int));
        int *rdispls = (int*)malloc(size * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        cout << rank << " send size:\n";
        for (int i = 0; i < size; i++) {
            cout << sendcounts[i] << "\t";
        }
        cout << endl;
        
        /* 进行两轮alltoall(alltoallv). 第一次同步需要交换数据的空间大小, 第二次交换数据 */
        MPI_Alltoall(sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT, 
                MPI_COMM_WORLD);

        cout << rank << " recv size:\n";
        for (int i = 0; i < size; i++) {
            cout << recvcounts[i] << "\t";
        }
        cout << endl;

        /* 申请发送和接收数据的空间 */
        rb = (char*)malloc(accumulate(recvcounts, recvcounts + size, 0));
        if ( !rb ) {
            perror( "can't allocate recv buffer");
            free(sb); MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        sb = (char*)malloc(accumulate(sendcounts, sendcounts + size, 0));
        if ( !sb ) {
            perror( "can't allocate send buffer" );
            MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE); 
        }
        for (int i = 1; i != size; i++) {
            sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
        sb = getSendbuffer(graph, sendcounts, sdispls, size, rank);

        /* 执行MPI_Alltoall 调用*/
        MPI_Alltoallv(sb, sendcounts, sdispls, MPI_CHAR, rb, recvcounts, rdispls, MPI_CHAR, 
                MPI_COMM_WORLD);

        /*从其他子图传过来的子图,应该更新到本子图上,然后计算本子图信息*/
        /*处理从别的节点传过来的数据(邻居节点), 并更新本地数据*/
        int rbsize = accumulate(recvcounts, recvcounts + size, 0);
        cout << "Process " << rank << " recv: ";
        for ( i = 0 ; i < rbsize; ) {
            int vid, eid, location, eweight, edgenum = 0;
            float vweight;
            memcpy(&vid, rb + i, sizeof(int));
            memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
            memcpy(&vweight, rb + (i += sizeof(int)), sizeof(float));
            memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
            printf(" %d %d %f %d ", vid, location, vweight, edgenum);
            i += sizeof(int);
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eid, rb + i, sizeof(int));
                printf(" %d", eid + 1);
            }
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eweight, rb + i, sizeof(int));
                printf(" %d", eweight);
            }
            printf(" %d / %d\n", i, rbsize);
        }
        printf("\n");

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        computing(rank, graph, rb, rbsize); 
        free(sb); free(rb);
        MPI_Barrier(MPI_COMM_WORLD);
        break;

//         /*判断迭代是否结束*/
//         int iterationCompleted = isCompleted(rank);
//         int *rbuf = (int *)malloc(size * sizeof(int));
//         MPI_Allgather(&iterationCompleted, 1, MPI_INT, rbuf, 1, MPI_INT, MPI_COMM_WORLD);
//         endflag = accumulate(rbuf, rbuf + size, 0);
//         free(rbuf);
        iterNum++;
    }
    MPI_Finalize();
    gk_graph_Free(&graph);
}

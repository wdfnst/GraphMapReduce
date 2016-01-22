#include <iostream>
#include <vector>
#include <list>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <stdlib.h> 
#include <math.h>
#include <float.h>
#include <stdio.h> 
#include <string.h> 
#include <errno.h> 
#include <GKlib.h>
#include "mpi.h"
#include "graph.h"
#include "gmr.h"

using namespace std;

int main(int argc, char *argv[]) {
    int rank, size, i;
    char *sb = nullptr, *rb = nullptr;
    char subgraphfilename[256];
    gk_graph_t *graph;
    double starttime = MPI_Wtime();

    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    /* 读入子图subgraph-rank */
    sprintf(subgraphfilename, "graph/%s.graph.subgraph.%d", graphs[testgraph].name, rank);
    graph = gk_graph_Read(subgraphfilename, GK_GRAPH_FMT_METIS, 0, 1, 0);
    if(INFO) printf("%d 节点和边数: %d %zd\n", rank, graph->nvtxs, graph->xadj[graph->nvtxs]);
    if (DEBUG) displayGraph(graph);

    int endflag = 1;
    while(endflag){
        /* 定义用于缓存向各节点发送数据数量和偏移的数组 */
        int *sendcounts = getSendBufferSize(graph, size, rank);
        int *recvcounts = (int*)malloc(size * sizeof(int));
        int *sdispls = (int*)malloc(size * sizeof(int));
        int *rdispls = (int*)malloc(size * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        if(INFO) printf("Process %d send size:", rank);
        for (int i = 0; i < size; i++) {
            if(INFO) printf(" %d\t", sendcounts[i]);
        }
        if(INFO) printf("\n");
        
        /* 进行两轮alltoall(alltoallv). 第一次同步需要交换数据的空间大小, 第二次交换数据 */
        recordTick("bexchangecounts");
        MPI_Alltoall(sendcounts, 1, MPI_INT, recvcounts, 1, MPI_INT, 
                MPI_COMM_WORLD);
        recordTick("eexchangecounts");

        if(INFO) printf("Prcess %d recv size:", rank);
        for (int i = 0; i < size; i++) {
            if(INFO) printf(" %d\t", recvcounts[i]);
        }
        if(INFO) printf("\n");

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
        recordTick("bgetsendbuffer");
        sb = getSendbuffer(graph, sendcounts, sdispls, size, rank);
        recordTick("egetsendbuffer");

        /* 执行MPI_Alltoall 调用*/
        recordTick("bexchangedata");
        MPI_Alltoallv(sb, sendcounts, sdispls, MPI_CHAR, rb, recvcounts, rdispls, MPI_CHAR, 
                MPI_COMM_WORLD);
        recordTick("eexchangedata");

        /*从其他子图传过来的子图,应该更新到本子图上,然后计算本子图信息*/
        /*处理从别的节点传过来的数据(邻居节点), 并更新本地数据*/
        int rbsize = accumulate(recvcounts, recvcounts + size, 0);
        if (DEBUG) printf("Process %d recv:", rank);
        for ( i = 0 ; i < rbsize; ) {
            int vid, eid, location, eweight, edgenum = 0;
            float vweight;
            memcpy(&vid, rb + i, sizeof(int));
            memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
            memcpy(&vweight, rb + (i += sizeof(int)), sizeof(float));
            memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
            if(DEBUG) printf(" %d %d %f %d ", vid, location, vweight, edgenum);
            i += sizeof(int);
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eid, rb + i, sizeof(int));
                if(DEBUG) printf(" %d", eid + 1);
            }
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eweight, rb + i, sizeof(int));
                if(DEBUG) printf(" %d", eweight);
            }
            if(DEBUG) printf(" %d / %d\n", i, rbsize);
        }
        if(DEBUG) printf("\n");

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        recordTick("bcomputing");
        computing(rank, graph, rb, rbsize); 
        recordTick("ecomputing");
        free(sb); free(rb);
        MPI_Barrier(MPI_COMM_WORLD);

        /* 相互收集其他节点是否迭代结束 */
        int iterationCompleted = isCompleted(rank);
        int *rbuf = (int *)malloc(size * sizeof(int));
        recordTick("bexchangeiterfinish");
        MPI_Allgather(&iterationCompleted, 1, MPI_INT, rbuf, 1, MPI_INT, MPI_COMM_WORLD);
        recordTick("eexchangeiterfinish");
        endflag = accumulate(rbuf, rbuf + size, 0);
        free(rbuf);
        iterNum++;
        if(INFO) printf("迭代次数:%d ~ 迭代未结束节点:%d\n", iterNum, endflag);
        printTimeConsume();
    }
    MPI_Finalize();
    gk_graph_Free(&graph);
    printf("程序运行结束,总共耗时:%f, 最大消耗内存:(未统计)\n", MPI_Wtime() - starttime);
}

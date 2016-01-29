#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <bitset>
#include <iterator>
#include <algorithm>
#include <numeric>
#include <stdlib.h> 
#include <stdarg.h>
#include <math.h>
#include <float.h>
#include <stdio.h> 
#include <string.h> 
#include <signal.h>
#include <unistd.h>
#include <errno.h> 
#include "mpi.h"
#include "error.h"
#include "graph.h"
#include "gmr.h"
#include "algorithms.h"

using namespace std;

int main(int argc, char *argv[]) {
    /* rank, size: MPI进程序号和进程数, sb: 发送缓存, rb: 接收缓存 */
    int rank, size, i;
    char *sb = nullptr, *rb = nullptr;
    double starttime = MPI_Wtime();

    /* 子图文件和用到的图算法实现类 */
    char subgraphfilename[256];
    graph_t *graph;
    GMR *pagerank_gmr = new PageRank();

    /* 初始化MPI */
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    /* allothersendcounts: 
     * 接收所有进程发送数据的大小, 用于判断迭代结束(若所有进程都无数据需要发送)
     * sendcounts, recvcounts: 数据发送和接收缓冲区
     * sdispls, rdispls: 发送和接收数据缓冲区的偏移 */
    int *allothersendcounts = (int*)malloc(size * size * sizeof(int));
    int *sendcounts         = NULL;
    int *recvcounts         = (int*)malloc(size * sizeof(int));
    int *sdispls            = (int*)malloc(size * sizeof(int));
    int *rdispls            = (int*)malloc(size * sizeof(int));

    /* 根据进程号, 拼接子图文件名, 并读取子图到结构体graph中 */
    sprintf(subgraphfilename, "graph/small.graph.subgraph.%d", rank);
    graph = graph_Read(subgraphfilename, GK_GRAPH_FMT_METIS, 1, 1, 0);
    ntxs = graph->nvtxs;
    if(INFO) printf("%d 节点和边数: %d %zd\n", rank, graph->nvtxs, graph->xadj[graph->nvtxs]);
    if (DEBUG) displayGraph(graph);

    while(true && iterNum < MAX_ITERATION){
        sendcounts = getSendBufferSize(graph, size, rank);
        memset(allothersendcounts, 0, size * size * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        /* 打印出节点发送缓冲区大小 */
        if(INFO) printf("Process %d send size:", rank);
        for (int i = 0; i < size; i++) {
            if(INFO) printf(" %d\t", sendcounts[i]);
        }
        if(INFO) printf("\n");
        
        /* 如果每个节点发向其他所有节点的数据大小都为0, 则迭代结束 */
        recordTick("bexchangecounts");
        MPI_Allgather(sendcounts, size, MPI_INT, allothersendcounts, size,
                MPI_INT, MPI_COMM_WORLD); 
        if (accumulate(allothersendcounts, allothersendcounts + size, 0) == 0) break;
        /* 将本节点接收缓存区大小拷贝到recvcounts中 */
        for (int i = 0; i < size; i++)
            recvcounts[i] = allothersendcounts[rank + i * size];
        recordTick("eexchangecounts");

        /* 打印出节点接收到的接收缓冲区大小 */
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
        /* 计算发送和接收缓冲区偏移 */
        for (int i = 1; i != size; i++) {
            sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
        recordTick("bgetsendbuffer");
        sb = getSendbuffer(graph, sendcounts, sdispls, size, rank);
        recordTick("egetsendbuffer");

        /* 调用MPIA_Alltoallv(), 交换数据 */
        recordTick("bexchangedata");
        MPI_Alltoallv(sb, sendcounts, sdispls, MPI_CHAR, rb, recvcounts, rdispls, MPI_CHAR, 
                MPI_COMM_WORLD);
        recordTick("eexchangedata");

        /*从其他子图传过来的子图,应该更新到本子图上,然后计算本子图信息*/
        /*处理从别的节点传过来的数据(邻居节点), 并更新本地数据*/
        int rbsize = accumulate(recvcounts, recvcounts + size, 0);
        totalRecvBytes += rbsize;
        if (DEBUG) printf("Process %d recv:", rank);
        for ( i = 0 ; i < rbsize; ) {
            int vid, eid, location, eloc, edgenum = 0;
            float fewgt, fvwgt;
            memcpy(&vid, rb + i, sizeof(int));
            memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
            memcpy(&fvwgt, rb + (i += sizeof(int)), sizeof(float));
            memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
            if(DEBUG) printf(" %d %d %f %d ", vid, location, fvwgt, edgenum);
            i += sizeof(int);
            /* 读取边的另外一个顶点 */
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eid, rb + i, sizeof(int));
                if(DEBUG) printf(" %d", eid + 1);
            }
            /* 读取边的另外一个顶点所在的节点 */
            for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                memcpy(&eloc, rb + i, sizeof(int));
                if(DEBUG) printf(" %d", eloc);
            }
            /* 读取边的权重 */
            for (int j = 0; j < edgenum; j++, i += sizeof(float)) {
                memcpy(&fewgt, rb + i, sizeof(float));
                if(DEBUG) printf(" %f", fewgt);
            }
            if(DEBUG) printf(" %d / %d\n", i, rbsize);
        }
        if(DEBUG) printf("\n");

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        recordTick("bcomputing");
        computing(rank, graph, rb, rbsize, pagerank_gmr); 
        recordTick("ecomputing");
        free(sb); free(rb);
        MPI_Barrier(MPI_COMM_WORLD);
        recordTick("eiteration");

        /* 释放内存, 并打印迭代信息 */
        free(sendcounts);
        iterNum++;
        printTimeConsume();
    }
    MPI_Finalize();
    graph_Free(&graph);
    if (sdispls) free(sdispls); if(rdispls) free(rdispls); // if(sendcounts) free(sendcounts); 
    if (recvcounts) free(recvcounts); if(allothersendcounts) free(allothersendcounts);
    printf("程序运行结束,总共耗时:%f secs, 通信量:%ld Byte, 最大消耗内存:(未统计)Byte\n", 
            MPI_Wtime() - starttime, totalRecvBytes);
}

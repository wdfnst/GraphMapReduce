#include <iostream>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include <list>
#include <map>
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
#include <limits.h>
#include <errno.h> 
#include "mpi.h"
#include "error.h"
#include "graph.h"
#include "gmr.h"
#include "algorithms.h"
#include "partition.h"

using namespace std;

/** 运行示例:
 * 1.)单机运行: mpirun -np pro_num ./gmr algorithm partition graphfile
 * 2.)集群运行: mpirun -mahcinefile hosts ./gmr algorithm partition graphfile
 */
int main(int argc, char *argv[]) {
    /* rank, size: MPI进程序号和进程数, sb: 发送缓存, rb: 接收缓存 */
    int rank, size, i;
    char *sb = nullptr, *rb = nullptr;
    graph_t *graph;
    GMR *gmr = nullptr;

    /* 初始化MPI */
    MPI_Init(&argc,&argv);
    MPI_Comm_rank(MPI_COMM_WORLD,&rank);
    MPI_Comm_size(MPI_COMM_WORLD,&size);

    double starttime = MPI_Wtime();
    /* allothersendcounts: 
     * 接收所有进程发送数据的大小, 用于判断迭代结束(若所有进程都无数据需要发送)
     * sendcountswithconv:带收敛进度的发送缓存区大小(image: sendcounts : conv) 
     * sendcounts, recvcounts: 数据发送和接收缓冲区
     * sdispls, rdispls: 发送和接收数据缓冲区的偏移 */
    int *allothersendcounts = (int*)malloc((size + 1) * size * sizeof(int));
    int *sendcountswithconv = (int*)malloc((size + 1) * sizeof(int));
    int *sendcounts         = NULL;
    int *recvcounts         = (int*)malloc(size * sizeof(int));
    int *sdispls            = (int*)malloc(size * sizeof(int));
    int *rdispls            = (int*)malloc(size * sizeof(int));

    /* 如果没有提供任何命令行参数, 则采用默认算法和分图 */
    if (argc == 1 && rank == 0) {
            printf("===>示例本地运行(采用rdsmall.graph图文件和随机分图算法)<===\n");
    }
    /* 参数提供的顺序: mpirun -np 3 gmr algorithm partition graphfile */
    /* 参数个数大于4时, 显示程序的调用格式 */
    if (argc > 3) {
        printf("Usage:1.)mpirun -np 3 ./gmr graphfile random\n"
                 "2.)mpirun -machinefile hosts -np 3 ./gmr graphfile metis\n");
        exit(0);
    }
    
    if (argc > 2)
        graph = graph_Read(argv[2], rank, size);
    else
        graph = graph_Read("graph/4elt.graph", rank, size);

    /* 根据调用命令行实例化相应算法, 没用提供则模式使用TriangeCount算法 */
    /* 如果有提供算法名字, 则使用规定的算法 */
    if (argc > 1) {
        if (strcmp(argv[1], "pagerank") == 0)
            gmr = new PageRank();
        else if (strcmp(argv[1], "trianglecount") == 0)
            gmr = new TriangleCount();
        else if (strcmp(argv[1], "sssp") == 0)
            gmr = new SSSP(1);
        else {
            printf("目前没有提供%s的图算法方法.\n", argv[1]);
            exit(0);
        }
    }
    else
        gmr = new TriangleCount();

    /* 将子图的顶点个数赋给进程的全局变量 */
    ntxs = graph->nvtxs;
    if(INFO) printf("%d 节点和边数: %d %zd\n", rank, graph->nvtxs,
            graph->xadj[graph->nvtxs]);
    if (DEBUG) displayGraph(graph);

    /* 首先使用算法对图进行初始化:
     * PageRank: 初始化为空
     * SSSP: 如果调用的是SSSP算法, 需要将图进行初始化
     * startv.value = 0, otherv = ∞ */
    gmr->initGraph(graph);

    /* 获取发送数据的大小, 并将其放到发送缓冲区 */
    /* 从当前子图获取需要向其他节点发送的字节数 */
    sendcounts = getSendBufferSize(graph, size, rank);
    memset(sdispls, 0, size * sizeof(int));
    /* 计算发送和接收缓冲区偏移 */
    for (i = 1; i != size; i++) {
        sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
    }
    /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
    recordTick("bgetsendbuffer");
    sb = getSendbuffer(graph, sendcounts, sdispls, size, rank);
    recordTick("egetsendbuffer");

    while(true && iterNum < MAX_ITERATION && iterNum < gmr->algoIterNum){
        memset(allothersendcounts, 0, (size + 1) * size * sizeof(int));
        memset(sendcountswithconv, 0, (size + 1) * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        //memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        /* 打印出节点发送缓冲区大小 */
        if(INFO) {
            printf("Process %d send size:", rank);
            for (i = 0; i < size; i++) {
                printf(" %d\t", sendcounts[i]);
            }
            printf("\n");
        }
        
        /* 计算迭代进度(收敛顶点数/总的顶点数 * 10,000), 并将其加在缓存大小后面
         * ,接收数据后,首先判断所有进程的收敛进度是否接收,再拷贝接收缓存大小 */
        recordTick("bexchangecounts");
        /* 将收敛精度乘以10000, 实际上是以精确到小数点后两位以整数形式发送 */
        int convergence = (int)(1.0 * convergentVertex / graph->nvtxs * 10000);
        memcpy(sendcountswithconv, sendcounts, size * sizeof(int));
        memcpy(sendcountswithconv + size, &convergence, sizeof(int));
        /* 交换需要发送字节数和收敛的进度 */
        MPI_Allgather(sendcountswithconv, size + 1, MPI_INT, allothersendcounts,
                size + 1, MPI_INT, MPI_COMM_WORLD); 
        i = size;
        while(allothersendcounts[i] == 10000 && i < (size + 1) * size) i += (size + 1);
        if (i > (size + 1) * size - 1) break;

        /* 将本节点接收缓存区大小拷贝到recvcounts中 */
        for (i = 0; i < size; i++)
            recvcounts[i] = allothersendcounts[rank + i * (size + 1)];
        recordTick("eexchangecounts");

        /* 打印出节点接收到的接收缓冲区大小 */
        if(INFO) {
            printf("Prcess %d recv size:", rank);
            for (i = 0; i < size; i++) {
                printf(" %d\t", recvcounts[i]);
            }
            printf("\n");
        }

        /* 申请发送和接收数据的空间 */
        rb = (char*)malloc(accumulate(recvcounts, recvcounts + size, 0));
        if ( !rb ) {
            perror( "can't allocate recv buffer");
            free(sb); MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        /* 计算发送和接收缓冲区偏移 */
        for (i = 1; i != size; i++) {
            //sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
        //recordTick("bgetsendbuffer");
        //sb = getSendbuffer(graph, sendcounts, sdispls, size, rank);
        //recordTick("egetsendbuffer");

        /* 调用MPIA_Alltoallv(), 交换数据 */
        recordTick("bexchangedata");
        MPI_Alltoallv(sb, sendcounts, sdispls, MPI_CHAR, rb, recvcounts,
                rdispls, MPI_CHAR, MPI_COMM_WORLD);
        recordTick("eexchangedata");

        /* 打印输出接收到的图顶点信息 */
        int rbsize = accumulate(recvcounts, recvcounts + size, 0);
        totalRecvBytes += rbsize;

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        recordTick("bcomputing");
        computing(rank, graph, rb, rbsize, gmr); 
        recordTick("ecomputing");
        free(rb);
        MPI_Barrier(MPI_COMM_WORLD);
        recordTick("eiteration");

        iterNum++;
        printTimeConsume(rank);
    }
    printf("程序运行结束,总共耗时:%f secs, 通信量:%ld Byte, 最大消耗"
            "内存:(未统计)Byte\n", MPI_Wtime() - starttime, totalRecvBytes);

    MPI_Finalize();
    /* 打印处理完之后的结果(图) */
    //displayGraph(graph);
    gmr->printResult(graph);
    graph_Free(&graph);
    free(sendcounts);
    free(sb); 
    if (sdispls) free(sdispls); if(rdispls) free(rdispls);
    if (recvcounts) free(recvcounts);
    if(allothersendcounts) free(allothersendcounts);
    if(sendcountswithconv) free(sendcountswithconv);
}

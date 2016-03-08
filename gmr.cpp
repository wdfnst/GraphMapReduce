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
#include "partition.h"
#include "graph.h"
#include "gmr.h"
#include "algorithms.h"

using namespace std;

/** 运行示例:
 * 1.)单机运行: mpirun -np pro_num ./gmr algorithm partition graphfile
 * 2.)集群运行: mpirun -mahcinefile hosts ./gmr algorithm partition graphfile
 */
int main(int argc, char *argv[]) {
    /* rank, size: MPI进程序号和进程数, sb: 发送缓存, rb: 接收缓存 */
    int rank, size, i;
    char *sb = nullptr, *rb = nullptr;
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

    /**********************Variables for Zoltan***********************/
    int numGlobalVertices, rc;
    float ver;
    struct Zoltan_Struct *zz;
    int changes, numGidEntries, numLidEntries, numImport, numExport;
    ZOLTAN_ID_PTR importGlobalGids, importLocalGids, exportGlobalGids, exportLocalGids;
    int *importProcs, *importToPart, *exportProcs, *exportToPart;
    int *parts = NULL;
    FILE *fp;
    GRAPH_DATA myGraph;
    char *fname = "graph/rdsmall.graph";
    /******************************************************************/

    /* 如果没有提供任何命令行参数, 则采用默认算法和分图 */
    if (argc == 1 && rank == 0) {
        printf("===>示例本地运行(采用rdsmall.graph图文件和随机分图算法)<===\n");
    }
    /* 参数提供的顺序: mpirun -np 3 gmr algorithm partition graphfile */
    /* 参数个数大于4时, 显示程序的调用格式 */
    if (argc > 4) {
        printf("Usage:1.)mpirun -np 3 ./gmr graphfile random\n"
               "2.)mpirun -machinefile hosts -np 3 ./gmr graphfile metis");
        exit(0);
    }
    
    /* 根据调用命令行打开相应的图文件并采用提供的图切分算法进行切图, 
     * 否则使用默认方式: rdmdual.graph图和随机切分算法 */
    if (argc > 3) 
        fname = argv[3];

    rc = Zoltan_Initialize(argc, argv, &ver);
    if (rc != ZOLTAN_OK){
        printf("sorry...\n");
        MPI_Finalize();
        exit(0);
    }
    fp = fopen(fname, "r");
    if (!fp){
        if (rank == 0) fprintf(stderr,"ERROR: Can not open %s\n", fname);
        MPI_Finalize();
        exit(1);
    }
    fclose(fp);
    numGlobalVertices = read_input_file(rank, size, fname, &myGraph);
    /* 因为只有root(0)进程知道文件中记录的顶点数, 所以需要进行广播 */
    MPI_Bcast( &numGlobalVertices, 1, MPI_INT, 0, MPI_COMM_WORLD );
    
    /* 根据调用命令提供的参数, 读取不同的图文件或子图文件 */
    if (argc > 2 && strcmp(argv[2], "zoltan") == 0) {
        zz = Zoltan_Create(MPI_COMM_WORLD);
        /* General parameters */
        Zoltan_Set_Param(zz, "DEBUG_LEVEL", "0");
        Zoltan_Set_Param(zz, "LB_METHOD", "GRAPH");
        Zoltan_Set_Param(zz, "LB_APPROACH", "PARTITION");
        Zoltan_Set_Param(zz, "NUM_GID_ENTRIES", "1"); 
        Zoltan_Set_Param(zz, "NUM_LID_ENTRIES", "1");
        Zoltan_Set_Param(zz, "RETURN_LISTS", "ALL");

        /* Graph parameters */
        Zoltan_Set_Param(zz, "CHECK_GRAPH", "2"); 
        /* 0-remove all, 1-remove none */
        Zoltan_Set_Param(zz, "PHG_EDGE_SIZE_THRESHOLD", ".35");

        /* Query functions - defined in simpleQueries.h */
        Zoltan_Set_Num_Obj_Fn(zz, get_number_of_vertices, &myGraph);
        Zoltan_Set_Obj_List_Fn(zz, get_vertex_list, &myGraph);
        Zoltan_Set_Num_Edges_Multi_Fn(zz, get_num_edges_list, &myGraph);
        Zoltan_Set_Edge_List_Multi_Fn(zz, get_edge_list, &myGraph);

        rc = Zoltan_LB_Partition(zz, /* input (all remaining fields are output) */
            &changes,        /* 1 if partitioning was changed, 0 otherwise */ 
            &numGidEntries,  /* Number of integers used for a global ID */
            &numLidEntries,  /* Number of integers used for a local ID */
            &numImport,      /* Number of vertices to be sent to me */
            &importGlobalGids,  /* Global IDs of vertices to be sent to me */
            &importLocalGids,   /* Local IDs of vertices to be sent to me */
            &importProcs,    /* Process rank for source of each incoming vertex */
            &importToPart,   /* New partition for each incoming vertex */
            &numExport,      /* Number of vertices I must send to other processes*/
            &exportGlobalGids,  /* Global IDs of the vertices I must send */
            &exportLocalGids,   /* Local IDs of the vertices I must send */
            &exportProcs,    /* Process to which I send each of the vertices */
            &exportToPart);  /* Partition to which each vertex will belong */
        if (rc != ZOLTAN_OK){
            printf("sorry...\n");
            MPI_Finalize();
            Zoltan_Destroy(&zz);
            exit(0);
        }
//         parts = (int *)malloc(sizeof(int) * myGraph.numMyVertices);
//         for (i=0; i < myGraph.numMyVertices; i++) parts[i] = rank;
//         for (i=0; i < numExport; i++)
//             parts[exportLocalGids[i]] = exportToPart[i];
        /* zoltan的全局顶点id是从1开始的, 所以parts的大小要加1*/
        parts = (int *)malloc(sizeof(int) * (numGlobalVertices + 1));
        for (i = 0; i < numGlobalVertices + 1; i++)
            parts[i] = -1;
        for (i=0; i < myGraph.numMyVertices; i++)
            parts[myGraph.vertexGID[i]] = rank;
        for (i=0; i < numExport; i++)
            parts[exportGlobalGids[i]] = exportToPart[i];
        sendToBelongProc(rank, &myGraph, numGlobalVertices, parts, size);
        if (parts) free(parts);
        Zoltan_LB_Free_Part(&importGlobalGids, &importLocalGids, 
                          &importProcs, &importToPart);
        Zoltan_LB_Free_Part(&exportGlobalGids, &exportLocalGids, 
                          &exportProcs, &exportToPart);
        Zoltan_Destroy(&zz);
    }
    myGraph.fvwgts  = (float *)malloc(sizeof(float) * myGraph.numMyVertices); 
    myGraph.fadjwgt = (float *)malloc(sizeof(float) * myGraph.numAllNbors); 
    myGraph.fvwgts  = (float *)malloc(sizeof(float) * myGraph.numMyVertices); 
    myGraph.status  = (int *)malloc(sizeof(int) * myGraph.numMyVertices); 

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
    ntxs = myGraph.numMyVertices;
    if(INFO) printf("%d 节点和边数: %d %zd\n", rank, myGraph.numMyVertices,
            myGraph.nborIndex[myGraph.numMyVertices]);
    //if (DEBUG) displayGraph(myGraph);

    /* 首先使用算法对图进行初始化:
     * PageRank: 初始化为空
     * SSSP: 如果调用的是SSSP算法, 需要将图进行初始化
     * startv.value = 0, otherv = ∞ */
    gmr->initGraph(&myGraph);

    while(true && iterNum < MAX_ITERATION && iterNum < gmr->algoIterNum){
        /* 从当前子图获取需要向其他节点发送的字节数 */
        sendcounts = getSendBufferSize(&myGraph, size, rank);
        memset(allothersendcounts, 0, (size + 1) * size * sizeof(int));
        memset(sendcountswithconv, 0, (size + 1) * sizeof(int));
        memset(recvcounts, 0, size * sizeof(int));
        memset(sdispls, 0, size * sizeof(int));
        memset(rdispls, 0, size * sizeof(int));

        /* 打印出节点发送缓冲区大小 */
        if(INFO) printf("Process %d send size:", rank);
        for (i = 0; i < size; i++) {
            if(INFO) printf(" %d\t", sendcounts[i]);
        }
        if(INFO) printf("\n");
        
        /* 计算迭代进度(收敛顶点数/总的顶点数 * 10,000), 并将其加在缓存大小后面
         * ,接收数据后,首先判断所有进程的收敛进度是否接收,再拷贝接收缓存大小 */
        recordTick("bexchangecounts");
        /* 将收敛精度乘以10000, 实际上是以精确到小数点后两位以整数形式发送 */
        int convergence = (int)(1.0 * convergentVertex / myGraph.numMyVertices * 10000);
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
        if(INFO) printf("Prcess %d recv size:", rank);
        for (i = 0; i < size; i++) {
            if(INFO) printf(" %d\t", recvcounts[i]);
        }
        if(INFO) printf("\n");

        /* 申请发送和接收数据的空间 */
        rb = (char*)malloc(accumulate(recvcounts, recvcounts + size, 0));
        if ( !rb ) {
            perror( "can't allocate recv buffer");
            free(sb); MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE);
        }
        /* 计算发送和接收缓冲区偏移 */
        for (i = 1; i != size; i++) {
            sdispls[i] += (sdispls[i - 1] + sendcounts[i - 1]);
            rdispls[i] += (rdispls[i - 1] + recvcounts[i - 1]);
        }

        /* 将要发送的数据从graph中拷贝到发送缓存sb中 */
        recordTick("bgetsendbuffer");
        sb = getSendbuffer(&myGraph, sendcounts, sdispls, size, rank);
        recordTick("egetsendbuffer");

        /* 调用MPIA_Alltoallv(), 交换数据 */
        recordTick("bexchangedata");
        MPI_Alltoallv(sb, sendcounts, sdispls, MPI_CHAR, rb, recvcounts,
                rdispls, MPI_CHAR, MPI_COMM_WORLD);
        recordTick("eexchangedata");

        /* 打印输出接收到的图顶点信息 */
        int rbsize = accumulate(recvcounts, recvcounts + size, 0);
        totalRecvBytes += rbsize;
        if (DEBUG) {
            printf("Process %d recv:", rank);
            for ( i = 0 ; i < rbsize; ) {
                int vid, eid, location, eloc, edgenum = 0;
                float fewgt, fvwgt;
                memcpy(&vid, rb + i, sizeof(int));
                memcpy(&location, rb + (i += sizeof(int)), sizeof(int));
                memcpy(&fvwgt, rb + (i += sizeof(int)), sizeof(float));
                memcpy(&edgenum, rb + (i += sizeof(float)), sizeof(int));
                printf("===>%d %d %f %d ", vid, location, fvwgt, edgenum);
                i += sizeof(int);
                /* 读取边的另外一个顶点 */
                for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                    memcpy(&eid, rb + i, sizeof(int));
                    printf(" %d", eid + 1);
                }
                /* 读取边的另外一个顶点所在的节点 */
                for (int j = 0; j < edgenum; j++, i += sizeof(int)) {
                    memcpy(&eloc, rb + i, sizeof(int));
                    printf(" %d", eloc);
                }
                /* 读取边的权重 */
                for (int j = 0; j < edgenum; j++, i += sizeof(float)) {
                    memcpy(&fewgt, rb + i, sizeof(float));
                    printf(" %f", fewgt);
                }
                printf(" %d / %d\n", i, rbsize);
            }
            printf("\n");
        }

        /*合并其他节点传递过来的顶点，计算并判断是否迭代结束*/
        recordTick("bcomputing");
        computing(rank, &myGraph, rb, rbsize, gmr); 
        recordTick("ecomputing");
        free(sb); free(rb);
        MPI_Barrier(MPI_COMM_WORLD);
        recordTick("eiteration");

        /* 释放内存, 并打印迭代信息 */
        if (sendcounts) free(sendcounts);
        iterNum++;
        printTimeConsume(rank);
    }
    printf("程序运行结束,总共耗时:%f secs, 通信量:%ld Byte, 最大消耗"
            "内存:(未统计)Byte\n", MPI_Wtime() - starttime, totalRecvBytes);

    MPI_Finalize();
    /* 打印处理完之后的结果(图) */
    //displayGraph(graph);
    gmr->printResult(&myGraph);
    graph_free(&myGraph);
    //graph_Free(&graph);
    if (sdispls) free(sdispls); if(rdispls) free(rdispls);
    if (recvcounts) free(recvcounts);
    if(allothersendcounts) free(allothersendcounts);
    if(sendcountswithconv) free(sendcountswithconv);
}

/*************************************************************************/
/*! 此文件主要用来定义与图操作相关的函数，如:
 * 分图、图显示、计算发送顶点缓存大小
*/
/**************************************************************************/

/* 用于定义测试的整图信息(非子图), 分图之前的整图信息 */
struct Graph {
    char name[256];         /*! 图的名字 */
    size_t nvtx;            /*! 图的顶点数 */
    size_t nedge;           /*! 图的边数 */
};

/* 用于测试的图 */
const Graph graphs[] = {{"small", 10, 10}, {"4elt", 15606, 45878}, 
                    {"mdual", 258569, 513132}};
const int subgraphNum = 3;      /* 分图之后的子图个数 */
const int testgraph   = 2;      /* 用于测试的图位于上面图数组的序号 */

/* 使用MPI并行分图算法 */
void partitionGraph(gk_graph_t *graph, int nparts) { }

/* 打印图信息 */
void displayGraph(gk_graph_t *graph) {
    int hasvwgts, hasvsizes, hasewgts;
    hasewgts  = (graph->iadjwgt || graph->fadjwgt);
    hasvwgts  = (graph->ivwgts || graph->fvwgts);
    hasvsizes = (graph->ivsizes || graph->fvsizes);

    for (int i=0; i<graph->nvtxs; i++) {
        if (hasvsizes) {
            if (graph->ivsizes)
                printf(" %d", graph->ivsizes[i]);
            else
                printf(" %f", graph->fvsizes[i]);
        }
        if (hasvwgts) {
            if (graph->ivwgts)
                printf(" %d", graph->ivwgts[i]);
            else
                printf(" %f", graph->fvwgts[i]);
        }

        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            /* graph的邻居点数组中实际上存储的是以0开始的顶点,实际使用是以1开始
             * 所以需要在顶点原值的基础上加1 */
            printf(" %d", graph->adjncy[j]+1);
            if (hasewgts) {
                if (graph->iadjwgt)
                    printf(" %d", graph->iadjwgt[j]);
                else 
                    printf(" %f", graph->fadjwgt[j]);
                }
        }
        printf("\n");
    }
}

/* 获取发往其他各运算节点的字节数 */
int *getSendBufferSize(const gk_graph_t *graph, const int psize, const int rank) {
    int *sendcounts = (int*)malloc(psize * sizeof(int));
    memset(sendcounts, 0, psize * sizeof(int));
    /* 先遍历一次需要发送的数据，确定需要和每个节点交换的数据 */
    for (int i=0; i<graph->nvtxs; i++) {
        /* 记录当前节点的大小 */
        int currentVertexSize = 0;
        // id, loc, weight of vertex
        currentVertexSize += sizeof(int);
        currentVertexSize += sizeof(int);
        currentVertexSize += graph->ivwgts ? sizeof(int) : sizeof(float);
        // edges and weight of edges
        int neighborNum = graph->xadj[i+1] - graph->xadj[i];
        currentVertexSize += sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * (graph->iadjwgt ?  sizeof (int) : sizeof(float));

        /* visited 用于记录当前遍历的顶点是否已经发送给节点iadjwgt[j]*/
        std::bitset<subgraphNum> visited;
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            if (graph->iadjwgt[j] == rank) continue;
            if (visited.test(graph->iadjwgt[j])) continue;
            visited.set(graph->iadjwgt[j]);
            sendcounts[graph->iadjwgt[j]] += currentVertexSize;
        }
    }
    return sendcounts;
}

/* 将要发送的数据从graph中拷贝到发送缓存sb中 */
char *getSendbuffer(gk_graph_t *graph, int *sendcounts, int *sdispls, 
        int psize, int rank) {
    /* 申请发送缓存 */
    char *sb = (char*)malloc(std::accumulate(sendcounts, sendcounts + psize, 0));
    if ( !sb ) {
        perror( "can't allocate send buffer" );
        MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE); 
    }

    /* 将要发送顶点拷贝到对应的缓存 */
    int *offsets = (int*)malloc(psize * sizeof(int));
    memset(offsets, 0, psize * sizeof(int));
    for (int i=0; i<graph->nvtxs; i++) {
        /* record the size of current vertex */
        int currentVertexSize = 0;
        // accumulate the size of id, loc, weight of vertex
        currentVertexSize += sizeof(int);
        currentVertexSize += sizeof(int);
        currentVertexSize += graph->ivwgts ? sizeof(int) : sizeof(float);
        // accumulate the size of edges and weight of edges
        int neighborNum = graph->xadj[i+1] - graph->xadj[i];
        currentVertexSize += sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * (graph->iadjwgt ?  sizeof (int) : sizeof(float));

        /* vertex memory image: (id | location | weight | edgenum 
         * | edges1-n | weightOfedges1-n) */
        char *vertex = (char*)malloc(currentVertexSize * sizeof(char));;
        memset(vertex, 0, currentVertexSize * sizeof(char));
        memcpy(vertex, &(graph->ivsizes[i]), sizeof(int));
        memcpy(vertex + sizeof(int), &rank, sizeof(int));
        memcpy(vertex + 2 * sizeof(int), &(graph->fvwgts[i]), sizeof(float));
        memcpy(vertex + 2 * sizeof(int) + sizeof(float), &neighborNum, sizeof(int));
        memcpy(vertex + 3 * sizeof(int) + sizeof(float), &(graph->adjncy[graph->xadj[i]]), 
                neighborNum * sizeof(int));

        /* 将顶点的边的权重weight拷贝进发送缓存 */
        if (graph->iadjwgt) 
            memcpy(vertex + (3 + neighborNum) * sizeof(int) + sizeof(float), 
                    &(graph->iadjwgt[graph->xadj[i]]), neighborNum * sizeof(int));
        else 
            memcpy(vertex + (3 + neighborNum) * sizeof(int) + sizeof(float), 
                    &(graph->iadjwgt[graph->xadj[i]]), neighborNum * sizeof(float));

        /* visited 用于记录当前遍历的顶点是否已经发送给节点iadjwgt[j]*/
        std::bitset<subgraphNum> visited;
        for (int j = graph->xadj[i]; j< graph->xadj[i+1]; j++) {
            if (graph->iadjwgt[j] == rank) continue;
            if (visited.test(graph->iadjwgt[j])) continue;
            visited.set(graph->iadjwgt[j]);
            memcpy(sb + sdispls[graph->iadjwgt[j]] + offsets[graph->iadjwgt[j]],
                    vertex, currentVertexSize);
            offsets[graph->iadjwgt[j]] += currentVertexSize;
        }
        if (vertex) free(vertex);
    }
    if (offsets) free(offsets);
    return sb;
}

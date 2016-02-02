/*************************************************************************/
/*! 此文件主要用来定义与图操作相关的函数和变常量，如:
 * 变常量: 子图个数, 当前使用的子图的定点数、边数
 * 分图、图显示、计算发送顶点缓存大小
*/
/**************************************************************************/

/* List terminator for GKfree() */
#define LTERM                   (void **) 0
#define GK_GRAPH_FMT_METIS      1

/* subgraphNum: 子图个数
 * ntxs:        当前使用的子图中顶点数
 * nedges:      当前使用的子图边条数
 * NODE_STATUS: 顶点状态 */
const int subgraphNum = 3;
int ntxs = 0;
int nedges = 0;
enum NODE_STATUS {active, inactive};

/*-------------------------------------------------------------
 * The following data structure stores a sparse graph 
 *-------------------------------------------------------------*/
typedef struct graph_t {
  int32_t nvtxs;                /*!< The number of vertices in the graph */
  ssize_t *xadj;                /*!< The ptr-structure of the adjncy list */
  int32_t *adjncy;              /*!< The adjacency list of the graph */
  int32_t *iadjwgt;             /*!< The integer edge weights */
  float *fadjwgt;               /*!< The floating point edge weights */
  int32_t *ivwgts;              /*!< The integer vertex weights */
  float *fvwgts;                /*!< The floating point vertex weights */
  int32_t *ivsizes;             /*!< The integer vertex sizes */
  float *fvsizes;               /*!< The floating point vertex sizes */
  int32_t *vlabels;             /*!< The labels of the vertices */
  int32_t *adjloc;              /*!< The location of terminal of edge */
  int32_t *status;              /*!< The status of vertex */
} graph_t;

/* 使用MPI并行分图算法 */
void partitionGraph(graph_t *graph, int nparts) { }

/*************************************************************************
* This function is my wrapper around free, allows multiple pointers    
**************************************************************************/
void my_free(void **ptr1,...)
{
  va_list plist;
  void **ptr;

  if (*ptr1 != NULL) {
    free(*ptr1);
  }
  *ptr1 = NULL;

  va_start(plist, ptr1);
  while ((ptr = va_arg(plist, void **)) != LTERM) {
    if (*ptr != NULL) {
      free(*ptr);
    }
    *ptr = NULL;
  }
  va_end(plist);
}          

/*************************************************************************/
/*! Initializes the graph.
    \param graph is the graph to be initialized.
*/
/*************************************************************************/
void graph_Init(graph_t *graph)
{
  memset(graph, 0, sizeof(graph_t));
  graph->nvtxs = -1;
}
/*************************************************************************/
/*! Allocate memory for a graph and initializes it 
    \returns the allocated graph. The various fields are set to NULL.
*/
/**************************************************************************/
graph_t *graph_Create()
{
  graph_t *graph;
  graph = (graph_t *)malloc(sizeof(graph_t));
  graph_Init(graph);
  return graph;
}

/*************************************************************************/
/*! Frees only the memory allocated for the graph's different fields and
    sets them to NULL.
    \param graph is the graph whose contents will be freed.
*/    
/*************************************************************************/
void graph_FreeContents(graph_t *graph)
{
  my_free((void **)&graph->xadj, &graph->adjncy, 
          &graph->iadjwgt, &graph->fadjwgt,
          &graph->ivwgts, &graph->fvwgts,
          &graph->ivsizes, &graph->fvsizes,
          &graph->vlabels, &graph->adjloc,
          &graph->status,
          LTERM);
}

/*************************************************************************/
/*! Frees all the memory allocated for a graph.
    \param graph is the graph to be freed.
*/
/*************************************************************************/
void graph_Free(graph_t **graph)
{
  if (*graph == NULL)
    return;
  graph_FreeContents(*graph);
  my_free((void **)graph, LTERM);
}

/**************************************************************************/
/*! Reads a sparse graph from the supplied file 
    \param filename is the file that stores the data.
    \param format is the graph format. The supported values are:
           GK_GRAPH_FMT_METIS.
    \param isfewgts is 1 if the edge-weights should be read as floats
    \param isfvwgts is 1 if the vertex-weights should be read as floats
    \param isfvsizes is 1 if the vertex-sizes should be read as floats
    \returns the graph that was read.
*/
/**************************************************************************/
graph_t *graph_Read(std::string filename, int format, int isfewgts, 
                int isfvwgts, int isfvsizes)
{
    ssize_t i, k, l;
    size_t nvtxs, nedges, fmt, ncon;
    int readsizes=0, readwgts=0, readvals=0, numbering=0, readedgeloc = 0;
    char fmtstr[256];
    graph_t *graph=NULL;

    /* 打开并判断文件状态, 过滤掉注释行 */
    std::ifstream fin(filename, std::ifstream::in);
    if (!fin.good()) {
        errexit(SIGINT, "Open file failed.\n");
    }
    std::string line;
    while (fin.good()) {
        std::getline(fin, line);
        if (line[0] != '%' && line[0] != 'c')
            break;
    }

    /* 读取子图文件的头信息: 定点数,边数及图文件格式 */
    fmt = ncon = 0;
    std::istringstream iss(line);
    iss >> nvtxs >> nedges >> fmt >> ncon;
    if (nvtxs <= 0 || nedges <= 0)
        errexit(SIGINT, "Header line must contain at least 2 integers (#vtxs and #edges).\n");
    if (fmt > 1111)
        errexit(SIGERR, "Cannot read this type of file format fmt!\n");

    /* 根据子图文件中记录的图格式, 实例化变量以待用 */
    sprintf(fmtstr, "%04zu", fmt%10000);
    readsizes   = (fmtstr[0] == '1');
    readwgts    = (fmtstr[1] == '1');
    readvals    = (fmtstr[2] == '1');
    readedgeloc = (fmtstr[3] == '1');
    numbering   = 1;
    ncon        = (ncon == 0 ? 1 : ncon);

    /* 创建图结构体, 并申请图中数据所用到的空间 */
    graph = graph_Create();
    graph->nvtxs = nvtxs;
    graph->xadj   = (ssize_t*)malloc(sizeof(ssize_t) * (nvtxs+1));
    graph->adjncy = (int32_t*)malloc(sizeof(int32_t) * nedges);
    graph->status = (int32_t*)malloc(sizeof(int32_t) * nvtxs);
    if (readvals) {
        if (isfewgts)
            graph->fadjwgt = (float*)malloc(sizeof(float) * nedges);
        else
            graph->iadjwgt = (int32_t*)malloc(sizeof(int32_t) * nedges);
    }

    /* 判断图vsize数据类型, 实际上重用为记录顶点的id */
    if (readsizes) {
        if (isfvsizes)
            graph->fvsizes = (float*)malloc(sizeof(float) * nvtxs);
        else
            graph->ivsizes = (int32_t*)malloc(sizeof(int32_t) * nvtxs);
    }
    /* 判断顶点权重数据类型, 并申请存储空间 */
    if (readwgts) {
        if (isfvwgts)
            graph->fvwgts = (float*)malloc(sizeof(float) * nvtxs*ncon);
        else
            graph->ivwgts = (int32_t*)malloc(sizeof(int32_t) * nvtxs*ncon);
    }
    /* 判断是否存储了顶点所在的节点, 并申请存储空间 */
    if (readedgeloc) {
        graph->adjloc = (int32_t*)malloc(sizeof(int32_t) * nedges);
    }

  /*----------------------------------------------------------------------
   * Read the sparse graph file
   *---------------------------------------------------------------------*/
  numbering = (numbering ? - 1 : 0);
  for (graph->xadj[0]=0, k=0, i=0; i<nvtxs; i++) {
    do {
       getline(fin, line);
    } while (line[0] == '%');

    std::istringstream iss(line);
    size_t vid, eid, eloc;
    int ivwgt, iewgt;
    float fvwgt, fewgt;

    /* 默认状态下顶点的状态为active */
    graph->status[i] = active;

    /* Read vertex sizes */
    if (readsizes) {
        iss >> vid;
        graph->ivsizes[i] = vid;
        if (graph->ivsizes[i] < 0)
            errexit(SIGERR, "The size for vertex must be >= 0\n");
    }

    /* Read vertex weights */
    if (readwgts) {
        for (l=0; l<ncon; l++) {
            if (isfvwgts) {
                iss >> fvwgt;
                graph->fvwgts[i*ncon+l] = fvwgt;
                if (graph->fvwgts[i*ncon+l] < 0)
                    errexit(SIGERR, "The weight vertex and constraint must be >= 0\n");
            }
            else {
                iss >> ivwgt;
                graph->ivwgts[i*ncon+l] = ivwgt;
                if (graph->ivwgts[i*ncon+l] < 0)
                    errexit(SIGERR, "The weight vertex and constraint must be >= 0\n");
            }
        }
    }

    /* Read the rest of the row */
    while (iss >> eid) {
        /* read the edge */
        if ((graph->adjncy[k] = eid + numbering) < 0)
            errexit(SIGERR, "Error: Invalid column number.\n");

        /* read the location of terminal of edge */
        if (readedgeloc) {
            iss >> eloc;
            if ((graph->adjloc[k] = eloc) < 0)
                errexit(SIGERR, "Error: Invalid partition number at row .\n");
        }

        if (readvals) {
            if (isfewgts) {
                iss >> fewgt;
                graph->fadjwgt[k] = fewgt;
            }
            else {
                iss >> iewgt;
                graph->iadjwgt[k] = iewgt;
            }
        }
        k++;
    }
    graph->xadj[i+1] = k;
  }

  if (k != nedges)
      errexit(SIGERR, "gk_graph_Read: Something wrong with the number of edges in "
                       "the input file. nedges, Actualnedges.\n");
  fin.close();
  return graph;
}

/* 打印图信息 */
void displayGraph(graph_t *graph) {
    int hasvwgts, hasvsizes, hasewgts, haseloc;
    hasewgts  = (graph->iadjwgt || graph->fadjwgt);
    hasvwgts  = (graph->ivwgts || graph->fvwgts);
    hasvsizes = (graph->ivsizes || graph->fvsizes);
    haseloc   = graph->adjloc != NULL;

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
            if (haseloc)
                printf(" %d", graph->adjloc[j]);
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
int *getSendBufferSize(const graph_t *graph, const int psize, const int rank) {
    int hasvwgts, hasvsizes, hasewgts, haseloc;
    hasewgts  = (graph->iadjwgt || graph->fadjwgt);
    hasvwgts  = (graph->ivwgts || graph->fvwgts);
    hasvsizes = (graph->ivsizes || graph->fvsizes);
    haseloc   = graph->adjloc != NULL;

    int *sendcounts = (int*)malloc(psize * sizeof(int));
    memset(sendcounts, 0, psize * sizeof(int));
    /* 先遍历一次需要发送的数据，确定需要和每个节点交换的数据 */
    for (int i=0; i<graph->nvtxs; i++) {
        /* 如果当前顶点vertex为iactive, 则不用发送 */
        /* TODO: Bugfix - 不将收敛的节点发送, 可能导致不收敛 */
        //if (graph->status[i] == inactive) continue;
        /* 记录当前节点向外发送时，所需要的缓存大小 */
        int currentVertexSize = 0;
        // id, loc, weight of vertex
        currentVertexSize += sizeof(int);
        currentVertexSize += sizeof(int);
        currentVertexSize += graph->ivwgts ? sizeof(int) : sizeof(float);
        // quantity, terminal, location and weight of edges
        int neighborNum = graph->xadj[i+1] - graph->xadj[i];
        currentVertexSize += sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * (graph->iadjwgt ?  sizeof (int) : sizeof(float));

        /* visited 用于记录当前遍历的顶点是否已经发送给节点adjloc[j] */
        std::bitset<subgraphNum> visited;
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            /* 如果当前顶点的终止顶点为在当前节点, 则不必发送 */
            if (graph->adjloc[j] == rank) continue;
            /* 如果当前顶点已经在之前发过给此节点, 则也不用发送 */
            if (visited.test(graph->adjloc[j])) continue;
            visited.set(graph->adjloc[j]);
            sendcounts[graph->adjloc[j]] += currentVertexSize;
        }
    }
    return sendcounts;
}

/* 将要发送的数据从graph中拷贝到发送缓存sb中 */
char *getSendbuffer(graph_t *graph, int *sendcounts, int *sdispls, 
        int psize, int rank) {
    /* 申请发送缓存 */
    char *sb = (char*)malloc(std::accumulate(sendcounts, sendcounts + psize, 0));
    if ( !sb ) {
        perror( "can't allocate send buffer" );
        MPI_Abort(MPI_COMM_WORLD,EXIT_FAILURE); 
    }

    /* 将要发送顶点拷贝到对应的缓存: 内存拷贝(是否有方法减少拷贝?) */
    int *offsets = (int*)malloc(psize * sizeof(int));
    memset(offsets, 0, psize * sizeof(int));
    for (int i = 0; i < graph->nvtxs; i++) {
        /* 如果当前顶点vertex为iactive, 则不用发送 */
        /* TODO: Bugfix - 不将收敛的节点发送, 可能导致不收敛 */
        //if (graph->status[i] == inactive) continue;
        /* record the size of current vertex */
        int currentVertexSize = 0;
        // accumulate the size of id, loc, weight of vertex
        currentVertexSize += sizeof(int);
        currentVertexSize += sizeof(int);
        currentVertexSize += graph->ivwgts ? sizeof(int) : sizeof(float);
        // accumulate the quantity , terminal, loc and weight of edges
        int neighborNum = graph->xadj[i+1] - graph->xadj[i];
        currentVertexSize += sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * sizeof(int);
        currentVertexSize += neighborNum * (graph->iadjwgt ? sizeof(int) : sizeof(float));

        /* vertex memory image: (id | location | weight | edgenum 
         * | edges1-n | locationOfedges1-n | weightOfedges1-n) */
        char *vertex = (char*)malloc(currentVertexSize * sizeof(char));;
        memset(vertex, 0, currentVertexSize * sizeof(char));
        memcpy(vertex, &(graph->ivsizes[i]), sizeof(int));
        memcpy(vertex + sizeof(int), &rank, sizeof(int));
        memcpy(vertex + 2 * sizeof(int), &(graph->fvwgts[i]), sizeof(float));
        memcpy(vertex + 2 * sizeof(int) + sizeof(float), &neighborNum, sizeof(int));
        memcpy(vertex + 3 * sizeof(int) + sizeof(float), &(graph->adjncy[graph->xadj[i]]),
                neighborNum * sizeof(int));
        memcpy(vertex + (3 + neighborNum) * sizeof(int) + sizeof(float), 
                &(graph->adjloc[graph->xadj[i]]), neighborNum * sizeof(int));

        /* 将顶点的边的权重weight拷贝进发送缓存 */
        if (graph->iadjwgt) 
            memcpy(vertex + (3 + 2 * neighborNum) * sizeof(int) + sizeof(float), 
                    &(graph->iadjwgt[graph->xadj[i]]), neighborNum * sizeof(int));
        else 
            memcpy(vertex + (3 + 2 * neighborNum) * sizeof(int) + sizeof(float), 
                    &(graph->fadjwgt[graph->xadj[i]]), neighborNum * sizeof(float));

        /* visited 用于记录当前遍历的顶点是否已经发送给节点adjloc[j]*/
        std::bitset<subgraphNum> visited;
        for (int j = graph->xadj[i]; j< graph->xadj[i+1]; j++) {
            if (graph->adjloc[j] == rank) continue;
            if (visited.test(graph->adjloc[j])) continue;
            visited.set(graph->adjloc[j]);
            memcpy(sb + sdispls[graph->adjloc[j]] + offsets[graph->adjloc[j]],
                    vertex, currentVertexSize);
            offsets[graph->adjloc[j]] += currentVertexSize;
        }
        if (vertex) free(vertex);
    }
    if (offsets) free(offsets);
    return sb;
}

#include <mpi.h>
#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>

/* List terminator for GKfree() */
#define LTERM                   (void **) 0
#define GK_GRAPH_FMT_METIS      1
#define GRAPH_DEBUG             true 
#define GRAPH_INFO              true 

/* MAX_PROCESSOR: 最大子图个数 
 * ntxs:          当前进程使用的子图中顶点数
 * nedges:        当前进程使用的子图边条数
 * NODE_STATUS:   顶点状态 */
const int MAX_PROCESSOR = 256;
int ntxs = 0;
int nedges = 0;
enum NODE_STATUS {active, inactive};

static int numbering = 0;
typedef struct graph_t {
  int nvtxs; /* total vertices in in my partition */
  int nedges;   /* total number of neighbors of my vertices */
  int *ivsizes;    /* global ID of each of my vertices */
  int *xadj;    /* xadj[i] is location of start of neighbors for vertex i */
  int *adjncy;      /* adjncys[xadj[i]] is first neighbor of vertex i */
  int *adjloc;     /* process owning each nbor in adjncy */

  int32_t *ivwgts;              /*!< The integer vertex weights */
  int32_t *iadjwgt;             /*!< The integer edge weights */

  float *fvwgts;
  float *fadjwgt;
  int *status;
} graph_t;

unsigned int simple_hash(unsigned int *key, unsigned int n)
{
  unsigned int h, rest, *p, bytes, num_bytes;
  char *byteptr;

  num_bytes = (unsigned int) sizeof(int);

  /* First hash the int-sized portions of the key */
  h = 0;
  for (p = (unsigned int *)key, bytes=num_bytes;
       bytes >= (unsigned int) sizeof(int);
       bytes-=sizeof(int), p++){
    h = (h*2654435761U) ^ (*p);
  }

  /* Then take care of the remaining bytes, if any */
  rest = 0;
  for (byteptr = (char *)p; bytes > 0; bytes--, byteptr++){
    rest = (rest<<8) | (*byteptr);
  }

  /* Merge the two parts */
  if (rest)
    h = (h*2654435761U) ^ rest;

  /* Return h mod n */
  return (h%n);
}

/* Function to find next line of information in input file */
static int get_next_line(FILE *fp, char *buf, int bufsize)
{
  int i, cval, len;
  char *c;

  while (1){

    c = fgets(buf, bufsize, fp);

    if (c == NULL)
      return 0;  /* end of file */

    len = strlen(c);

    for (i=0, c=buf; i < len; i++, c++){
      cval = (int)*c; 
      if (isspace(cval) == 0) break;
    }
    if (i == len) continue;   /* blank line */
    if (*c == '#') continue;  /* comment */

    if (c != buf){
      strcpy(buf, c);
    }
    break;
  }

  return strlen(buf);  /* number of characters */
}

/* Function to find next line of information in input file */
static int get_next_vertex(FILE *fp, int *val)
{
  char buf[512];
  char *c = buf;
  int num, cval, i = 0, j = 0, len = 0, bufsize = 512;
  while (1){
    c = fgets(buf, bufsize, fp);
    /* end of the file */
    if (c == NULL) {
        if (j > 2)
            val[1] = j - 2;
        return j;
    }
    
    len = strlen(c);
    for (i=0, c=buf; i < len; i++, c++){
      cval = (int)*c; 
      if (isspace(cval) == 0) break;
    }
    if (i == len) continue;   /* blank line */
    if (*c == '#') continue;  /* comment */
    
    int from, to;
    num = sscanf(buf, "%d%*[^0-9]%d", &from, &to);
    if (num != 2)
        perror("输入文件格式错误.\n");
    if (j > 0) {
        if (from + numbering == val[0])
            val[j++] = to + numbering;
        else {
            fseek(fp, -1 * len, SEEK_CUR);
            break;
        }
    }
    else {
        if (from == 0) numbering = 1;
        val[j++] = from + numbering, val[j++] = 0, val[j++] = to + numbering;
    }
  }
  val[1] = j - 2;
  return j;
}

/* Proc 0 notifies others of error and exits */

static void input_file_error(int numProcs, int tag, int startProc)
{
int i, val[2];

  val[0] = -1;   /* error flag */

  fprintf(stderr,"ERROR in input file.\n");

  for (i=startProc; i < numProcs; i++){
    /* these procs have posted a receive for "tag" expecting counts */
    MPI_Send(val, 2, MPI_INT, i, tag, MPI_COMM_WORLD);
  }
  for (i=1; i < startProc; i++){
    /* these procs are done and waiting for ok-to-go */
    MPI_Send(val, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
  }

  MPI_Finalize();
  exit(1);
}


/*
 * Read the graph in the input file and distribute the vertices.
 */

void read_input_file(int myRank, int numProcs, char *fname, graph_t *graph)
{
  char buf[512];
  int bufsize;
  int numGlobalVertices, numGlobalNeighbors;
  int num, nnbors, ack=0;
  int vGID;
  int i, j, procID;
  int vals[102400], send_count[2];
  int *idx;
  unsigned int id;
  FILE *fp;
  MPI_Status status;
  int ack_tag = 5, count_tag = 10, id_tag = 15;
  graph_t *send_graph;

  if (myRank == 0){

    bufsize = 512;

    fp = fopen(fname, "r");

    /* Get the number of vertices */

    num = get_next_line(fp, buf, bufsize);
    if (num == 0) input_file_error(numProcs, count_tag, 1);
    num = sscanf(buf, "%d", &numGlobalVertices);
    if (num != 1) input_file_error(numProcs, count_tag, 1);

    /* Get the number of vertex neighbors  */

    num = get_next_line(fp, buf, bufsize);
    if (num == 0) input_file_error(numProcs, count_tag, 1);
    num = sscanf(buf, "%d", &numGlobalNeighbors);
    if (num != 1) input_file_error(numProcs, count_tag, 1);

    /* Allocate arrays to read in entire graph */
    graph->xadj = (int *)malloc(sizeof(int) * (numGlobalVertices + 1));
    graph->adjncy = (int *)malloc(sizeof(int) * numGlobalNeighbors);
    graph->adjloc = (int *)malloc(sizeof(int) * numGlobalNeighbors);
    graph->ivsizes = (int *)malloc(sizeof(int) * numGlobalVertices);

    graph->xadj[0] = 0;

    int old_vid = 1;
    for (i=0; i < numGlobalVertices; i++){
      num = get_next_vertex(fp, vals);
      /*if (num != 0 && vals[0] == old_vid && old_vid != 1) {
         printf("error input file.\n");
         MPI_Finalize();
         exit(1);
      }*/
//       printf("get num:%d\n", num);
      if (num == 0) {
        for (j = 1; i < numGlobalVertices; j++, i++) {
            vGID = vals[0] + j;
            nnbors = 0;
//             printf("==Fill gap:%d 0\n", vGID);
            graph->ivsizes[i] = (int)vGID;
            graph->xadj[i+1] = graph->xadj[i] + nnbors;
        }
        break;
      }
      if (num < 2) input_file_error(numProcs, count_tag, 1);
//        for (int k = 0; k < num; k++) {
//          printf("%d ", vals[k]);
//        }
//        printf("\n");
//       printf("======>after get from point:%d\n", vals[0]);

      /* 如果读取的图顶点的id, 与前一个读取的图顶点id不连续, 则需要补足中间不连续部分 */
      if (vals[0] - old_vid > 1) {
        if (vals[0] > numGlobalVertices) {
            perror("读取的图顶点id大于了图的最大顶点数.\n");
            MPI_Finalize();
            exit(1);
        }
        for (j = vals[0] - old_vid - 1; j > 0; j--, i++) {
            vGID = vals[0] - j;
            nnbors = 0;
//             printf("Fill gap:%d 0\n", vGID);
            graph->ivsizes[i] = (int)vGID;
            graph->xadj[i+1] = graph->xadj[i] + nnbors;
        }
      }

      old_vid = vals[0];

      vGID = vals[0];
      nnbors = vals[1];

      if (num < (nnbors + 2)) input_file_error(numProcs, count_tag, 1);
      graph->ivsizes[i] = (int)vGID;

      for (j=0; j < nnbors; j++){
        graph->adjncy[graph->xadj[i] + j] = (int)vals[2 + j];
      }

      graph->xadj[i+1] = graph->xadj[i] + nnbors;
    }
    printf("end of read file\n");

    fclose(fp);

    /* Assign each vertex to a process using a hash function */
    for (i=0; i <numGlobalNeighbors; i++){
      id = (unsigned int)graph->adjncy[i];
      graph->adjloc[i] = simple_hash(&id, numProcs);
    } 

    /* Create a sub graph for each process */

    send_graph = (graph_t *)calloc(sizeof(graph_t) , numProcs);

    for (i=0; i < numGlobalVertices; i++){
      id = (unsigned int)graph->ivsizes[i];
      procID = simple_hash(&id, numProcs);
      send_graph[procID].nvtxs++;
    }

    for (i=0; i < numProcs; i++){
      num = send_graph[i].nvtxs;
      send_graph[i].ivsizes = (int *)malloc(sizeof(int) * num);
      send_graph[i].xadj = (int *)calloc(sizeof(int) , (num + 1));
    }

    /* 新增注释: 用来缓存目前为止各个进程已放入的顶点数 */
    idx = (int *)calloc(sizeof(int), numProcs);

    for (i=0; i < numGlobalVertices; i++){

      id = (unsigned int)graph->ivsizes[i];
      nnbors = graph->xadj[i+1] - graph->xadj[i];
      procID = simple_hash(&id, numProcs);

      j = idx[procID];
      send_graph[procID].ivsizes[j] = (int)id;
      send_graph[procID].xadj[j+1] = send_graph[procID].xadj[j] + nnbors;

      idx[procID] = j+1;
    }

    for (i=0; i < numProcs; i++){

      num = send_graph[i].xadj[send_graph[i].nvtxs];

      send_graph[i].adjncy = (int *)malloc(sizeof(int) * num);
      send_graph[i].adjloc= (int *)malloc(sizeof(int) * num);

      send_graph[i].nedges = num;
    }

    memset(idx, 0, sizeof(int) * numProcs);

    for (i=0; i < numGlobalVertices; i++){

      id = (unsigned int)graph->ivsizes[i];
      nnbors = graph->xadj[i+1] - graph->xadj[i];
      procID = simple_hash(&id, numProcs);
      j = idx[procID];

      if (nnbors > 0){
        memcpy(send_graph[procID].adjncy + j, graph->adjncy + graph->xadj[i],
               nnbors * sizeof(int));
  
        memcpy(send_graph[procID].adjloc + j, graph->adjloc + graph->xadj[i],
               nnbors * sizeof(int));
  
        idx[procID] = j + nnbors;
      }
    }

    free(idx);

    /* Process zero sub-graph */

    free(graph->ivsizes);
    free(graph->xadj);
    free(graph->adjncy);
    free(graph->adjloc);

    *graph = send_graph[0];
    graph->fvwgts  = (float *)malloc(sizeof(int) * send_graph[0].nvtxs);
    graph->fadjwgt = (float *)malloc(sizeof(int) * send_graph[0].nedges);
    graph->status  = (int *)malloc(sizeof(int) * send_graph[0].nvtxs);
    /* Send other processes their subgraph */

    for (i=1; i < numProcs; i++){
      send_count[0] = send_graph[i].nvtxs;
      send_count[1] = send_graph[i].nedges;

      MPI_Send(send_count, 2, MPI_INT, i, count_tag, MPI_COMM_WORLD);
      MPI_Recv(&ack, 1, MPI_INT, i, ack_tag, MPI_COMM_WORLD, &status);

      if (send_count[0] > 0){
        MPI_Send(send_graph[i].ivsizes, send_count[0], MPI_INT, i,
                id_tag, MPI_COMM_WORLD);
        free(send_graph[i].ivsizes);

        MPI_Send(send_graph[i].xadj, send_count[0] + 1, MPI_INT, i,
                id_tag + 1, MPI_COMM_WORLD);
        free(send_graph[i].xadj);

        if (send_count[1] > 0){
          MPI_Send(send_graph[i].adjncy, send_count[1], MPI_INT, i,
                  id_tag + 2, MPI_COMM_WORLD);
          free(send_graph[i].adjncy);

          MPI_Send(send_graph[i].adjloc, send_count[1], MPI_INT, i, id_tag + 3, MPI_COMM_WORLD);
          free(send_graph[i].adjloc);
        }
      }
    }

    free(send_graph);
    /* signal all procs it is OK to go on */
    ack = 0;
    for (i=1; i < numProcs; i++){
      MPI_Send(&ack, 1, MPI_INT, i, 0, MPI_COMM_WORLD);
    }
  }
  else{
    MPI_Recv(send_count, 2, MPI_INT, 0, count_tag, MPI_COMM_WORLD, &status);

    if (send_count[0] < 0){
      MPI_Finalize();
      exit(1);
    }

    ack = 0;

    graph->nvtxs = send_count[0];
    graph->nedges  = send_count[1];

    if (send_count[0] > 0){

      graph->ivsizes = (int *)malloc(sizeof(int) * send_count[0]);
      graph->xadj = (int *)malloc(sizeof(int) * (send_count[0] + 1));
      graph->fvwgts  = (float *)malloc(sizeof(int) * send_count[0]);
      graph->status  = (int *)malloc(sizeof(int) * send_count[0]);

      if (send_count[1] > 0){
        graph->adjncy  = (int *)malloc(sizeof(int) * send_count[1]);
        graph->fadjwgt = (float *)malloc(sizeof(int) * send_count[1]);
        graph->adjloc = (int *)malloc(sizeof(int) * send_count[1]);
      }
    }

    MPI_Send(&ack, 1, MPI_INT, 0, ack_tag, MPI_COMM_WORLD);

    if (send_count[0] > 0){
      MPI_Recv(graph->ivsizes,send_count[0],MPI_INT, 0,
              id_tag, MPI_COMM_WORLD, &status);
      MPI_Recv(graph->xadj,send_count[0] + 1, MPI_INT, 0,
              id_tag + 1, MPI_COMM_WORLD, &status);

      if (send_count[1] > 0){
        MPI_Recv(graph->adjncy,send_count[1], MPI_INT, 0,
                id_tag + 2, MPI_COMM_WORLD, &status);
        MPI_Recv(graph->adjloc,send_count[1], MPI_INT, 0, id_tag + 3, MPI_COMM_WORLD, &status);
      }
    }

    /* ok to go on? */

    MPI_Recv(&ack, 1, MPI_INT, 0, 0, MPI_COMM_WORLD, &status);
    if (ack < 0){
      MPI_Finalize();
      exit(1);
    }
  }
}

/* 获取发往其他各运算节点的字节数 */
int *getSendBufferSize(const graph_t *graph, const int psize, const int rank) {
    int *sendcounts = (int*)malloc(psize * sizeof(int));
    memset(sendcounts, 0, psize * sizeof(int));
    /* 先遍历一次需要发送的数据，确定需要和每个节点交换的数据 */
    for (int i=0; i<graph->nvtxs; i++) {
        /* 如果当前顶点vertex为iactive, 则不用发送 */
        /* TODO: Bugfix - 不将收敛的节点发送, 可能导致不收敛 */
        if (graph->status[i] == inactive) continue;
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
        short visited[MAX_PROCESSOR];
        memset(visited, 0, MAX_PROCESSOR * sizeof(short));
        for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
            /* 如果当前顶点的终止顶点为在当前节点, 则不必发送 */
            if (graph->adjloc[j] == rank) continue;
            /* 如果当前顶点已经在之前发过给此节点, 则也不用发送 */
            if (visited[graph->adjloc[j]] == 1) continue;
            visited[graph->adjloc[j]] = 1;
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
        if (graph->status[i] == inactive) continue;
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
        short visited[MAX_PROCESSOR];
        memset(visited, 0, MAX_PROCESSOR * sizeof(short));
        for (int j = graph->xadj[i]; j< graph->xadj[i+1]; j++) {
            if (graph->adjloc[j] == rank) continue;
            if (visited[graph->adjloc[j]] == 1) continue;
            visited[graph->adjloc[j]] = 1;
            memcpy(sb + sdispls[graph->adjloc[j]] + offsets[graph->adjloc[j]],
                    vertex, currentVertexSize);
            offsets[graph->adjloc[j]] += currentVertexSize;
        }
        if (vertex) free(vertex);
    }
    if (offsets) free(offsets);
    return sb;
}

void graph_Free(graph_t *graph) {
    if (graph->nvtxs > 0) {
        free(graph->iviszes);
        free(graph->xadj);
    }
//     if (graph->ivsizes != NULL) free(graph->ivsizes);
//     if (graph->xadj != NULL) free(graph->xadj);
//     if (graph->adjncy != NULL) free(graph->adjncy);
//     if (graph->adjloc != NULL) free(graph->adjloc);
// 
//     if (graph->ivwgts != NULL) free(graph->ivwgts);
//     if (graph->iadjwgt != NULL) free(graph->iadjwgt);
// 
//     if (graph->fvwgts != NULL) free(graph->fvwgts);
//     if (graph->adjwgt != NULL) free(graph->adjwgt);
//     if (graph->status != NULL) free(graph->status); 
}

/* 打印图信息 */
void displayGraph(graph_t *graph) {
//     int hasvwgts, hasvsizes, hasewgts, haseloc;
//     hasewgts  = (graph->iadjwgt || graph->fadjwgt);
//     hasvwgts  = (graph->ivwgts || graph->fvwgts);
//     hasvsizes = (graph->ivsizes || graph->fvsizes);
//     haseloc   = graph->adjloc != NULL;
// 
//     for (int i=0; i<graph->nvtxs; i++) {
//         if (hasvsizes) {
//             if (graph->ivsizes)
//                 printf(" %d", graph->ivsizes[i]);
//             else
//                 printf(" %f", graph->fvsizes[i]);
//         }
//         if (hasvwgts) {
//             if (graph->ivwgts)
//                 printf(" %d", graph->ivwgts[i]);
//             else
//                 printf(" %f", graph->fvwgts[i]);
//         }
// 
//         for (int j=graph->xadj[i]; j<graph->xadj[i+1]; j++) {
//             printf(" %d", graph->adjncy[j]);
//             if (haseloc)
//                 printf(" %d", graph->adjloc[j]);
//             if (hasewgts) {
//                 if (graph->iadjwgt)
//                     printf(" %d", graph->iadjwgt[j]);
//                 else 
//                     printf(" %f", graph->fadjwgt[j]);
//                 }
//         }
//         printf("\n");
//     }
}

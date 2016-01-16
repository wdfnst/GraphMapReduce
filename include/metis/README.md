# 切图功能

#### 一、修改
1. 根据处理器的位数修改include/metis.h
   > IDXTYPEWIDTH = 32/64
2. 主要修改了gpmetis.c和io.c
gpmetis:
```c++
...
 if (status != METIS_OK) {
    printf("\n***Metis returned with an error.\n");
  }
  else {
    if (!params->nooutput) {
      /* Write the solution */
      gk_startcputimer(params->iotimer);
      WritePartition(params->filename, part, graph->nvtxs, params->nparts);
      WriteSubgraph(graph, params->filename, part, params->nparts);
      gk_stopcputimer(params->iotimer);
    }

    GPReportResults(params, graph, part, objval);
  }
...
```
io.c： 添加了函数 void WriteSubgraph(graph_t *graph, char *fname, idx_t *part, idx_t nparts)
```c++
/*************************************************************************/
/*! This function writes a graph into a file  */
/*************************************************************************/
void WriteSubgraph(graph_t *graph, char *fname, idx_t *part, idx_t nparts)
{
  idx_t i, j, nvtxs, ncon;
  idx_t *xadj, *adjncy, *adjwgt, *vwgt, *vsize;
  int hasvwgt=0, hasewgt=0, hasvsize=0;
  FILE *fpout[nparts];
  char filenames[nparts][MAXLINE];

  nvtxs  = graph->nvtxs;
  ncon   = graph->ncon;
  xadj   = graph->xadj;
  adjncy = graph->adjncy;
  vwgt   = graph->vwgt;
  vsize  = graph->vsize;
  adjwgt = graph->adjwgt;

  /* determine if the graph has non-unity vwgt, vsize, or adjwgt */
  if (vwgt) {
    for (i=0; i<nvtxs*ncon; i++) {
      if (vwgt[i] != 1) {
        hasvwgt = 1;
        break;
      }
    }
  }
  if (vsize) {
    for (i=0; i<nvtxs; i++) {
      if (vsize[i] != 1) {
        hasvsize = 1;
        break;
      }
    }
  }
  if (adjwgt) {
    for (i=0; i<xadj[nvtxs]; i++) {
      if (adjwgt[i] != 1) {
        hasewgt = 1;
        break;
      }
    }
  }

  for (i = 0; i < nparts; i++) {
      sprintf(filenames[i], "%s.subgraph.%"PRIDX, fname, i);
      fpout[i] = gk_fopen(filenames[i], "w", __func__);

      /* write the header line */
      fprintf(fpout[i], "%"PRIDX" %"PRIDX" %s", nvtxs, xadj[nvtxs]/2, "011");
      if (hasvwgt || hasvsize || hasewgt) {
        fprintf(fpout[i], " %d%d%d", hasvsize, hasvwgt, hasewgt);
        if (hasvwgt)
          fprintf(fpout[i], " %d", (int)graph->ncon);
      }
  }

  /* write the rest of the graph */
  for (i=0; i<nvtxs; i++) {
    int sgindex = part[i];
    fprintf(fpout[sgindex], "\n");
    if (hasvsize)
      fprintf(fpout[sgindex], " %"PRIDX, vsize[i]);

    if (hasvwgt) {
      for (j=0; j<ncon; j++)
        fprintf(fpout[sgindex], " %"PRIDX, vwgt[i*ncon+j]);
    }
    fprintf(fpout[sgindex], " %"PRIDX, i + 1);

    for (j=xadj[i]; j<xadj[i+1]; j++) {
      fprintf(fpout[sgindex], " %"PRIDX, adjncy[j]+1);
      /* print the partition of the current neighbor belongs to*/
      fprintf(fpout[sgindex], " %"PRIDX, part[adjncy[j]]);
      if (hasewgt)
        fprintf(fpout[sgindex], " %"PRIDX, adjwgt[j]);
    }
  }

  for (i = 0; i < nparts; i++) {
      gk_fclose(fpout[i]);
  }
}
```

#### 二、编译

编译之后可执行文件位于build/

#### 三、输入、执行和结果
  > 注: 主要修改了gpmetis.c的代码，修改之后生成了gpmetis，
gpmetis的用法:
  ```sh
    gpmetis parth/input_graph_file nparts  % 生成子图文件名为input_graph_file.subgraph.i (i=0, 1, 2, nparts)
  ```
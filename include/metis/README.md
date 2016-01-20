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
    ...
}
```

#### 二、编译

编译之后可执行文件位于build/

#### 三、输入、执行和结果
1. 输入：输入文件格式说明
测试图文件位于: include/metis/graphs

> 4elt.graph    README        copter2.graph mdual.graph   metis.mesh    test.mgraph

2. gpmetis的用法:
  > 注: 主要修改了gpmetis.c的代码，修改之后生成了gpmetis

  ```sh
    gpmetis parth/input_graph_file nparts  % 生成子图文件名为input_graph_file.subgraph.i (i=0, 1, 2, nparts)
  ```

3. 生成子图：
生成子图文件名为input_graph_file.subgraph.i (i=0, 1, 2, nparts)
# 重用说明

#### 1. 图中数据结构体重用说明
重用的字段:
> ivsizes 用于表示当前顶点(vertex)的id;
> ivwgts 用于表示当前顶点的值
> iadjwgt 用于表示此邻居顶点(vertex)所在节点(process)的id;
```c++
/*-------------------------------------------------------------
 * The following data structure stores a sparse graph 
 *-------------------------------------------------------------*/
typedef struct gk_graph_t {
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
} gk_graph_t;
```

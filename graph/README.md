# 例图数据说明
主要包含了三个例图数据，其结构定义在graph.h中:
```c++
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
const int testgraph = 2;        /* 用于测试的图位于上面图数组的序号 */
```

#### 1. small.graph 小型图
共有10个顶点和10条边

#### 2. 4elt.graph, 中型规模图
共有15,606个顶点和45,878条边

#### 3. mdual.graph 中大型规模图
共有258,569个顶点和513,132条边

 #### 4. large.graph 大型规模图 (还没找到这个数据)
共有258,569个顶点和513,132条边

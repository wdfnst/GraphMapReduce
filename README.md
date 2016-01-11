#GraphMapReduce: 基于MapReduce的编程模型的图计算框架

(名词约束: 顶点Vertex-图中顶点;节点Process-计算单元节点)
### 框架的基础
1. MPI:
结算节点之间通信通过MPI实现；
2. MapReduce编程模型
3. 图划分:
为了将整图的不同部分放到不同的计算节点进行并行计算，首先将整划分为若干子图，本框架中每个子图包含三个部分{inners, borders, neighbors}, inners表示子图内与其他子图没有连接的顶点；borders表示子图内与其他子图又连接的顶点；neighbors表示子图外与本子图连接的顶点。

### 例子
1. 如下图所示的简单图，包含10个顶点{1,2,3,...,10}，划分之后，包含三个子图subgraphs[3]，每个子图中包含的顶点如图所示

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/165752_c1b26e30_496314.png "简单图划分示意图")

2. 迭代过程

- 每个子图现将自己的边界顶点发送给其所连接的邻居节点，采用MPI_Alltoall()实现；
- 在每个计算节点的内部，将每个顶点<id, loc, [neighbors]执行map函数, value>映射为若干键值对:
          > {key, value1},其中key in [neighbors], value1 = value / neighbors.size()
```
void map(Vertex &v, std::list<KV> &kvs){
    int neighbor_count = 0;
    while(v.neighbors[neighbor_count] != 0)neighbor_count++;

    float value = v.value / neighbor_count;
    for (int i = 0; i < neighbor_count; i++)
        kvs.push_back({v.neighbors[i], value});
}
```
- 在每个节点内将map生成的键值对按键值进行排序
- 根据键值，对键值相同的键值组执行reduce函数
```
KV reduce(std::list<KV> &kvs) {
    float sum = 0.0;
    for (auto kv : kvs) {
        sum += kv.value;
    }

    /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
    sum = 0.5 * sum + (1 - 0.5) / (sizeof(vs) / sizeof(Vertex) - 1); 
    return {kvs.front().key, sum};
}
```
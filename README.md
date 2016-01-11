#GraphMapReduce: 基于MapReduce的编程模型的图计算框架

(名词约束: 顶点-图中顶点;节点-计算单元节点)
### 框架的基础
1. MPI
2. MapReduce编程模型
3. 图划分

### 例子
1. 如下图所示：

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/165752_c1b26e30_496314.png "在这里输入图片标题")

2. 迭代过程
- 每个子图现将自己的边界顶点发送给其所连接的邻居节点；
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
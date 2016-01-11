#GraphMapReduce: 基于MapReduce的编程模型的图计算框架

(名词约束: 顶点Vertex-图中顶点;节点Process-计算单元节点)
## 一. 框架的基础
#### 1. MPI:
结算节点之间通信通过MPI实现；
#### 2. MapReduce编程模型
#### 3. 图划分:
为了将整图的不同部分放到不同的计算节点进行并行计算，需要将整划分为若干子图。本框架中每个子图包含三个部分{inners, borders, neighbors}, inners表示子图内与其他子图没有连接的顶点；borders表示子图内与其他子图又连接的顶点；neighbors表示子图外与本子图连接的顶点。

## 二. 例子
### 2.1 PageRank
#### 2.1.1. 如下图所示的简单图，包含10个顶点{1,2,3,...,10}，划分之后，包含三个子图subgraphs[3]，每个子图中包含的顶点如图所示
顶点表示：
```
/* id-顶点id; loc-顶点所在的计算节点;neighbors-顶点的邻居;value-顶点的值 */
struct Vertex {
    int id;
    int loc;
    int neighbors[512];
    float value;
};
```
Vertex graph[12] = {{}, {1, 0, {2, 3, 4, 5, 10}, 1.0}, {2, 1, {1, 3, 6, 7}, 1.0}, 
                             {3, 2, {1, 2, 8, 9}, 1.0},
                             {4, 0, {1}, 1.0}, {5, 0, {1}, 1.0}, {6, 1, {2}, 1.0}, {7, 1, {2}, 1.0},
                             {8, 2, {3}, 1.0}, {9, 2, {3}, 1.0}, {10, 2, {1}, 1.0}};

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/165752_c1b26e30_496314.png "简单图划分示意图")

#### 2.1.2. 迭代过程

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

#### 2.2.3 PageRank终止点问题和陷阱问题
上述上网者的行为是一个马尔科夫过程的实例，要满足收敛性，需要具备一个条件：
图是强连通的，即从任意网页可以到达其他任意网页：
互联网上的网页不满足强连通的特性，因为有一些网页不指向任何网页，如果按照上面的计算，上网者到达这样的网页后便走投无路、四顾茫然，导致前面累 计得到的转移概率被清零，这样下去，最终的得到的概率分布向量所有元素几乎都为0。假设我们把上面图中C到A的链接丢掉，C变成了一个终止点，得到下面这 个图：
![输入图片说明](http://jingyan.baidu.com/album/4ae03de31bbf883eff9e6b1b.html?picindex=5 "在这里输入图片标题")
另外一个问题就是陷阱问题，即有些网页不存在指向其他网页的链接，但存在指向自己的链接。比如下面这个图：
![输入图片说明](http://jingyan.baidu.com/album/4ae03de31bbf883eff9e6b1b.html?picindex=8 "在这里输入图片标题")
上网者跑到C网页后，就像跳进了陷阱，陷入了漩涡，再也不能从C中出来，将最终导致概率分布值全部转移到C上来，这使得其他网页的概率分布值为0，从而整个网页排名就失去了意义。
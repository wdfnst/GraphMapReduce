#GraphMapReduce: 基于MapReduce编程模型的图计算框架

(名词约束: 顶点Vertex-图中顶点;节点Process-计算单元节点),目录说明:     


> 代码主要包含四个文件: gmr.cpp gmr.h algorithms.h graph.h     
> |__graph/---------#此目录包含测试用的图例数据     
> |__include/-------#此目录包含所使用到的第三方库的头文件(目前只用到了ParMetis，去掉了GKlib)     
> |__lib/------------#包含了使用到的第三方库     
> |__gmr.cpp------#程序的main函数入口和迭代循环     
> |__gmr.h---------#包含主要的计算过程函数computing()和计算结果更新函数updateGraph()     
> |__algorithm.h---#常用图算法的MapReduce实现     
> |__graph.h-------#定义了图数据结果和常用的集中图操作函数   
 

## 一. 框架的基础
#### 1. MPI:
结算节点之间通信通过MPI实现；
#### 2. MapReduce编程模型
#### 3. 图划分:
为了将整图的不同部分放到不同的计算节点进行并行计算，需要将整划分为若干子图。本框架中每个子图包含三个部分{inners, borders, neighbors}, inners表示子图内与其他子图没有连接的顶点；borders表示子图内与其他子图又连接的顶点；neighbors表示子图外与本子图连接的顶点。

## 二、 编译和运行
### 1. (not mandatory)切图
切图采用了metis库，其源码和说明位于include/metis中，其编译使用可参考include/metis/README.md.
已经有切好的例图，位于graph/下。

### 2. 编译gmr
make clean && make

### 3. 运行
mpirun -np 3(注:进程数) ./gmr
> 注: 目前正在移植Parmetis(MPI-based)分图部分代码, 所以暂时只能运行graph/已经分好的例图。因为例图都被分为了三个子图，所以目前只能运行三个MPI进程。

## 三、迭代计算过程
#### 1. 数据交换:
第一步,先遍历自己计算的子图graph与其他子图的邻居情况,并收集需要向其他节点发送的字节数,并申请发送缓冲区;

第二步,通过MPI_Alltoall()与其他节点交换其他节点需要接受的字节数,每个节点收到信息后,各自计算和申请接受数据需要的空间。

第三步,再次遍历自己计算的子图graph,并将需要发往其他节点的顶点信心拷贝到发送缓存char *sb;

第四部,调用MPI_Alltoallv(),将发送缓存中的数据发往各节点.
#### 2. 计算1th/2:map
将子图graph和接受缓冲区中的数据实例化为顶点Vertex，再调用业务逻辑函数map将Vertex生成key/value list。
#### 3. 对生成key/value list进行排序: sort
#### 4. 计算2th/2:reduce
将排序好的key/value list按照业务逻辑函数reduce进行规约.
#### 5. 将reduce计算的结果更新到graph中

## 四. 例子
### 4.1 PageRank
#### 4.1.1. 如下包含10个顶点的简单图，划分之后包含三个子图subgraphs[3]:
![输入图片说明](http://git.oschina.net/uploads/images/2016/0120/132332_24897e71_496314.png "在这里输入图片标题")

#### 4.1.2. 迭代过程

- 每个子图现将自己的边界顶点发送给其所连接的邻居节点，采用MPI_Alltoall()实现；
- 在每个计算节点的内部，将每个顶点<id, loc, [neighbors]执行map函数, value>映射为若干键值对:
          > {key, value1},其中key in [neighbors], value1 = value / neighbors.size()
```c++
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
```c++
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

#### 4.2.3 PageRank终止点问题和陷阱问题
上述上网者的行为是一个马尔科夫过程的实例，要满足收敛性，需要具备一个条件：
图是强连通的，即从任意网页可以到达其他任意网页：
互联网上的网页不满足强连通的特性，因为有一些网页不指向任何网页，如果按照上面的计算，上网者到达这样的网页后便走投无路、四顾茫然，导致前面累 计得到的转移概率被清零，这样下去，最终的得到的概率分布向量所有元素几乎都为0。假设我们把上面图中C到A的链接丢掉，C变成了一个终止点，得到下面这个图：

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/214258_5e3a6ed7_496314.jpeg "在这里输入图片标题")

另外一个问题就是陷阱问题，即有些网页不存在指向其他网页的链接，但存在指向自己的链接。比如下面这个图：

![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/214318_aadc9dd1_496314.jpeg "在这里输入图片标题")

上网者跑到C网页后，就像跳进了陷阱，陷入了漩涡，再也不能从C中出来，将最终导致概率分布值全部转移到C上来，这使得其他网页的概率分布值为0，从而整个网页排名就失去了意义。

### 4.2 单源最短路算法SSSP（DJ算法）

### 4.3 并行广度优先搜索算法的MapReduce实现

### 4.4 二度人脉算法:广度搜索算法

## 五、对比实验
Processor\Platform |   GMR      |    Spark      |   GraphX       |    GraphLab      |     Pregel  |
      1            |            |               |                |                  |             |
      3            |            |               |                |                  |             |
      8            |            |               |                |                  |             |
      16           |            |               |                |                  |             |
      64           |            |               |                |                  |             |
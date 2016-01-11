#GraphMapReduce: 基于MapReduce的编程模型的图计算框架

(名词约束: 顶点-图中顶点;节点-计算单元节点)
### 框架的基础
1. MPI
2. MapReduce编程模型
3. 图划分

### 例子
1. 如下图所示：
![输入图片说明](http://git.oschina.net/uploads/images/2016/0111/165339_b54bd86a_496314.png "在这里输入图片标题")
2. 迭代过程
- 每个子图现将自己的边界顶点发送给其所连接的邻居节点；
- 在每个计算节点的内部，将每个顶点<id, loc, [neighbors], value>映射为若干键值对:
          > {key, value1},其中key in [neighbors], value1 = value / neighbors.size()

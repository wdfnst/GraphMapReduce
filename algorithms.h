/****************************************************
 * 该类用于实现PageRank算法
 * *************************************************/
class PageRank : public GMR {
public:
    /* Map/Reduce编程模型中的Map函数 */
    void map(Vertex &v, std::list<KV> &kvs) {
        if(DEBUG) printf("get in the map function.\n");
        float value = v.value / v.neighborSize;
        for (int i = 0; i < v.neighborSize; i++) {
            kvs.push_back({v.neighbors[i], value});
            if(DEBUG) printf("%d %f\n", v.neighbors[i], value);
        }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        float sum = 0.0;
        for (auto kv : kvs) {
            sum += kv.value;
        }
        /*Pagerank=a*(p1+p2+…Pm)+(1-a)*1/n，其中m是指向网页j的网页j数，n所有网页数*/
        sum = 0.5 * sum + (1 - 0.5) / ntxs; 
        if (DEBUG) printf("reduce result: %d %f\n", kvs.front().key, sum);
        return {kvs.front().key, sum};
    }
};

/****************************************************
 * 该类用实求解SSSP(单源最短路径)算法
 ***************************************************/
class SSSP : public GMR {
public:
    void map(Vertex &v, std::list<KV> &kvs) {
        /* 当第一次迭代的时候对图进行出发顶点和其他个顶点的值进行初始化 */
        if(DEBUG) printf("get in the map function.\n");
        kvs.push_back({v.id, v.value});
        for (int i = 0; i < v.neighborSize; i++) {
            kvs.push_back({v.neighbors[i], v.value + v.edgewgt[i]});
            if(DEBUG) printf("%d %f\n", v.neighbors[i], v.value + v.edgewgt[i]);
        }
    }

    /* 用于将Map/Reduce计算过程中产生的KV list进行排序 */
    void sort(std::list<KV> &kvs) { }

    /* Map/Reduce编程模型中的Reduce函数 */
    KV reduce(std::list<KV> &kvs) {
        // dist1 [u] = Edge[v][u]
        // dist k [u] = min{ dist k-1 [u], min{ dist k-1 [j] + Edge[j][u] } }, j=0,1,…,n-1,j≠u
        float shortestPath = 0.0;
        for (auto kv : kvs) {
            if (kv.value < shortestPath)
                shortestPath = kv.value;
        }
        if (DEBUG) printf("reduce result: %d %f\n", kvs.front().key, shortestPath);
        return {kvs.front().key, shortestPath};
    }
};

/****************************************************
 * 该类用实求解BFS(广度优先搜索算法)算法
 ***************************************************/
class BFS : public GMR { };

/****************************************************
 * 该类用实求解triangleCount(三角形关系计算)算法
 ***************************************************/
class triangleCount : public GMR { };

/****************************************************
 * 该类用实求解connectedComponents(连图子图)算法
 ***************************************************/
class connectedComponents : public GMR { };


/****************************************************
 * 其他图算法
 * 参考:
 * The following is a quick summary of the functionality defined in both Graph and GraphOps
 * https://spark.apache.org/docs/latest/graphx-programming-guide.html#graph-operators
 ***************************************************/
class stronglyConnectedComponents : public GMR { };

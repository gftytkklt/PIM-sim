#include <iostream>
#include <vector>
#include <map>
#include <algorithm>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/topological_sort.hpp>

// 使用Boost Graph Library的有向图类型
typedef boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS> Graph;
typedef boost::graph_traits<Graph>::vertex_descriptor Vertex;

int main() {
    // 构建有向无环图（DAG）
    Graph g;

    // 假设我们有5个节点，编号为0到4
    // 添加边来表示数据依赖关系，例如：0 -> 1 表示节点1依赖于节点0
    boost::add_edge(0, 2, g);
    boost::add_edge(1, 2, g);
    boost::add_edge(1, 3, g);
    boost::add_edge(2, 4, g);
    boost::add_edge(3, 4, g);

    // 存储每个节点的层级
    std::vector<int> layer(boost::num_vertices(g), 1); // 初始层级为1

    // 进行拓扑排序
    std::vector<Vertex> topo_order;
    try {
        boost::topological_sort(g, std::back_inserter(topo_order));
    }
    catch(boost::not_a_dag& e) {
        std::cerr << "图不是有向无环图 (DAG)!" << std::endl;
        return -1;
    }

    // 拓扑排序的结果是逆序的，因此需要反转
    std::reverse(topo_order.begin(), topo_order.end());

    // 遍历拓扑排序后的节点，计算每个节点的层级
    for(auto v : topo_order) {
        // 获取当前节点的所有子节点（通过out_edges）
        boost::graph_traits<Graph>::out_edge_iterator out_i, out_end;
        for(boost::tie(out_i, out_end) = boost::out_edges(v, g); out_i != out_end; ++out_i) {
            Vertex child = boost::target(*out_i, g);
            // 更新子节点的层级为当前节点的层级 +1，取最大值
            if(layer[child] < layer[v] + 1) {
                layer[child] = layer[v] + 1;
            }
        }
    }

    // 输出每个节点的层级
    for(int i = 0; i < layer.size(); ++i) {
        std::cout << "节点 " << i << " 的层级: " << layer[i] << std::endl;
    }

    return 0;
}

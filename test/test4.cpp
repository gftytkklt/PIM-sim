#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/depth_first_search.hpp>
#include <iostream>

using namespace boost;
using namespace std;

struct vertex_properties {
    int id;
};

typedef adjacency_list<vecS, vecS, undirectedS, vertex_properties> Graph;

int main() {
    // 创建一个简单的无向图
    Graph g(5);

    // 添加一些边
    add_edge(0, 1, g);
    add_edge(1, 2, g);
    add_edge(2, 3, g);
    add_edge(3, 4, g);

    // 深度优先搜索
    depth_first_search(g, visitor(default_dfs_visitor()));

    return 0;
}

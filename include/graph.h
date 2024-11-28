#ifndef GRAPH_H
#define GRAPH_H
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <iostream>

template <typename NodePropertyType, typename EdgePropertyType>
class BaseGraph {
public:
    using NodeProperty = boost::property<boost::vertex_property_tag, NodePropertyType>;
    using EdgeProperty = boost::property<boost::edge_property_tag, EdgePropertyType>;

    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, NodeProperty, EdgeProperty>;

    Graph g;

    // 构造函数：可以传递不同的节点和边权重结构体
    BaseGraph() = default;

    // 向图中添加节点和边
    void add_node(const NodeProperty& node_prop) {
        boost::add_vertex(node_prop, g);
    }

    void add_edge(int v1, int v2, const EdgeProperty& edge_prop) {
        boost::add_edge(v1, v2, edge_prop, g);
    }
};

#include "graph.hpp"
#endif
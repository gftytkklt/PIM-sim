#ifndef GRAPH_H
#define GRAPH_H
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <iostream>
#include <vector>

struct NNkernel {
    std::pair<int,int> wsize;   // (w, h) of kernel
    std::pair<int,int> channel; // (in, out) of stride
    int scaling_factor;         // stride * pooling stride, fmap reducing factor
};

enum class DepType {
    Accum,  // intra-layer accumulation
    Prop,   // inter-layer propagation
    // ...
};

struct CNode {
    int layer;                          // Layer index
    std::pair<int,int> size;            // (WL, BL) of xbar
    int ofmap_size;                     // Ofm size
    std::pair<int,int> id_cin, id_cout; // (cin, cout) channel index  
};

struct CEdge {
    DepType c_type;
    int datavolume;
};

struct TNode {

};

struct TEdge {

};

struct HNode {

};

struct HEdge {

};

struct DNode {

};

struct DEdge {

};

template <typename NodePropertyType, typename EdgePropertyType>
class BaseGraph {
public:
    using NodeProperty = boost::property<boost::vertex_property_tag, NodePropertyType>;
    using EdgeProperty = boost::property<boost::edge_property_tag, EdgePropertyType>;

    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, NodeProperty, EdgeProperty>;

    // 构造函数：可以传递不同的节点和边权重结构体
    BaseGraph() = default;
    // virtual void add_node(const NodeProperty& node_prop) = 0;
    // virtual void add_edge(int v1, int v2, const EdgeProperty& edge_prop) = 0;

    // 向图中添加节点和边
    void add_node(const NodeProperty& node_prop, Graph& g) {
        boost::add_vertex(node_prop, g);
    }

    void add_edge(int v1, int v2, const EdgeProperty& edge_prop, Graph& g) {
        boost::add_edge(v1, v2, edge_prop, g);
    }

    virtual void analysis() = 0;
// private:
    // Graph g;
};

class CGraph : public BaseGraph<CNode, CEdge> {
public:
    CGraph(const std::vector<NNkernel>& kernels);
    void analysis() override {
        std::cout << "Analysis of CGraph" << std::endl;
    }
private:
    Graph cg;
};

class TGraph : public BaseGraph<TNode, TEdge> {

};

class HGraph : public BaseGraph<HNode, HEdge> {

};

class DGraph : public BaseGraph<DNode, DEdge> {

};

#include "graph.hpp"
#endif
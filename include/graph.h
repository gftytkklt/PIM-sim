#ifndef GRAPH_H
#define GRAPH_H
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <iostream>
#include <vector>
#include <memory>

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

    using Node = boost::graph_traits<Graph>::vertex_descriptor;
    using Edge = boost::graph_traits<Graph>::edge_descriptor;

    // default ctor
    BaseGraph() = default;
    // virtual void add_node(const NodeProperty& node_prop) = 0;
    // virtual void add_edge(int v1, int v2, const EdgeProperty& edge_prop) = 0;

    // add vertices and edges
    void add_node(const NodeProperty& node_prop, Graph& g) {
        boost::add_vertex(node_prop, g);
    }

    void add_edge(int v1, int v2, const EdgeProperty& edge_prop, Graph& g) {
        boost::add_edge(v1, v2, edge_prop, g);
    }

    // remove vertices and edges
    void remove_node(int v, Graph& g) {
        boost::remove_vertex(v, g);
    }

    void remove_edge(int v1, int v2, Graph& g) {
        boost::remove_edge(v1, v2, g);
    }

    // vertices and edges getter
    const NodeProperty& get_node_property(int v, const Graph& g) const {
        Node n = boost::vertex(v, g);
        return g[n];
        // return g[v];
    }

    const EdgeProperty& get_edge_property(int v1, int v2, const Graph& g) const {
        Edge e;
        bool found;
        boost::tie(e, found) = boost::edge(v1, v2, g);
        if (found) {
            return g[e];
        }
        return EdgeProperty();
    }

    // vertices and edges attributes setter
    void set_node_property(int v, const NodeProperty& node_prop, Graph& g) {
        Node n = boost::vertex(v, g);
        g[n] = node_prop;
    }

    void set_edge_property(int v1, int v2, const EdgeProperty& edge_prop, Graph& g) {
        Edge e;
        bool found;
        boost::tie(e, found) = boost::edge(v1, v2, g);
        if (found) {
            g[e] = edge_prop;
        }
    }

    // get adjacent vertices and edges
    std::vector<int> get_adjacent_nodes(int v, const Graph& g) const {
        std::vector<int> adj_nodes;
        boost::graph_traits<Graph>::adjacency_iterator ai, ai_end;
        for (boost::tie(ai, ai_end) = boost::adjacent_nodes(v, g); ai != ai_end; ++ai) {
            adj_nodes.push_back(*ai);
        }
        return adj_nodes;
    }

    std::vector<int> get_adjacent_edges(int v, const Graph& g) const {
        std::vector<int> adj_edges;
        boost::graph_traits<Graph>::out_edge_iterator ei, ei_end;
        for (boost::tie(ei, ei_end) = boost::out_edges(v, g); ei != ei_end; ++ei) {
            adj_edges.push_back(*ei);
        }
        return adj_edges;
    }

    // analysis func interface
    virtual void analysis() = 0;
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
public:
    TGraph(std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of TGraph" << std::endl;
    }
private:
    Graph tg;
    std::shared_ptr<const CGraph> cg_ref;
};

class HGraph : public BaseGraph<HNode, HEdge> {
public:
    HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of HGraph" << std::endl;
    }
private:
    Graph hg;
    std::shared_ptr<const TGraph> tg_ref;
    std::shared_ptr<const CGraph> cg_ref;
};

class DGraph : public BaseGraph<DNode, DEdge> {
public:
    DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of DGraph" << std::endl;
    }
private:
    Graph dg;
    std::shared_ptr<const HGraph> hg_ref;
    std::shared_ptr<const TGraph> tg_ref;
    std::shared_ptr<const CGraph> cg_ref;
};

#include "graph.hpp"
#endif
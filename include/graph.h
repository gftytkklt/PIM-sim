#ifndef GRAPH_H
#define GRAPH_H
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <optional>

struct NNkernel {
    std::pair<int,int> wsize;   // (w, h) of kernel
    std::pair<int,int> channel; // (in, out) of stride
    int scaling_factor;         // stride * pooling stride, fmap reducing factor
    std::pair<int,int> depinfo; // (dep_layer, dep_channel_num)
    int fmap_size;              // fmap size(w*h*c)
};

enum class DepType {
    ErrorType,
    Accum,  // intra-layer accumulation
    Prop,   // inter-layer propagation
    // ...
};

struct CNode {
    int layer;                          // Layer index
    // std::pair<int,int> size;            // (WL, BL) of xbar
    int ofmap_size;                     // Ofm size
    std::pair<int,int> id_cin, id_cout; // (cin, cout) channel index
    std::pair<int,int> depinfo;         // (dep_layer, dep_channel_num)
};

std::ostream& operator<<(std::ostream& os, const CNode& cnode);

struct CEdge {
    DepType c_type;
    int datavolume;
};

std::ostream& operator<<(std::ostream& os, const CEdge& cedge);

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

template <typename NodeProperty, typename EdgeProperty>
class BaseGraph {
protected:
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, NodeProperty, EdgeProperty>;
    using UGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, NodeProperty, EdgeProperty>;
    using BiGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, NodeProperty, EdgeProperty>;
    using Node = boost::graph_traits<Graph>::vertex_descriptor;
    using Edge = boost::graph_traits<Graph>::edge_descriptor;

    // default ctor
    BaseGraph() = default;

    // add vertices and edges
    void add_node(const NodeProperty& node_prop, Graph& g) {
        boost::add_vertex(node_prop, g);
        std::cout << "add node" << std::endl;
    }

    void add_edge(int v1, int v2, const EdgeProperty& edge_prop, Graph& g) {
        // auto n1 = boost::vertex(v1, g);
        // auto n2 = boost::vertex(v2, g);
        boost::add_edge(v1, v2, edge_prop, g);
        std::cout << "add edge" << std::endl;
    }

    // remove vertices and edges
    void remove_node(int v, Graph& g) {
        auto ei = edges(g);
        for (auto e = ei.first; e != ei.second; ++e) {
            if (target(*e, g) == v) { // out-edges are deleted automatically
                remove_edge(*e, g);  // delete in-edges mauanlly
            }
        }
        boost::remove_vertex(v, g);
    }

    void remove_edge(int v1, int v2, Graph& g) {
        boost::remove_edge(v1, v2, g);
    }

    void remove_edge(const Edge& e, Graph& g) {
        boost::remove_edge(e, g);
    }

    // graph getter
    virtual const Graph& get_graph() const = 0;

    // vertices and edges getter
    const NodeProperty& get_node_property(int v, const Graph& g) const {
        auto n = boost::vertex(v, g);
        return g[n];
        // return n.m_property.m_value;
        // return g.m_vertices[n].m_property.m_value;
    } 

    const EdgeProperty& get_edge_property(int v1, int v2, const Graph& g) const {
        Edge e;
        bool found;
        boost::tie(e, found) = boost::edge(v1, v2, g);
        if (found) {
            return g[e];
        }
        static const EdgeProperty default_edge_property{};  // 静态常量，避免多次构造
        return default_edge_property;  // 返回默认属性的常量引用
    }

    const EdgeProperty& get_edge_property(const Edge& e, const Graph& g) const {
        return g[e];
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

    void set_edge_property(const Edge& e, const EdgeProperty& edge_prop, Graph& g) {
        g[e] = edge_prop;
    }

    // get adjacent vertices
    std::vector<int> get_adjacent_nodes(int v, const Graph& g) const {
        std::vector<int> adj_nodes;
        typename boost::graph_traits<Graph>::adjacency_iterator ai, ai_end;
        for (boost::tie(ai, ai_end) = boost::adjacent_vertices(v, g); ai != ai_end; ++ai) {
            adj_nodes.push_back(*ai);
        }
        return adj_nodes;
    }

    // deprecated because only out-edges are stored and can be determined by adjacent vertices
    // std::vector<int> get_adjacent_edges(int v, const Graph& g) const {
    //     std::vector<int> adj_edges;
    //     typename boost::graph_traits<Graph>::out_edge_iterator ei, ei_end;
    //     for (boost::tie(ei, ei_end) = boost::out_edges(v, g); ei != ei_end; ++ei) {
    //         // adj_edges.push_back(*ei);
    //         adj_edges.push_back(boost::target(*ei, g));
    //     }
    //     return adj_edges;
    // }

    // analysis func interface
    virtual void analysis() = 0;

    // print graph
    virtual void print_graph_info(const Graph& g) const {
        // traverse all nodes
        for (auto vp = boost::vertices(g); vp.first != vp.second; ++vp.first) {
            auto v = *vp.first;
            std::cout << "Node " << v << ": " << get_node_property(v, g) << std::endl;
        }

        // traverse all edges
        for (auto ep = boost::edges(g); ep.first != ep.second; ++ep.first) {
            auto e = *ep.first;
            std::cout << "Edge (" << boost::source(e, g) << ", " << boost::target(e, g) << "): " 
                      << get_edge_property(e, g) << std::endl;
        }
    }
};

class CGraph : public BaseGraph<CNode, CEdge> {
public:
    CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size);
    void analysis() final;
    const Graph& get_graph() const {
        return cg;
    }
    Graph& get_graph() {
        return cg;
    }
    void print_graph_info() const{
        BaseGraph<CNode, CEdge>::print_graph_info(cg);
    }
private:
    const std::vector<NNkernel>& kernels;
    Graph cg;
    std::pair<int, int> CNode_size; // (W, H) of node
};

class TGraph : public BaseGraph<TNode, TEdge> {
public:
    TGraph(std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of TGraph" << std::endl;
    }
private:
    Graph tg; // T-VDFG
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for T-VDFG inference
};

class HGraph : public BaseGraph<HNode, HEdge> {
public:
    HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of HGraph" << std::endl;
    }
private:
    Graph hg; // HCG
    std::shared_ptr<const TGraph> tg_ref; // T-VDFG for HCG inference
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for HCG inference
};

class DGraph : public BaseGraph<DNode, DEdge> {
public:
    DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg);
    void analysis() override {
        std::cout << "Analysis of DGraph" << std::endl;
    }
private:
    Graph dg; // DHCG
    std::shared_ptr<const HGraph> hg_ref; // HCG for DHCG inference
    std::shared_ptr<const TGraph> tg_ref; // T-VDFG for DHCG inference
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for DHCG inference  
};

// #include "graph.hpp"
#endif
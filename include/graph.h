#ifndef GRAPH_H
#define GRAPH_H
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/properties.hpp>
#include <boost/graph/graph_traits.hpp>
#include <boost/graph/topological_sort.hpp>
#include <iostream>
#include <vector>
#include <memory>
#include <cmath>
#include "mapper.h"
#include "scheduler.h"
#include "util.h"

// Dep info of a kernel dep
struct Depinfo{
    int dep_layer;
    std::pair<int,int> dep_chan;
};

struct NNkernel {
    int layer;
    std::pair<int,int> wsize;   // (w, h) of kernel
    std::pair<int,int> channel; // (in, out) of channel
    std::vector<Depinfo> depinfo; // (dep_layer, dep_channel_num)
    std::pair<int,int> ifmap_size, ofmap_size;  // ofmap size(w, h)
};

enum class DepType {
    ErrorType,
    Accum,  // intra-layer accumulation
    Prop,   // inter-layer propagation
    Mixed,  // tile-level deptype
    // ...
};

struct CNode {
    int layer;                          // Layer inde
    int ifmap_size;                     // Ifm size
    int ofmap_size;                     // Ofm size
    std::pair<int,int> id_cin, id_cout; // (cin, cout) channel index
};

std::ostream& operator<<(std::ostream& os, const CNode& cnode);

struct CEdge {
    DepType c_type;
    std::pair<int,int> channel_id;
    int datavolume;
};

std::ostream& operator<<(std::ostream& os, const CEdge& cedge);

struct TNode {
    std::vector<size_t> cnode_id; // original cnode id
    // std::vector<CNode> super_nodes; // merged cnodes info
    std::set<size_t> parent_id; // inter-layer parent tnode id
    TNode() = default;
    TNode(std::vector<size_t> cnode_id)
    : cnode_id(cnode_id), parent_id{} {}
    // TNode(std::vector<size_t> cnode_id);
};

std::ostream& operator<<(std::ostream& os, const TNode& tnode);

struct TEdge {
    DepType t_type; 
    int accvolume;  
    int propvolume; 
    std::map<int, int> layer_map;// key: layer, value: datavolume
};

std::ostream& operator<<(std::ostream& os, const TEdge& tedge);

struct HNode {
    size_t tnode_id;
    std::pair<int,int> tile_id;
    bool mapped;
    // int ofm_size;
};

std::ostream& operator<<(std::ostream& os, const HNode& hnode);

struct HEdge {
    // std::vector<size_t> path_id;
    std::map<size_t, int> pathset; // (id, datavolume)
    int datavolume;
};

std::ostream& operator<<(std::ostream& os, const HEdge& hedge);

struct DNode {
    size_t tnode_id;
    std::pair<int,int> tile_id;
};

std::ostream& operator<<(std::ostream& os, const DNode& dnode);

using DEdge = HEdge;
// struct DEdge {
//     using PSet = std::set<std::pair<size_t, int>, pair_second_comparator>;
//     PSet pathset; // (id, datavolume)
//     int datavolume;
//     // used only for sched analysis, meanlingless in graph class
//     // double bce;
//     // int congestion_volume;
// };



// std::ostream& operator<<(std::ostream& os, const DEdge& dedge);

template <typename NodeProperty, typename EdgeProperty>
class BaseGraph {
protected:
    using Graph = boost::adjacency_list<boost::vecS, boost::vecS, boost::directedS, NodeProperty, EdgeProperty>;
    using UGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, NodeProperty, EdgeProperty>;
    using BiGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::bidirectionalS, NodeProperty, EdgeProperty>;
    using Node = boost::graph_traits<Graph>::vertex_descriptor;
    using Edge = boost::graph_traits<Graph>::edge_descriptor;
    using UNode = boost::graph_traits<UGraph>::vertex_descriptor;
    using UEdge = boost::graph_traits<UGraph>::edge_descriptor;

    // default ctor
    BaseGraph() = default;

    // add vertices and edges
    template <typename GraphType>
    auto add_node(const NodeProperty& node_prop, GraphType& g) {
        return boost::add_vertex(node_prop, g);
    }

    template <typename GraphType>
    void add_edge(Node v1, Node v2, const EdgeProperty& edge_prop, GraphType& g) {
        boost::add_edge(v1, v2, edge_prop, g);
    }

    // remove vertices and edges
    template <typename GraphType>
    void remove_node(Node v, GraphType& g) {
        auto ei = edges(g);
        for (auto e = ei.first; e != ei.second; ++e) {
            if (target(*e, g) == v) { // out-edges are deleted automatically
                remove_edge(*e, g);  // delete in-edges mauanlly
            }
        }
        boost::remove_vertex(v, g);
    }

    template <typename GraphType>
    void remove_edge(Node v1, Node v2, GraphType& g) {
        boost::remove_edge(v1, v2, g);
    }

    template <typename GraphType, typename EdgeType>
    void remove_edge(const EdgeType& e, GraphType& g) {
        boost::remove_edge(e, g);
    }

    // vertices and edges getter
    template <typename GraphType>
    const auto num_nodes(const GraphType& g) const {
        return boost::num_vertices(g);
    }

    template <typename GraphType>
    const auto num_edges(const GraphType& g) const {
        return boost::num_edges(g);
    }
public:
    template <typename GraphType>
    const NodeProperty& get_node_property(Node v, const GraphType& g) const {
        return g[v];
        // method deprecated but works
        // return g.m_vertices[n].m_property.m_value;
    } 

    template <typename GraphType>
    NodeProperty& get_node_property(Node v, GraphType& g) {
        return g[v];
    }

    template <typename GraphType>
    const EdgeProperty& get_edge_property(Node v1, Node v2, const GraphType& g) const {
        static const EdgeProperty default_edge_property{};
        auto [e, found] = boost::edge(v1, v2, g);
        return found ? g[e] : default_edge_property;
    }

    template <typename GraphType, typename EdgeType>
    const EdgeProperty& get_edge_property(const EdgeType& e, const GraphType& g) const {
        return g[e];
    }
protected:
    // vertices and edges attributes setter
    template <typename GraphType>
    void set_node_property(Node v, const NodeProperty& node_prop, GraphType& g) {
        // Node n = boost::vertex(v, g);
        g[v] = node_prop;
    }

    template <typename GraphType>
    void set_edge_property(Node v1, Node v2, const EdgeProperty& edge_prop, GraphType& g) {
        // Edge e;
        // bool found;
        auto [e, found] = boost::edge(v1, v2, g);
        if (found) {
            g[e] = edge_prop;
        }
    }

    template <typename GraphType, typename EdgeType>
    void set_edge_property(const EdgeType& e, const EdgeProperty& edge_prop, GraphType& g) {
        g[e] = edge_prop;
    }

    template <typename GraphType>
    EdgeProperty& get_edge_property(Node v1, Node v2, GraphType& g) {
        static EdgeProperty default_edge_property{};
        auto [e, found] = boost::edge(v1, v2, g);
        if (found) {
            return g[e];
        } else {
            return default_edge_property;
        }
    }
public:
    // get adjacent vertices
    template <typename GraphType>
    std::vector<Node> get_adjacent_nodes(Node v, const GraphType& g) const {
        std::vector<Node> adj_nodes;
        typename boost::graph_traits<GraphType>::adjacency_iterator ai, ai_end;
        for (boost::tie(ai, ai_end) = boost::adjacent_vertices(v, g); ai != ai_end; ++ai) {
            adj_nodes.push_back(*ai);
        }
        return adj_nodes;
    }

    // deprecated because only out-edges are stored and can be determined by adjacent vertices
    // std::vector<int> get_adjacent_edges(int v, const GraphType& g) const {
    //     std::vector<int> adj_edges;
    //     typename boost::graph_traits<GraphType>::out_edge_iterator ei, ei_end;
    //     for (boost::tie(ei, ei_end) = boost::out_edges(v, g); ei != ei_end; ++ei) {
    //         // adj_edges.push_back(*ei);
    //         adj_edges.push_back(boost::target(*ei, g));
    //     }
    //     return adj_edges;
    // }

    // analysis func interface
    virtual void analysis() = 0;

    // print graph
    template <typename GraphType>
    void print_graph_info(const GraphType& g) const {
        // traverse all nodes
        std::cout << "Node num: " << boost::num_vertices(g) << std::endl;
        for (auto vp = boost::vertices(g); vp.first != vp.second; ++vp.first) {
            auto v = *vp.first;
            std::cout << "Node " << v << ": " << get_node_property(v, g) << std::endl;
        }

        // traverse all edges
        std::cout << "Edge num: " << boost::num_edges(g) << std::endl;
        for (auto ep = boost::edges(g); ep.first != ep.second; ++ep.first) {
            auto e = *ep.first;
            std::cout << "Edge (" << boost::source(e, g) << ", " << boost::target(e, g) << "): " 
                      << get_edge_property(e, g) << std::endl;
        }
    }
};

class CGraph;
class TGraph;
class HGraph;
class DGraph;

class CGraph : public BaseGraph<CNode, CEdge> {
    friend class TGraph;
public:
    // acc cnodes group with in a NN kernel
    struct AccBlk{
        std::vector<Node> vertex_id;
        std::pair<int, int> cout_id;
    };

    // Dep struct for a NN kernel
    struct CDep{
        std::vector<AccBlk> acc_blks;
        int layer;
        std::vector<Depinfo> dep_info;
    };

    CGraph() = default;
    CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size);
    // CGraph(CGraph&& other) noexcept;
    // CGraph& operator=(CGraph&& other) noexcept;

    const Graph& get_graph() const { return cg; }
    Graph& get_graph() { return cg; }
    const auto& get_cdep() const { return cdeps; }
    auto& get_cdep() { return cdeps; }
    const auto& get_depth_map() const { return depth_map; }
    void print_graph_info() const;
private:
    const std::vector<NNkernel> kernels;
    Graph cg;
    std::pair<int, int> CNode_size; // (W, H) of node
    std::vector<CDep> cdeps; // kernel-wise dep list
    std::map<int, int> depth_map; // (layer, depth) for each layer
    // create and connect accblk kernel-wise
    void analysis() override final;
    void create_cnodes();
    void conn_accblk();
    void inter_layer_conn();
};

class TGraph : public BaseGraph<TNode, TEdge> {
    friend class HGraph;
    friend class DGraph;
public:
    using TDep = std::vector<std::set<Node>>; // TNode acctile info
    TGraph() = default;
    TGraph(const CGraph& cg, int tile_xbar_num);
    TGraph(std::shared_ptr<const CGraph> cg, int tile_xbar_num);
    TGraph(std::shared_ptr<const CGraph> cg, int tile_xbar_num, bool map);
    const Graph& get_graph() const { return tg; }
    Graph& get_graph() { return tg; }
    const TDep& get_tdep() const { return tdeps; }
    TDep& get_tdep() { return tdeps; }
    int get_layer(Node tnode) const {
        auto cnode_id = get_node_property(tnode, tg).cnode_id[0];
        return cg_ref->get_node_property(cnode_id, cg_ref->get_graph()).layer;
    }
    void print_graph_info() const;
private:
    Graph tg; // T-VDFG
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for T-VDFG inference
    std::unordered_map<Node, Node> node_map; // map from cnode to tnode
    TDep tdeps;
    int tile_xbar_num; // number of xbar in a tile
    bool mapping_opt = true; // mapping optimization flag, default true
    void analysis() override final;
    void analysis_zigzag(); // for zigzag mapping baseline
    void create_tnodes();
    void create_TDep();
    void inter_tile_conn();
    void update_tedges(Node src_t, Node dst_t, CEdge cedge, int src_layer);
};

class HGraph : public BaseGraph<HNode, HEdge> {
    friend class DGraph;
public:
    HGraph() = default;
    HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, std::pair<int, int> tile_size);
    HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, std::pair<int, int> tile_size, bool map);
    // automatic hardware template generation, tile size generated by algorithm requirement
    HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg);
    const UGraph& get_graph() const { return hg; }
    auto get_shape() const { return tile_size; }
    // for HNode id, xy transformation
    std::pair<int, int> id_to_xy(size_t id) const;
    size_t xy_to_id(std::pair<int, int> xy) const;
    // use tnode to get hnode
    auto get_hnode(Node tnode) const {return mapper.get_core(tnode);}
    // use hnode to get tnode
    auto get_tnode(int x, int y) const {return mapper.get_node(x, y);}
    auto get_tnode(std::pair<int, int> xy) const {return mapper.get_node(xy);}
    // print graph info
    void print_graph_info() const;
private:
    UGraph hg; // HCG
    std::shared_ptr<const TGraph> tg_ref; // T-VDFG for HCG inference
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for HCG inference
    std::pair<int, int> tile_size; // (W, H) of tile array
    std::vector<Path> paths; // path info
    Mapper mapper; // mapper for HCG
    bool mapping_opt = true; // mapping optimization flag, default true
    void analysis() override final;
    void init_hw_setting(); // init hardware template
    void zigzag_mapping(); // zigzag mapping for HCG
    void greedy_mapping(); // map TNode to HNode
    void init_path(); // init XY-routing path
    void add_path(Node src, Node dst, size_t path_index);
    void remove_path(Node src, Node dst, size_t path_index);
};

class DGraph : public BaseGraph<DNode, DEdge> {
public:
    DGraph() = default;
    DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg,std::shared_ptr<const CGraph> cg);
    DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, int pipeline_depth);
    DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, int pipeline_depth, bool sched);
    std::pair<int, int> id_to_xy(size_t id) const {return hg_ref->id_to_xy(id);}
    size_t xy_to_id(std::pair<int, int> xy) const {return hg_ref->xy_to_id(xy);}
    // get path ptr with src tnode id
    std::vector<std::shared_ptr<Path>> get_tpath(size_t src) {
        // auto it = paths.find(src);
        auto it = path_map.find(src);
        return it == path_map.end() ? std::vector<std::shared_ptr<Path>>{} : get_pathset(it->second);
    }
    // std::vector<std::vector<Path>> get_path_segs() const {return path_segs;}
    std::vector<std::vector<int>> get_path_segs() const {return path_segs;}
    std::vector<std::set<int>> get_layer_segs() const {return layer_segs;}
    std::vector<std::shared_ptr<Path>> get_pathset(std::vector<int> path_ids);
    std::vector<std::shared_ptr<Path>> get_pathset(std::vector<int> path_ids) const;
    auto get_congestion_segs() const {return congestion_segs;}
    void print_graph_info() const;
    void print_path_info() const;
private:
    int pipeline_depth; // pipeline depth for DHCG partition
    std::pair<int, int> tile_size; // (W, H) of tile array
    bool sched_opt = true; // scheduling optimization flag, default true
    UGraph sdg; // HCG node and path set
    // std::vector<std::vector<Path>> path_segs; // path subset of each DSeg
    std::vector<std::vector<int>> path_segs; // path subset of each DSeg
    std::vector<std::set<int>> layer_segs; // layer subset of each DSeg
    std::shared_ptr<const HGraph> hg_ref; // HCG for DHCG inference
    std::shared_ptr<const TGraph> tg_ref; // T-VDFG for DHCG inference
    std::shared_ptr<const CGraph> cg_ref; // C-VDFG for DHCG inference
    std::map<size_t, std::vector<int>> tdep_map; // (prop_node, tdeps)
    std::map<int, size_t> harbor_map;
    // std::map<size_t, std::vector<Path>> paths; // path info with src tnode id
    std::map<size_t, std::vector<int>> path_map; // path info with src tnode id
    // std::vector<Path> paths; // path info
    std::vector<std::shared_ptr<Path>> paths; // path info
    std::vector<long long> congestion_segs; // congestion of each seg
    // std::map<size_t, double> congestion_map; // (path_id, congestion)
    // std::vector<Path> final_path; // final path set (poor design)
    Scheduler scheduler;
    auto get_core(size_t node) const {return hg_ref->mapper.get_core(node);}
    auto get_node(int x, int y) const {return hg_ref->mapper.get_node(x, y);}
    auto get_node(std::pair<int, int> xy) const {return hg_ref->mapper.get_node(xy);}
    void analysis() override final;
    void set_harbor();
    void set_sdg();
    void create_DSeg();
    void bce_routing();
    void xy_routing();
    void add_path(const std::shared_ptr<Path> path_ptr);
};

#endif
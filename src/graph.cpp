#include "graph.h"
#include "util.h"
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/graph/betweenness_centrality.hpp>

CGraph::CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size) 
    : cg{}, kernels{kernels}, CNode_size{CNode_size}, dep_infos{} {
    analysis();
}

void CGraph::analysis() {
    create_cnodes();
    conn_accblk();
    inter_layer_conn();
}

void CGraph::create_cnodes() {
    // create cnodes kernel-wise
    for (const auto& i: kernels) {
        // determine in/out chan num of a cnode
        int window_size = i.wsize.first * i.wsize.second;
        int in_chan = (CNode_size.first + window_size - 1) / window_size;
        int out_chan = CNode_size.second;
        // cur layer info
        auto [ker_in, ker_out] = i.channel;
        auto cur_l = i.layer;
        const auto& cur_dep = i.depinfo;
        auto cur_ifm = i.ifmap_size;
        auto cur_ofm = i.ofmap_size;
        // construct nodes under the size constraints of (in_chan, out_chan)
        int co_begin = 0;
        std::vector<AccBlk> accblks{};
        while (co_begin < ker_out) {
            // create the segmentation along the output channel dimension
            int co_end = std::min(co_begin + out_chan, ker_out);
            auto co_id = std::make_pair(co_begin+1, co_end);
            int ci_begin = 0;
            std::vector<Node> accblk{};
            while(ci_begin < ker_in) {
                // create the segmentation along the input channel dimension
                int ci_end = std::min(ci_begin + in_chan, ker_in);
                auto ci_id = std::make_pair(ci_begin+1, ci_end);
                // instantiate a cnode
                // TODO: impl ofm calculation
                int cur_fmap = -1;
                // last accblk: generate ofm
                if(ci_end == ker_in) {
                    cur_fmap = cur_ofm.first * cur_ofm.second * (co_end - co_begin);
                }
                // other accblk: generate ifm
                else {
                    cur_fmap = cur_ifm.first * cur_ifm.second * (co_end - co_begin);
                }
                auto cnode = CNode{cur_l, cur_fmap, ci_id, co_id};
                // add node to accblk
                accblk.emplace_back(add_node(cnode, cg));
                // update ci_begin
                ci_begin = ci_end;
            }
            // add accblk to accblks group
            accblks.emplace_back(AccBlk{accblk, co_id});
            // update co_begin
            co_begin = co_end;
        }
        // add accblks gropu to dep_infos
        dep_infos.emplace_back(CDep{accblks, cur_l, cur_dep});
    }
}

void CGraph::conn_accblk() {
    for (const auto& v : dep_infos) {
        for (const auto& blk : v.acc_blks) {
            const auto vertexs = blk.vertex_id;
            auto node_num = vertexs.size();
            // skip conn for empty(should not happen) or single node group
            if (node_num <= 1) {continue;}
            const auto cur_src = get_node_property(vertexs[0], cg);
            const auto cur_ofm = cur_src.ofmap_size;
            const auto cur_chan = cur_src.id_cout;
            // TODO: impl fmap cal
            for (int i=0;i<node_num-1;i++) {
                // since input channel are impl in order
                // and accum order is commutable
                add_edge(vertexs[i], vertexs[i+1], CEdge{DepType::Accum,cur_chan,cur_ofm}, cg);
            }
        }
    }
}

void CGraph::inter_layer_conn() {
    for (const auto& v : dep_infos) {
        // get acc blks and dep info
        const auto& deps = v.dep_info;
        for (const auto& src : v.acc_blks) {
            // get cur data volume and output channel id
            auto src_node = src.vertex_id.back();
            const auto src_property = get_node_property(src_node,cg);
            auto cur_datavolume = src_property.ofmap_size;
            // auto cur_cin_num = src_property.id_cout.second - src_property.id_cout.first + 1;
            auto co_src = src.cout_id;
            auto cur_cout_num = co_src.second - co_src.first + 1;
            // get dst node
            for (const auto& dep : deps) {
                // get dep layer info struct
                for (const auto& dst_layer : dep_infos[dep.dep_layer].acc_blks) {
                    for (const auto& dst_node : dst_layer.vertex_id) {
                        auto ci_dst = get_node_property(dst_node, cg).id_cin;
                        // get intersection num
                        auto start = std::max(co_src.first, ci_dst.first);
                        auto end = std::min(co_src.second, ci_dst.second);
                        if(start <= end) {
                            auto overlap = end - start + 1;
                            // add edge and corresponding data volume
                            add_edge(src_node, dst_node, CEdge{DepType::Prop, {start,end}, cur_datavolume * overlap / cur_cout_num}, cg);
                        }
                    }
                }
            }
        }
    }
}

void CGraph::print_graph_info() const{ 
    std::cout << "Graph info:" << std::endl;
    BaseGraph<CNode, CEdge>::print_graph_info(cg);
    std::cout << "AccBlk info:" << std::endl;
    for(const auto&v : dep_infos) {
        std::cout << "Layer: " << v.layer << std::endl;
        for(const auto& blk : v.acc_blks) {
            std::cout << "AccBlk: " << blk.cout_id.first << " - " << blk.cout_id.second << std::endl;
            for(const auto& node : blk.vertex_id) {
                std::cout << "Node: " << node << std::endl;
            }
        }
    }
}

TGraph::TGraph(const CGraph& cg, int tile_xbar_num) 
    : tg{}, cg_ref{std::make_shared<const CGraph>(cg)}, tile_xbar_num{tile_xbar_num} {
    analysis();
}

TGraph::TGraph(std::shared_ptr<const CGraph> cg, int tile_xbar_num) 
    : tg{}, cg_ref{cg}, tile_xbar_num{tile_xbar_num} {
    analysis();
}

void TGraph::analysis() {
    create_tnodes();
    create_TDep();
    inter_tile_conn();
}

void TGraph::create_tnodes() {
    const auto& cg = cg_ref->get_graph();
    const auto& cdeps = cg_ref->get_dep_infos();
    for (const auto& cdep: cdeps) {
        // get cnode size
        // BL split
        auto col_size = cdep.acc_blks.size();
        // WL split
        auto row_size = cdep.acc_blks[0].vertex_id.size();
        // uniformsplit
        auto split = uniformsplit(row_size, col_size, tile_xbar_num);
        // create TNode
        for(const auto& cgroup : split) {
            // get cnode id of cur tnode
            std::vector<size_t> cnode_id{};
            // supernode info can be generated here
            // do not generate ofm info here
            CNode supernode{};
            bool empty = true;
            auto mergenode = [&](Node node_id) {
                auto cnode = cg_ref-> get_node_property(node_id, cg);
                if(empty) {
                    supernode = cnode;
                    empty = false;
                } else {
                    auto cout_num = supernode.id_cout.second - supernode.id_cout.first + 1;
                    supernode.id_cin.first = std::min(supernode.id_cin.first, cnode.id_cin.first);
                    supernode.id_cin.second = std::max(supernode.id_cin.second, cnode.id_cin.second);
                    supernode.id_cout.first = std::min(supernode.id_cout.first, cnode.id_cout.first);
                    supernode.id_cout.second = std::max(supernode.id_cout.second, cnode.id_cout.second);
                }
            };
            for(const auto& i : cgroup) {
                cnode_id.emplace_back(cdep.acc_blks[i.second].vertex_id[i.first]);
                mergenode(cnode_id.back());
            }
            // clear invalid supernode info
            supernode.ofmap_size = 0;
            auto tnode_id = add_node(TNode{cnode_id, std::vector<CNode>{supernode}}, tg);
            // build node map, i is unique
            for (const auto& i : cnode_id) {
                node_map.emplace(i, tnode_id);
            }
        }
    }
}

void TGraph::create_TDep() {
    // get cdep info
    const auto& cdeps = cg_ref->get_dep_infos();
    // create TDep
    for (const auto& cdep : cdeps) {
        // get cnode accblk index
        const auto& accblks = cdep.acc_blks;
        for(auto accblk : accblks) {
            std::set<Node> tdep{};
            for(auto node : accblk.vertex_id) {
                tdep.emplace(node_map[node]);
            }
            tdeps.emplace_back(tdep);
        }
        
    }
}

// merge inter-tile edges
void TGraph::inter_tile_conn() {
    // find inter-tile c-edges
    const auto& cg = cg_ref->get_graph();
    using EdgeElem = std::map<Node,std::vector<CEdge>>;
    using EdgeMap = std::map<std::pair<Node, Node>, EdgeElem>;
    EdgeMap edge_map{};
    // traverse edges
    for (const auto& e : boost::make_iterator_range(edges(cg))) {
        // CEdge info
        auto src = boost::source(e, cg);
        auto dst = boost::target(e, cg);
        // TNode info
        Node src_t = node_map[src];
        Node dst_t = node_map[dst];
        // find inter-tile edges and update tedges
        if(src_t != dst_t) {
            // get inter-tile cedge property
            auto cedge = cg_ref->get_edge_property(e, cg);
            // update edgemap
            edge_map[std::make_pair(src_t, dst_t)][src].emplace_back(cedge);
            // edge_map[std::make_pair(src_t, dst_t)][src].emplace(cedge);
            // edge_map[std::make_pair(src_t, dst_t)].emplace_back(EdgeElem{src, cedge});
            // update_tedges(src_t, dst_t, cedge);
        }
    }
    // traverse tnode edge_map
    for (auto& [key, val] : edge_map) {
        // traverse edges with same src cnode
        for (auto& [src, cedges] : val) {
            // sort cedges by channel start id
            std::sort(cedges.begin(), cedges.end(), [](CEdge& a, CEdge& b) {
                return a.channel_id.first < b.channel_id.first;
            });
            // merge cedges
            std::vector<CEdge> merged_edges{};
            // add first edge
            merged_edges.emplace_back(cedges[0]);
            // merge edge info
            for (int i = 1; i < cedges.size(); i++) {
                auto& cur_edge = merged_edges.back();
                // only update non-overlapping edgeinfo
                // case1: has overlap
                if (cedges[i].channel_id.first <= cur_edge.channel_id.second) {
                    auto unique_num = UniqueElements(cur_edge.channel_id, cedges[i].channel_id);
                    // unique channel must extend the end index
                    cur_edge.channel_id.second += unique_num;
                    // update datavolume
                    cur_edge.datavolume += cedges[i].datavolume * unique_num / (cedges[i].channel_id.second - cedges[i].channel_id.first + 1);
                }
                // case2: no overlap
                else {
                    merged_edges.emplace_back(cedges[i]);
                }
            }
            // update tedges
            for (auto& edge : merged_edges) {
                update_tedges(key.first, key.second, edge);
            }
        }
    }
}

void TGraph::update_tedges(Node src_t, Node dst_t, CEdge cedge) {
    auto cur_tedge = get_edge_property(src_t, dst_t, tg);
    auto acc_num = cedge.datavolume * (cedge.c_type == DepType::Accum);
    auto prop_num = cedge.datavolume * (cedge.c_type == DepType::Prop);
    // if empty, create new edge
    // std::cout << cedge << std::endl;
    if (cur_tedge.t_type == DepType::ErrorType) {
        // std::cout << "Create new edge" << std::endl;
        add_edge(src_t, dst_t, TEdge{cedge.c_type, acc_num, prop_num}, tg);
    }
    else {
        // update edge
        if(cur_tedge.t_type != cedge.c_type) {
            cur_tedge.t_type = DepType::Mixed;
        }
        cur_tedge.accvolume += acc_num;
        cur_tedge.propvolume += prop_num;
        set_edge_property(src_t, dst_t, cur_tedge, tg);
    }
    // std::cout << cur_tedge << std::endl;
}

void TGraph::print_graph_info() const { 
    std::cout << "Graph info:" << std::endl;
    BaseGraph<TNode, TEdge>::print_graph_info(tg);
    std::cout << "TDep info:" << std::endl;
    int i = 0;
    for(const auto& v : tdeps) {
        std::cout << "TDep: " << ++i << std::endl;
        for(const auto& node : v) {
            std::cout << "Node: " << node << " ";
        }
        std::cout << std::endl;
    }
}

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, std::pair<int, int> tile_size)
    : hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{tile_size} {
    if (tile_size.first * tile_size.second < tg_ref->num_nodes(tg_ref->get_graph())) {
        throw std::invalid_argument("Tile size does not match the number of nodes in the TGraph.");
    }
    analysis();
}

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg)
    : hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{} {
    auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
    auto tile_x = static_cast<int>(std::ceil(std::sqrt(num_tile)));
    tile_size = std::make_pair(tile_x, tile_x);
    analysis();
}

void HGraph::analysis() {
    std::cout << "Analysis HGraph" << std::endl;
}

void CGraph::debug() {
    // test BGL builtin algorithm
    auto coords_map = boost::get(&CNode::id_cin, cg);
    for(auto v : boost::make_iterator_range(vertices(cg))) {
        auto sth = coords_map[v]; // attribute getter
        if(sth == std::make_pair(1,384)) {
            std::cout << "Find Node: " << v << std::endl;
            std::cout << get_node_property(v,cg) << std::endl;
            cg[v].ofmap_size += 1; // setter
            std::cout << get_node_property(v,cg) << std::endl;
        }
    }
    std::vector<int> dist(boost::num_vertices(cg));
    std::vector<Node> pred(num_vertices(cg));
    auto weight_map = boost::get(&CEdge::datavolume, cg);
    auto source = boost::vertex(0, cg);

    boost::dijkstra_shortest_paths(cg, source, 
            boost::predecessor_map(&pred[0])
            .distance_map(&dist[0])
            .weight_map(weight_map)
    );

    std::cout << "Distances from node 1:" << std::endl;
    for (size_t i = 0; i < dist.size(); ++i) {
        std::cout << "Node " << i + 1 << ": " << dist[i] << std::endl;
    }

    // output paths
    std::cout << "Paths:" << std::endl;
    for (size_t i = 0; i < pred.size(); ++i) {
        std::cout << "Node " << i + 1 << ": ";
        if (pred[i] != boost::graph_traits<Graph>::null_vertex()) {
            std::cout << pred[i] + 1 << std::endl;  // unexpected output
        } else {
            std::cout << "No predecessor (source node)" << std::endl;
        }
    }
}

void TGraph::debug() {
    // test bce func here?
}

std::ostream& operator<<(std::ostream& os, const CNode& cnode) {
    os << "Layer: " << cnode.layer << std::endl;
    os << "Ofmap size: " << cnode.ofmap_size << std::endl;
    os << "In Channel index: (" << cnode.id_cin.first << ", " << cnode.id_cin.second << ")" << std::endl;
    os << "Out Channel index: (" << cnode.id_cout.first << ", " << cnode.id_cout.second << ")" << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const CEdge& cedge) {
    os << "Dependency type: ";
    switch (cedge.c_type) {
        case DepType::Accum:
            os << "Accumulation";
            break;
        case DepType::Prop:
            os << "Propagation";
            break;
        default:
            os << "Unknown";
            break;
    }
    os << std::endl;
    os << "Channel id: (" << cedge.channel_id.first << ", " << cedge.channel_id.second << ")" << std::endl;
    os << "Data volume: " << cedge.datavolume << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const TNode& tnode) {
    os << "CNode id: ";
    for (const auto& i : tnode.cnode_id) {
        os << i << " ";
    }
    os << std::endl;
    os << "Super nodes: " << std::endl;
    for (const auto& i : tnode.super_nodes) {
        os << i << std::endl;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const TEdge& tedge) {
    os << "Dependency type: ";
    switch (tedge.t_type) {
        case DepType::Accum:
            os << "Accumulation";
            break;
        case DepType::Prop:
            os << "Propagation";
            break;
        case DepType::Mixed:
            os << "Mixed";
            break;
        default:
            os << "Unknown";
            break;
    }
    os << std::endl;
    os << "Total Data volume: " << tedge.accvolume + tedge.propvolume << std::endl;
    os << "Accumulation Data volume: " << tedge.accvolume << std::endl;
    os << "Propagation Data volume: " << tedge.propvolume << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HNode& hnode) {
    os << "TNode id: " << hnode.tnode_id << std::endl;
    os << "Tile id: (" << hnode.tile_id.first << ", " << hnode.tile_id.second << ")" << std::endl;
    os << "Ofmap size: " << hnode.ofm_size << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HEdge& hedge) {
    os << "Path id: ";
    for (const auto& i : hedge.path_id) {
        os << i << " ";
    }
    os << std::endl;
    os << "Data volume: " << hedge.datavolume << std::endl;
    return os;
}
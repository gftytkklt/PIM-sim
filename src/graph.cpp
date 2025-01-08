#include "graph.h"
#include "util.h"
// #include <boost/graph/dijkstra_shortest_paths.hpp>
// #include <boost/graph/betweenness_centrality.hpp>

CGraph::CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size) 
    : cg{}, kernels{kernels}, CNode_size{CNode_size}, cdeps{} {
    analysis();
    std::cout << "CGraph created" << std::endl;
}

// CGraph::CGraph(CGraph&& other) noexcept
//     : cg(std::move(other.cg)),
//       kernels(std::move(other.kernels)),
//       CNode_size(std::move(other.CNode_size)),
//       cdeps(std::move(other.cdeps)) {}

// CGraph& CGraph::operator=(CGraph&& other) noexcept {
//     if (this != &other) {
//         cg = std::move(other.cg);
//         kernels = std::move(other.kernels);
//         CNode_size = std::move(other.CNode_size);
//         cdeps = std::move(other.cdeps);
//     }
//     return *this;
// }

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
                int node_ifm = -1;
                int node_ofm = -1;
                // last accblk: generate ofm
                node_ifm = cur_ifm.first * cur_ifm.second * (ci_end - ci_begin);
                if(ci_end == ker_in) {
                    node_ofm = cur_ofm.first * cur_ofm.second * (co_end - co_begin);
                }
                // other accblk: generate ifm
                else {
                    node_ofm = cur_ifm.first * cur_ifm.second * (co_end - co_begin);
                }
                auto cnode = CNode{cur_l, node_ifm, node_ofm, ci_id, co_id};
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
        // add accblks gropu to cdeps
        cdeps.emplace_back(CDep{accblks, cur_l, cur_dep});
    }
}

void CGraph::conn_accblk() {
    for (const auto& v : cdeps) {
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
    for (const auto& v : cdeps) {
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
                // skip output dep, represent by -1
                if (dep.dep_layer == -1) {continue;}
                // get dep layer info struct
                for (const auto& dst_layer : cdeps[dep.dep_layer].acc_blks) {
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
    for(const auto&v : cdeps) {
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

TGraph::TGraph(std::shared_ptr<const CGraph> cg, int tile_xbar_num, bool map) 
    : tg{}, cg_ref{cg}, tile_xbar_num{tile_xbar_num}, mapping_opt{map} {
    analysis();
}

void TGraph::analysis() {
    if(mapping_opt) {
        create_tnodes();
    }
    else{
        analysis_zigzag();
    }
    // create_tnodes();
    create_TDep();
    inter_tile_conn();
}

void TGraph::analysis_zigzag() {
    // merge cnodes sequentially & layer-wise
    std::vector<size_t> cnode_id{};
    for(const auto& cdep : cg_ref->get_cdep()) {
        for(const auto& accblk : cdep.acc_blks) {
            for(const auto& node : accblk.vertex_id) {
                if(cnode_id.size() == tile_xbar_num) {
                    // merge cur group into a tnode
                    auto tnode_id = add_node(TNode{cnode_id}, tg);
                    // build node map, i is unique
                    for (const auto& i : cnode_id) {
                        node_map.emplace(i, tnode_id);
                    }
                    cnode_id.clear();
                }
                cnode_id.emplace_back(node);
            }
        }
        // merge remaining cnodes
        if(!cnode_id.empty()) {
            auto tnode_id = add_node(TNode{cnode_id}, tg);
            // build node map, i is unique
            for (const auto& i : cnode_id) {
                node_map.emplace(i, tnode_id);
            }
            cnode_id.clear();
        }
    }
    
}

void TGraph::create_tnodes() {
    const auto& cg = cg_ref->get_graph();
    const auto& cdeps = cg_ref->get_cdep();
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
            // CNode supernode{};
            // bool empty = true;
            // auto mergenode = [&](Node node_id) {
            //     auto cnode = cg_ref-> get_node_property(node_id, cg);
            //     if(empty) {
            //         supernode = cnode;
            //         empty = false;
            //     } else {
            //         auto cout_num = supernode.id_cout.second - supernode.id_cout.first + 1;
            //         supernode.id_cin.first = std::min(supernode.id_cin.first, cnode.id_cin.first);
            //         supernode.id_cin.second = std::max(supernode.id_cin.second, cnode.id_cin.second);
            //         supernode.id_cout.first = std::min(supernode.id_cout.first, cnode.id_cout.first);
            //         supernode.id_cout.second = std::max(supernode.id_cout.second, cnode.id_cout.second);
            //     }
            // };
            for(const auto& i : cgroup) {
                cnode_id.emplace_back(cdep.acc_blks[i.second].vertex_id[i.first]);
                // mergenode(cnode_id.back());
            }
            // clear invalid supernode info
            // supernode.ofmap_size = 0;
            auto tnode_id = add_node(TNode{cnode_id}, tg);
            // build node map, i is unique
            for (const auto& i : cnode_id) {
                node_map.emplace(i, tnode_id);
            }
        }
    }
}

void TGraph::create_TDep() {
    // get cdep info
    const auto& cdeps = cg_ref->get_cdep();
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
    using EdgeElem = std::unordered_map<Node,std::vector<CEdge>>;
    using EdgeMap = std::unordered_map<std::pair<Node, Node>, EdgeElem, pair_hash>;
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
        // update tnode parent info
        auto& tnode = get_node_property(key.second, tg);
        if(get_edge_property(key.first, key.second, tg).t_type == DepType::Prop) {
            tnode.parent_id.emplace_back(key.first);
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
    : hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{tile_size}, mapper{}{
    if (tile_size.first * tile_size.second < tg_ref->num_nodes(tg_ref->get_graph())) {
        // throw std::invalid_argument("Tile size does not match the number of nodes in the TGraph.");
        auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
        auto tile_x = static_cast<int>(std::ceil(std::sqrt(num_tile)));
        this->tile_size = std::make_pair(std::max(tile_size.first,tile_x), std::max(tile_size.second,tile_x));
        std::cout << "Reshape to " << this->tile_size.first << " x " << this->tile_size.second << " to fit algorithm size" << std::endl;
    }
    else {
        std::cout << "Tile size: " << this->tile_size.first << " x " << this->tile_size.second << std::endl;
    }
    mapper = Mapper{this->tile_size};
    analysis();
}

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg)
    : hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{}, mapper{} {
    auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
    auto tile_x = static_cast<int>(std::ceil(std::sqrt(num_tile)));
    tile_size = std::make_pair(tile_x, tile_x);
    mapper = Mapper{tile_size};
    analysis();
}

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, std::pair<int, int> tile_size, bool map)
    : hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{tile_size}, mapper{}, mapping_opt{map} {
    if (tile_size.first * tile_size.second < tg_ref->num_nodes(tg_ref->get_graph())) {
        // throw std::invalid_argument("Tile size does not match the number of nodes in the TGraph.");
        auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
        auto tile_x = static_cast<int>(std::ceil(std::sqrt(num_tile)));
        this->tile_size = std::make_pair(std::max(tile_size.first,tile_x), std::max(tile_size.second,tile_x));
        std::cout << "Reshape to " << this->tile_size.first << " x " << this->tile_size.second << " to fit algorithm size" << std::endl;
    }
    else {
        std::cout << "Tile size: " << this->tile_size.first << " x " << this->tile_size.second << std::endl;
    }
    mapper = Mapper{this->tile_size};
    analysis();
}

void HGraph::analysis() {
    init_hw_setting();
    if(mapping_opt){
        greedy_mapping();
    }
    else{
        zigzag_mapping();
    }
    // greedy_mapping();
    init_path();
}

void HGraph::init_hw_setting() {
    // init tile array
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            auto hnode = HNode{0, std::make_pair(i, j), false};
            // add_node(hnode, hg);
            auto hid = add_node(hnode, hg);
            // std::cout << "Add node: " << i << "," << j << "to" << hid << std::endl;
        }
    }
    // 2D-mesh connection
    
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            if (i > 0) {
                add_edge(i * tile_size.second + j, (i - 1) * tile_size.second + j, HEdge{{}, 0}, hg);
            }
            if (j > 0) {
                add_edge(i * tile_size.second + j, i * tile_size.second + j - 1, HEdge{{}, 0}, hg);
            }
        }
    }
}

void HGraph::zigzag_mapping() {
    auto tg = tg_ref->get_graph();
    mapper.zigzag_mapping(num_nodes(tg));
    for (size_t i = 0; i < num_nodes(tg); ++i) {
        // auto tnode = tg_ref->get_node_property(i, tg);
        auto hnode = mapper.get_core(i);
        auto hid = xy_to_id(hnode);
        set_node_property(hid, HNode{i, hnode, true}, hg);
    }
}

void HGraph::greedy_mapping() {
    // TNode and TDep info
    const auto& tg = tg_ref->get_graph();
    const auto& tdeps = tg_ref->get_tdep();
    // map tgrp to HNodes
    int i = 0;
    for (const auto& tdep : tdeps) {
        std::set<size_t> dep_set{};
        // get dep set
        for (const auto& node : tdep) {
            auto parents = tg_ref->get_node_property(node, tg).parent_id;
            for (const auto& parent : parents) {
                dep_set.insert(parent);
            }
        }
        // get inter-layer child(guaranteed by reverse begin)
        auto child_set = tg_ref->get_adjacent_nodes(*tdep.rbegin(),tg);
        // print child set
        // std::cout << "Child set of TDep " << i++ << ": ";
        // for (const auto& child : child_set) {
        //     std::cout << child << " ";
        // }
        // std::cout << std::endl;
        dep_set.insert(child_set.begin(), child_set.end());
        // print dep_set for checking
        // std::cout << "Dep set of TDep " << i++ << ": ";
        // for (const auto& i : dep_set) {
        //     std::cout << i << " ";
        // }
        // std::cout << std::endl;
        mapper.map_group(tdep, dep_set);
    }
    // update HGraph
    for (size_t i = 0; i < num_nodes(tg); ++i) {
        // auto tnode = tg_ref->get_node_property(i, tg);
        auto hnode = mapper.get_core(i);
        auto hid = xy_to_id(hnode);
        set_node_property(hid, HNode{i, hnode, true}, hg);
    }
}

void HGraph::init_path() {
    // get TGraph
    const auto& tg = tg_ref->get_graph();
    // Traverse TEdges
    for (const auto& e : boost::make_iterator_range(edges(tg))) {
        auto src = boost::source(e, tg);
        auto dst = boost::target(e, tg);
        auto tedge = tg_ref->get_edge_property(e, tg);
        auto datavolume = tedge.accvolume + tedge.propvolume;
        // get src and dst HNode
        auto src_h = mapper.get_core(src);
        auto dst_h = mapper.get_core(dst);
        // get path
        auto path = XYinit(src_h, dst_h);
        // add path to paths
        // paths.push_back(Path{path, datavolume});
        // auto path_index = paths.size() - 1;
        auto path_index = paths.size();
        paths.push_back(Path{path_index,src_h, dst_h, path, datavolume});
        // std::cout << "map Tpath: " << src << " -> " << dst << " Path: " << path_index << " Volume: " << datavolume << std::endl;
        // std::cout << "Hpath: " << src_h.first << "," << src_h.second << " -> " << dst_h.first << "," << dst_h.second << std::endl;
        // add path to HGraph
        for (int i = 0; i < path.size() - 1; i++) {
            auto src_id = xy_to_id(path[i]);
            auto dst_id = xy_to_id(path[i + 1]);
            add_path(src_id, dst_id, path_index);
        }
        // std::cout << std::endl;
    }
}

void HGraph::add_path(Node src, Node dst, size_t path_index) {
    auto hedge = get_edge_property(src, dst, hg);
    auto datavolume = paths[path_index].datavolume;
    // std::cout << "Add path: " << src << " -> " << dst << " Path: " << path_index << " Volume: " << datavolume << std::endl;
    // std::cout << "Path before: " << hedge;
    // hedge.pathset.insert({path_index, datavolume});
    hedge.pathset[path_index] = datavolume;
    hedge.datavolume += datavolume;
    // std::cout << "Path after: " << hedge << std::endl;
    set_edge_property(src, dst, hedge, hg);
}

void HGraph::remove_path(Node src, Node dst, size_t path_index) {
    auto hedge = get_edge_property(src, dst, hg);
    auto datavolume = paths[path_index].datavolume;
    // hedge.pathset.erase({path_index, datavolume});
    hedge.pathset.erase(path_index);
    hedge.datavolume -= datavolume;
    set_edge_property(src, dst, hedge, hg);
}

std::pair<int, int> HGraph::id_to_xy(size_t id) const {
    return hg[id].tile_id;
}

size_t HGraph::xy_to_id(std::pair<int, int> xy) const {
    return xy.first * tile_size.second + xy.second;
}

void HGraph::print_graph_info() const {
    std::cout << "HGraph info:" << std::endl;
    BaseGraph<HNode, HEdge>::print_graph_info(hg);
}

DGraph::DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg,std::shared_ptr<const CGraph> cg)
    : hg_ref{hg}, tg_ref{tg}, cg_ref{cg}, pipeline_depth{1}, tile_size{hg->tile_size}, scheduler{hg->tile_size} {
    analysis();
}

DGraph::DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, int pipeline_depth)
    : hg_ref{hg}, tg_ref{tg}, cg_ref{cg}, pipeline_depth{pipeline_depth}, tile_size{hg->tile_size}, scheduler{hg->tile_size} {
    analysis();
}

DGraph::DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, int pipeline_depth, bool sched)
    : hg_ref{hg}, tg_ref{tg}, cg_ref{cg}, pipeline_depth{pipeline_depth}, tile_size{hg->tile_size}, scheduler{hg->tile_size}, sched_opt{sched} {
    analysis();
}

void DGraph::analysis() {
    // segment DHCG
    set_harbor();
    set_sdg();
    create_DSeg();
    // std::cout << "before" << std::endl;
    // print_path_info();
    if (sched_opt) {
        bce_routing();
        // std::cout << "after" << std::endl;
        // print_path_info();
    }
}

void DGraph::set_harbor() {
    // get TDep
    const auto& tdeps = tg_ref->get_tdep();
    // set harbor node for each tdep
    // harbor node place accblk.rbegin()
    // get last node of each tdep
    for (auto i = 0;i<tdeps.size();i++) {
        auto tdep = tdeps[i];
        auto last_node = *tdep.rbegin();
        tdep_map[last_node].push_back(i);
        harbor_map[i] = last_node;
    }
    // print old harbor map
    // for (const auto& [key, val] : harbor_map) {
    //     std::cout << "TDep: " << key << " Harbor: " << val << std::endl;
    // }
    // update harbor node info
    for (const auto& [key, val] : tdep_map) {
        // only standalone tdep need to be updated
        if(val.size() == 1) {
            auto tdep_id = val[0];
            auto tnodes_id = tdeps[tdep_id];
            std::vector<std::pair<int, int>> node_list;
            node_list.reserve(tnodes_id.size());
            std::transform(tnodes_id.begin(), tnodes_id.end(), std::back_inserter(node_list), [&](Node i) {
                return get_core(i);
            });
            // get harbor node
            auto harbor_loc = get_median_point(node_list);
            // std::cout << "harbor loc: " << harbor_loc.first << "," << harbor_loc.second << std::endl;
            // swap harbor_node with last node
            auto harbor_loc_id = get_node(harbor_loc);
            harbor_map[tdep_id] = harbor_loc_id;
        }
    }
    // print new harbor map
    // for (const auto& [key, val] : harbor_map) {
    //     std::cout << "TDep: " << key << " Harbor: " << val << std::endl;
    // }
}
/**
 * @brief set sdg node and (tnode, {path}) map only
 * 
 */
void DGraph::set_sdg() {
    // init node based on hg
    const auto& hg = hg_ref->get_graph();
    for (size_t i = 0; i < num_nodes(hg); i++) {
        auto hnode = hg_ref->get_node_property(i, hg);
        auto tnode_id = hnode.tnode_id;
        auto tile_id = hnode.tile_id;
        add_node(DNode{tnode_id, tile_id}, sdg);
    }
    // init interconnection: 2D mesh currently
    for (int i = 0; i < tile_size.first; i++) {
        for (int j = 0; j < tile_size.second; j++) {
            if (i > 0) {
                add_edge(i * tile_size.second + j, (i - 1) * tile_size.second + j, DEdge{{}, 0}, sdg);
            }
            if (j > 0) {
                add_edge(i * tile_size.second + j, i * tile_size.second + j - 1, DEdge{{}, 0}, sdg);
            }
        }
    }
    // init scheduler, impl at init now
    // scheduler = Scheduler{sdg, tile_size};
    // init pathset based on hg and harbor node
    const auto& tdeps = tg_ref->get_tdep();
    const auto& tg = tg_ref->get_graph();
    // inter-layer tedge
    size_t path_id = 0;
    for (const auto& [harbor_id, tdep_ids] : tdep_map) {
        std::vector<std::size_t> child_id;
        // use tdep.rbegin to get parent node of inter-layer edge
        size_t tdep_id;
        if(tdep_ids.size() == 1) {
            // get child layer node
            auto tdep = tdeps[tdep_ids[0]];
            tdep_id = *tdep.rbegin();
        }
        else {
            tdep_id = harbor_id;
        }
        // get inter-layer child by tdep_id
        // even if tdep_id has intra-layer child, it will work correctly
        child_id = tg_ref->get_adjacent_nodes(tdep_id, tg);
        // append path
        for (const auto& child : child_id) {
            // get data volume
            const auto& edge = tg_ref->get_edge_property(tdep_id, child, tg);
            auto datavolume = edge.accvolume + edge.propvolume;
            // get src and dst node
            auto src_tile = get_core(harbor_id);
            auto dst_tile = get_core(child);
            auto path = XYinit(src_tile, dst_tile);
            // add path to paths
            // paths.push_back(Path{path, datavolume});
            paths[harbor_id].push_back(Path{path_id++, src_tile, dst_tile, path, datavolume});
        }
        // intra-layer tedge
        for (const auto& tdep_elem : tdep_ids) {
            const auto& cur_tdep = tdeps[tdep_elem];
            // get acc edge info if exist
            if(cur_tdep.size() > 1) {
                auto first_node = *cur_tdep.begin();
                auto second_node = *std::next(cur_tdep.begin());
                // get data volume
                const auto& edge = tg_ref->get_edge_property(first_node, second_node, tg);
                auto datavolume = edge.accvolume + edge.propvolume;
                for (auto it = cur_tdep.begin(); it != cur_tdep.end(); it++) {
                    auto src = *it;
                    // acc edge if not harbor node
                    if (src != harbor_id) {
                        auto src_tile = get_core(src);
                        auto dst_tile = get_core(harbor_id);
                        auto path = XYinit(src_tile, dst_tile);
                        // add path to paths
                        // paths.push_back(Path{path, datavolume});
                        paths[src].push_back(Path{path_id++, src_tile, dst_tile, path, datavolume});
                    }
                }
            }
        }
    }
}

void DGraph::create_DSeg() {
    // get TNode layer
    const auto& tg = tg_ref->get_graph();
    std::vector<int> layer(num_nodes(tg), 1);
    // std::cout << "Layer info:" << layer.size() << std::endl;
    // std::map<int, std::vector<size_t>> layer_map;
    // update layer info
    std::vector<size_t> topo_order;
    try {
        boost::topological_sort(tg, std::back_inserter(topo_order));
    }
    catch(boost::not_a_dag& e) {
        std::cerr << "Not a DAG!" << std::endl;
        return;
    }
    std::reverse(topo_order.begin(), topo_order.end());

    // only prop edge update layer info
    for(auto v : topo_order) {
        // root is already 1
        if(!tg[v].parent_id.empty()) {
            int max_layer = 0;
            for (const auto& parent : tg[v].parent_id) {
                max_layer = std::max(max_layer, layer[parent]);
            }
            layer[v] = max_layer + 1;
        }
    }

    // create segment
    auto layer_num = *std::max_element(layer.begin(), layer.end());
    const auto& hg = hg_ref->get_graph();
    // if depth = 0, add standalone if branch to impl
    for (int i = 1; i < layer_num; i += pipeline_depth) {
        auto dst_layer = std::min(i + pipeline_depth, layer_num);
        std::vector<Path> path_seg{};
        for (size_t id = 0; id < layer.size(); id++) {
            if (layer[id] >= i && layer[id] < dst_layer) {
                // append edge to path_seg
                auto tnode_id = topo_order[id];
                auto pathset = paths[tnode_id];
                path_seg.insert(path_seg.end(), 
                std::make_move_iterator(pathset.begin()),
                std::make_move_iterator(pathset.end()));
            }
        }
        path_segs.push_back(path_seg);
    }
}
/**
 * @brief use path_seg to optimize routing
 * 
 */
void DGraph::bce_routing() {
    // schedule pathset-wise
    for (auto& pathset : path_segs) {
        // std::cout << "dg Path before:" << std::endl;
        // for (const auto& path : pathset) {
        //     for (const auto& via : path.via) {
        //         std::cout << via.first << "," << via.second << " ";
        //     }
        //     std::cout << std::endl;
        // }
        scheduler.set_path_set(pathset);
        pathset = scheduler.schedule();
        // std::cout << "dg Path after:" << std::endl;
        // for (const auto& path : pathset) {
        //     for (const auto& via : path.via) {
        //         std::cout << via.first << "," << via.second << " ";
        //     }
        //     std::cout << std::endl;
        // }
        // append final path to sdg
        for (const auto& path : pathset) {
            add_path(path);
        }
    }
}

void DGraph::add_path(const Path& path) {
    const auto& via = path.via;
    for (int i = 0; i < via.size() - 1; i++) {
        auto src = xy_to_id(via[i]);
        auto dst = xy_to_id(via[i + 1]);
        auto dedge = get_edge_property(src, dst, sdg);
        dedge.pathset.insert({path.id, path.datavolume});
        dedge.datavolume += path.datavolume;
        set_edge_property(src, dst, std::move(dedge), sdg);
    }
}

void DGraph::print_graph_info() const {
    std::cout << "DGraph info:" << std::endl;
    // print seg num
    std::cout << "Segment num: " << path_segs.size() << std::endl;
    // print path info
    for (const auto& [key, val] : paths) {
        std::cout << "Path of tile " << key << std::endl;
        for (const auto& path : val) {
            std::cout << "Path: ";
            for (const auto& node : path.via) {
                std::cout << node.first << "," << node.second << " ";
            }
            std::cout << "Volume: " << path.datavolume << std::endl;
        }
    }
}

void DGraph::print_path_info() const {
    // print path info seg-wise
    int i = 0;
    for (const auto& seg : path_segs) {
        std::cout << "Segment " << i++ << std::endl;
        for (const auto& path : seg) {
            std::cout << "Path: ";
            for (const auto& node : path.via) {
                std::cout << node.first << "," << node.second << " ";
            }
            std::cout << "Volume: " << path.datavolume << std::endl;
        }
    }
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
    // os << "Super nodes: " << std::endl;
    // for (const auto& i : tnode.super_nodes) {
    //     os << i;
    // }
    os << "Parent id: ";
    for (const auto& i : tnode.parent_id) {
        os << i << " ";
    }
    os << std::endl;
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
    if (hnode.mapped) {
        os << "TNode id: " << hnode.tnode_id << std::endl;
    } else {
        os << "TNode id: " << "unmapped" << std::endl;
    }
    os << "Tile id: (" << hnode.tile_id.first << ", " << hnode.tile_id.second << ")" << std::endl;
    // os << "Ofmap size: " << hnode.ofm_size << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HEdge& hedge) {
    os << "Path set: ";
    for (const auto& i : hedge.pathset) {
        os << "(" << i.first << ", " << i.second << ") ";
    }
    os << "Data volume: " << hedge.datavolume << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const DNode& dnode) {
    os << "TNode id: " << dnode.tnode_id << std::endl;
    os << "Tile id: (" << dnode.tile_id.first << ", " << dnode.tile_id.second << ")" << std::endl;
    return os;
}

// std::ostream& operator<<(std::ostream& os, const DEdge& dedge) {
//     os << "Path set: ";
//     for (const auto& [key, val] : dedge.pathset) {
//         os << "(" << key << ", " << val << ") ";
//     }
//     // os << "Path set: ";
//     // for (const auto& i : dedge.pathset) {
//     //     os << "(" << i.first << ", " << i.second << ") ";
//     // }
//     // os << "Data volume: " << dedge.datavolume << std::endl;
//     // os << "BCE: " << dedge.bce << std::endl;
//     return os;
// }

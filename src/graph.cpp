#include "graph.h"

CGraph::CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size, 
               std::shared_ptr<CStrategyBase> strategy)
    : BaseGraph<CGraph, CNode, CEdge>(strategy),
      kernels{kernels}, CNode_size{CNode_size}, cdeps{}, dup_num(kernels.size(), 1) {
    this->analysis();
    std::cout << "CGraph created" << std::endl;
}

void CGraph::create_dup_num() {
    // layer-throughput reduction struct
    struct LayerDup {
        int layer;
        int dup_num;
        int compute_num; // compute num under current dup_num
        int node_num; // num for single kernel
    };
    struct LayerDupComparator {
        bool operator()(const LayerDup& a, const LayerDup& b) const {
            return (a.compute_num > b.compute_num) || (a.compute_num == b.compute_num && a.layer < b.layer); // max throughput reduction first
        }
    };
    std::multiset<LayerDup, LayerDupComparator> layer_dup_set;
    // create dup_num for each layer
    int total_node_num = 0;
    // overall compute num for each layer
    std::vector<int> layer_compute_nums(kernels.size(), 0);
    // initialize layer_dup_set
    for (const auto& i : kernels) {
        auto layer_node_num = compute_node_num(CNode_size, i.wsize, i.channel);
        total_node_num += layer_node_num;
        auto layer_compute_num = i.ifmap_size.first * i.ifmap_size.second * i.channel.first * i.channel.second;
        layer_dup_set.insert({i.layer, 1, layer_compute_num, layer_node_num});
        layer_compute_nums[i.layer] = layer_compute_num;
    }
    int available_num = total_node_num; // available extra node
    std::cout << "Total node num: " << total_node_num << std::endl;
    // max throughput reduction layer first
    auto it = layer_dup_set.begin();
    while (available_num > 0 && it != layer_dup_set.end()) {
        auto cur_iter = it++;
        auto cur_node_num = cur_iter->node_num;
        if (available_num >= cur_node_num) {
            // update dup_num info
            auto cur_layer = cur_iter->layer;
            available_num -= cur_node_num;
            // std::cout << "Layer " << cur_layer << " cur node num: " << cur_node_num << " duplicated, available node num: " << available_num << std::endl;
            dup_num[cur_layer] += cur_iter->dup_num;
            // update layer_compute_num and dup_num
            auto new_dup = cur_iter->dup_num + 1;
            auto new_compute_num = layer_compute_nums[cur_layer] / new_dup;
            // re-insert updated layer info
            layer_dup_set.erase(cur_iter);
            layer_dup_set.insert({cur_layer, new_dup, new_compute_num, cur_node_num});
            // back to top of the set
            it = layer_dup_set.begin();
        }
    }
    // print dup_num
    // std::cout << "Duplication number for each layer:" << std::endl;
    // for (size_t i = 0; i < dup_num.size(); i++) {
    //     std::cout << "Layer " << i << ": " << dup_num[i] << std::endl;
    // }
}

void CGraph::create_cnodes() {
    // create cnodes kernel-wise
    for (const auto& i: kernels) {
        // determine in/out chan num of a cnode
        int window_size = i.wsize.first * i.wsize.second;
        // int in_chan = (CNode_size.first + window_size - 1) / window_size;
        int in_chan = CNode_size.first / window_size;
        int out_chan = CNode_size.second;
        // cur layer info
        auto [ker_in, ker_out] = i.channel;
        auto cur_l = i.layer;
        const auto& cur_dep = i.depinfo;
        auto cur_ifm = i.ifmap_size;
        auto cur_ofm = i.ofmap_size;
        // construct nodes under the size constraints of (in_chan, out_chan)
        auto cur_dup = dup_num[cur_l];
        // for dup accblk group
        std::vector<std::vector<AccBlk>> accblks_group{};
        // create duplicated dep
        for (int d = 0; d < cur_dup; d++) {
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
                    // node_ifm = cur_ifm.first * cur_ifm.second / cur_dup * (ci_end - ci_begin);
                    node_ifm = std::max(cur_ifm.first * cur_ifm.second / cur_dup, 1) * (ci_end - ci_begin);
                    if(ci_end == ker_in) {
                        // node_ofm = cur_ofm.first * cur_ofm.second / cur_dup * (co_end - co_begin);
                        node_ofm = std::max(cur_ofm.first * cur_ofm.second / cur_dup, 1) * (co_end - co_begin);
                    }
                    // other accblk: generate ifm
                    else {
                        // node_ofm = cur_ifm.first * cur_ifm.second / cur_dup * (co_end - co_begin);
                        node_ofm = std::max(cur_ifm.first * cur_ifm.second / cur_dup, 1) * (co_end - co_begin);
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
            // add accblks group to cdeps
            // cdeps.emplace_back(CDep{accblks, cur_l, cur_dep});
            accblks_group.emplace_back(accblks);
        }
        cdeps.emplace_back(CDep{accblks_group, cur_l, cur_dep});
        // update depth map
        depth_map.try_emplace(cur_l, 0);
        int child_depth = depth_map[cur_l] + 1;
        std::for_each(cur_dep.begin(), cur_dep.end(), [&](const Depinfo& dep) {
            if (dep.dep_layer != -1) { // skip output dep
                depth_map[dep.dep_layer] = std::max(depth_map[dep.dep_layer], child_depth);
            }
        });
    }
    // print depth_map
    // std::cout << "Depth map:" << std::endl;
    // for (const auto& [layer, depth] : depth_map) {
    //     std::cout << "Layer " << layer << ": " << depth << std::endl;
    // }
}

void CGraph::conn_accblk() {
    for (const auto& v : cdeps) {
        for (const auto& blk : v.acc_blks) {
            for (const auto& blk_elem : blk) {
                const auto vertexs = blk_elem.vertex_id;
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
}

void CGraph::inter_layer_conn() {
    for (const auto& v : cdeps) {
        // get acc blks and dep info
        const auto& deps = v.dep_info;
        // use for determine connection relationship with dep layers grp
        int grp_id = 0;
        auto cur_dupnum = dup_num[v.layer];
        for (const auto& src_grp : v.acc_blks) {
            for (const auto& src : src_grp) {
                // get cur data volume and output channel id
                auto src_node = src.vertex_id.back();
                const auto src_property = get_node_property(src_node,cg);
                auto cur_datavolume = src_property.ofmap_size;
                // auto cur_cin_num = src_property.id_cout.second - src_property.id_cout.first + 1;
                auto co_src = src.cout_id;
                auto cur_cout_num = co_src.second - co_src.first + 1;
                // get dst node grp
                for (const auto& dep : deps) {
                    // skip output dep, represent by -1
                    if (dep.dep_layer == -1) {continue;}
                    auto dep_dupnum = dup_num[dep.dep_layer];
                    auto conn_id = dup_to_dup(cur_dupnum, dep_dupnum, grp_id);
                    // get dep layer info struct
                    // for (const auto& dst_layer : cdeps[dep.dep_layer].acc_blks) {
                    for (auto i : conn_id) {
                        const auto& dst_grp = cdeps[dep.dep_layer].acc_blks[i];
                        for (const auto& dst_layer : dst_grp) {
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
            grp_id++;
        }
    }
}

void CGraph::print_graph_info() const{ 
    std::cout << "Graph info:" << std::endl;
    BaseGraph<CGraph, CNode, CEdge>::print_graph_info(cg);
    std::cout << "AccBlk info:" << std::endl;
    for(const auto&v : cdeps) {
        std::cout << "Layer: " << v.layer << std::endl;
        for (const auto& blk_grp : v.acc_blks) {
            for(const auto& blk : blk_grp) {
                std::cout << "AccBlk: " << blk.cout_id.first << " - " << blk.cout_id.second << std::endl;
                for(const auto& node : blk.vertex_id) {
                    std::cout << "Node: " << node << std::endl;
                }
            }
        }
    }
}

TGraph::TGraph(std::shared_ptr<const CGraph> cg, int tile_xbar_num, 
               std::shared_ptr<TStrategyBase> strategy)
    : BaseGraph<TGraph, TNode, TEdge>(strategy),
      tg{}, cg_ref{cg}, node_map{}, tdeps{}, tile_xbar_num{tile_xbar_num} {
    auto cnode_num = cg_ref->num_nodes(cg_ref->get_graph());
    std::cout << "cnode num: " << cnode_num << " tile xbar num: " << tile_xbar_num << std::endl;
    this->analysis();
}

void TGraph::create_tnodes_MNSIM() {
    // merge cnodes sequentially & layer-wise
    std::vector<size_t> cnode_id{};
    for(const auto& cdep : cg_ref->get_cdep()) {
        for (const auto& acc_grp : cdep.acc_blks) {
            for(const auto& accblk : acc_grp) {
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
        auto acc_grp = cdep.acc_blks;
        for (const auto& acc_blks: acc_grp) {
            // BL split
            auto col_size = acc_blks.size();
            // WL split
            auto row_size = acc_blks[0].vertex_id.size();
            // uniformsplit
            auto split = uniformsplit(row_size, col_size, tile_xbar_num);
            // create TNode
            for(const auto& cgroup : split) {
                // get cnode id of cur tnode
                std::vector<size_t> cnode_id{};
                for(const auto& i : cgroup) {
                    cnode_id.emplace_back(acc_blks[i.second].vertex_id[i.first]);
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
}

void TGraph::create_tnodes_PIMAPPING() {
    // struct TNodeGroup {
    //     std::vector<size_t> cnode_id; // cnode id
    //     struct Ofm{
    //         int intra_layer = 0; // intra-layer ofm size
    //         int inter_layer = 0; // inter-layer ofm size
    //     };
    //     struct TileData {
    //         int ifmap_size = 0;
    //         Ofm ofmap_size = {};
    //     }; // tile data volume
    //     TileData inter_tile{}, intra_tile{};
    // };
    // const auto& cg = cg_ref->get_graph();
    // const auto& cdeps = cg_ref->get_cdep();
    // std::map<size_t, bool> cnode_visited; // cnode visited flag
    // TNodeGroup tnode_group{}; // current tnode group
    // for (const auto& cdep: cdeps) {
    //     // get cnode size
    //     auto acc_grp = cdep.acc_blks;
    //     for (const auto& acc_blks: acc_grp) {
    //         // BL split
    //         auto col_size = acc_blks.size();
    //         // WL split
    //         auto row_size = acc_blks[0].vertex_id.size();
    //         // 
    //     }
    // }
    std::unordered_map<size_t, std::unordered_map<size_t, int>> conn_intensity_map;
    const auto& cg = cg_ref->get_graph();
    // build intensity map by ofm
    for (const auto& e: boost::make_iterator_range(edges(cg))) {
        auto src = source(e, cg);
        auto dst = target(e, cg);
        auto cedge = cg_ref->get_edge_property(e, cg);
        conn_intensity_map[src][dst] += cedge.datavolume;
        conn_intensity_map[dst][src] += cedge.datavolume;
    }
    // update intensity map by ifm
    for (const auto& cdep_vec: cg_ref->get_cdep()) {
        for (const auto& acc_blks : cdep_vec.acc_blks) {
            auto col_size = acc_blks.size();
            auto row_size = acc_blks[0].vertex_id.size();
            for (auto i = 0; i < row_size; i++) {
                auto common_ifm = cg_ref->get_node_property(acc_blks[0].vertex_id[i], cg).ifmap_size;
                for (auto j = 1; j < col_size; j++) {
                    conn_intensity_map[acc_blks[0].vertex_id[i]][acc_blks[j].vertex_id[i]] += common_ifm;
                }
            }
        }
    }
    // get sorted cnode by conn intensity
    auto sorted_cnodes = k_group_sort(conn_intensity_map, tile_xbar_num);
    // create tnodes
    std::vector<size_t> cnode_id{};
    for (const auto& cnode : sorted_cnodes) {
        if(cnode_id.size() == tile_xbar_num) {
            // merge cur group into a tnode
            auto tnode_id = add_node(TNode{cnode_id}, tg);
            // build node map, i is unique
            for (const auto& i : cnode_id) {
                node_map.emplace(i, tnode_id);
            }
            cnode_id.clear();
        }
        cnode_id.emplace_back(cnode);
    }
    // merge remaining cnodes
    if(!cnode_id.empty()) {
        auto tnode_id = add_node(TNode{cnode_id}, tg);
        // build node map, i is unique
        for (const auto& i : cnode_id) {
            node_map.emplace(i, tnode_id);
        }
    }
}

void TGraph::create_tnodes_SPATEM() {
    // inter-layer merge strategy
    std::vector<std::vector<size_t>> cnode_ids{};
    int tile_num = (boost::num_vertices(cg_ref->get_graph()) + tile_xbar_num - 1) / tile_xbar_num;
    cnode_ids.resize(tile_num);
    // std::cout << "Tile num: " << tile_num << std::endl;
    // std::cout << "cnode_ids size: " << cnode_ids.size() << std::endl;
    // traverse cdeps
    int cur_cnode_counter = 0;
    for (const auto& cdep : cg_ref->get_cdep()) {
        // get acc blks vector
        const auto& acc_blks_vec = cdep.acc_blks;
        for (const auto& acc_blks : acc_blks_vec) {
            // get cnode id
            for (const auto& acc_blk : acc_blks) {
                for (const auto& node : acc_blk.vertex_id) {
                    auto cur_tnode_id = cur_cnode_counter % tile_num;
                    // std::cout << "cur_tnode_id: " << cur_tnode_id << std::endl;
                    cnode_ids[cur_tnode_id].emplace_back(node);
                    // std::cout << "done" << std::endl;
                    cur_cnode_counter++;
                }
            }
        }
    }
    // create TNode
    for (const auto& cnode_id : cnode_ids) {
        if (cnode_id.empty()) {continue;}
        // create tnode
        auto tnode_id = add_node(TNode{cnode_id}, tg);
        // build node map, i is unique
        for (const auto& i : cnode_id) {
            node_map.emplace(i, tnode_id);
        }
    }
}

void TGraph::create_TDep() {
    // get cdep info
    const auto& cdeps = cg_ref->get_cdep();
    // create TDep
    for (const auto& cdep : cdeps) {
        // get cnode accblk index
        const auto& accblks_grp = cdep.acc_blks;
        for (auto accblks : accblks_grp) {
            for(auto accblk : accblks) {
                std::set<Node> tdep{};
                for(auto node : accblk.vertex_id) {
                    tdep.emplace(node_map[node]);
                }
                tdeps.emplace_back(tdep);
            }
        }
    }
}

void TGraph::inter_tile_conn() {
    // find inter-tile c-edges
    const auto& cg = cg_ref->get_graph();
    // traverse nodes
    for (const auto& v : boost::make_iterator_range(vertices(cg))) {
        Node src_t = node_map[v];
        // auto src_layer = get_node_property(v, cg).layer;
        auto src_layer = cg_ref->get_node_property(v, cg).layer;
        const auto& v_dst = get_adjacent_nodes(v, cg);
        // traverse dst nodes
        std::map<Node, std::vector<CEdge>> inter_tile_edges;
        for (const auto& dst : v_dst) {
            Node dst_t = node_map[dst];
            // skip self-loop
            if(src_t == dst_t) {continue;}
            // get cedge property
            auto cedge = cg_ref->get_edge_property(v, dst, cg);
            // update tedges
            inter_tile_edges[dst_t].emplace_back(cedge);
        }
        // merge inter-tile edges
        for (auto& [tnode_key, cedge_vec] : inter_tile_edges) {
            std::sort(cedge_vec.begin(), cedge_vec.end(), [](const CEdge& a, const CEdge& b) {
                return a.channel_id.first < b.channel_id.first;
            });
            CEdge merged_edge{cedge_vec[0]};
            for (int i = 1;i < cedge_vec.size(); i++) {
                auto unique_chan = UniqueElements(merged_edge.channel_id, cedge_vec[i].channel_id);
                merged_edge.datavolume += cedge_vec[i].datavolume * unique_chan / (cedge_vec[i].channel_id.second - cedge_vec[i].channel_id.first + 1);
                // update end channel range
                merged_edge.channel_id.second = std::max(merged_edge.channel_id.second, cedge_vec[i].channel_id.second);
            }
            update_tedges(src_t, tnode_key, merged_edge, src_layer);
            // update tnode parent info
            if(get_edge_property(src_t, tnode_key, tg).t_type != DepType::Accum) {
                auto& tnode = get_node_property(tnode_key, tg);
                tnode.parent_id.insert(src_t);
            }
        }
    }
}

void TGraph::update_tedges(Node src_t, Node dst_t, CEdge cedge, int src_layer) {
    auto acc_num = cedge.datavolume * (cedge.c_type == DepType::Accum);
    auto prop_num = cedge.datavolume * (cedge.c_type == DepType::Prop);
    auto [e, found] = boost::edge(src_t, dst_t, tg);
    if (!found) {
        // create new edge
        add_edge(src_t, dst_t, TEdge{cedge.c_type, acc_num, prop_num, {{src_layer, cedge.datavolume}}}, tg);
    } else {
        // update edge
        auto& cur_tedge = get_edge_property(src_t, dst_t, tg);
        if(cur_tedge.t_type != cedge.c_type) {
            cur_tedge.t_type = DepType::Mixed;
        }
        cur_tedge.accvolume += acc_num;
        cur_tedge.propvolume += prop_num;
        cur_tedge.layer_map[src_layer] += cedge.datavolume;
    }
}

void TGraph::print_graph_info() const { 
    std::cout << "Graph info:" << std::endl;
    BaseGraph<TGraph, TNode, TEdge>::print_graph_info(tg);
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

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, 
               std::pair<int, int> tile_size, std::shared_ptr<HStrategyBase> strategy)
    : BaseGraph<HGraph, HNode, HEdge>(strategy),
      hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{tile_size}, paths{}, mapper{} {
    
    auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
    std::cout << "num tile: " << num_tile << std::endl;
    
    if (tile_size.first * tile_size.second < num_tile) {
        auto tile_x = static_cast<int>(std::ceil(std::sqrt(num_tile)));
        this->tile_size = std::make_pair(std::max(tile_size.first,tile_x), std::max(tile_size.second,tile_x));
        std::cout << "Reshape to " << this->tile_size.first << " x " << this->tile_size.second << " to fit algorithm size" << std::endl;
    } else {
        std::cout << "Tile size: " << this->tile_size.first << " x " << this->tile_size.second << std::endl;
    }
    
    mapper = Mapper{this->tile_size};
    this->analysis();
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

void HGraph::SPATEM_mapping() {
    // sort Tnode by connection intensity
    const auto& tg = tg_ref->get_graph();
    // build connection intensity map
    // traverse TEdges
    std::unordered_map<size_t, std::unordered_map<size_t, int>> conn_intensity_map;
    for (const auto& e : boost::make_iterator_range(edges(tg))) {
        auto src = boost::source(e, tg);
        auto dst = boost::target(e, tg);
        auto tedge = tg_ref->get_edge_property(e, tg);
        auto datavolume = tedge.accvolume + tedge.propvolume;
        conn_intensity_map[src][dst] += datavolume;
        conn_intensity_map[dst][src] += datavolume; // undirected graph
    }
    auto mapping_order = neighbor_ranking_sort(conn_intensity_map);
    // map Tnode to HNode
    mapper.SPATEM_mapping(mapping_order);
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
    BaseGraph<HGraph, HNode, HEdge>::print_graph_info(hg);
}

DGraph::DGraph(std::shared_ptr<const HGraph> hg, std::shared_ptr<const TGraph> tg,
               std::shared_ptr<const CGraph> cg, int pipeline_depth, 
               std::shared_ptr<DStrategyBase> strategy)
    : BaseGraph<DGraph, DNode, DEdge>(strategy),
      pipeline_depth{pipeline_depth}, tile_size{hg->tile_size},
      sdg{}, path_segs{}, layer_segs{}, hg_ref{hg}, tg_ref{tg}, cg_ref{cg},
      tdep_map{}, harbor_map{}, path_map{}, paths{}, congestion_segs{},
      scheduler{hg->tile_size} {
    this->analysis();
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
    if (opt_type != OptType::PIMAPPING) {return;}
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
    // build path
    const auto& tg = tg_ref->get_graph();
    // traverse TEdges
    size_t path_id = 0;
    // int min_layer = std::numeric_limits<int>::max();
    // int max_layer = 0;
    std::vector<std::pair<int, size_t>> pathid_layer_map{};
    for (const auto& e : boost::make_iterator_range(edges(tg))) {
        auto src = boost::source(e, tg);
        auto dst = boost::target(e, tg);
        auto tedge = tg_ref->get_edge_property(e, tg);
        auto tedge_layermap = tedge.layer_map;
        auto src_d = get_core(src);
        auto dst_d = get_core(dst);
        // get path
        auto path = XYinit(src_d, dst_d);
        // traverse edges in tedge_layermap
        for (const auto& [layer, datavolume] : tedge_layermap) {
            // add path to paths
            // path_map[src].push_back(path_id);
            path_map[src].emplace_back(layer, path_id);
            pathid_layer_map.emplace_back(layer, path_id);
            paths.emplace_back(std::make_shared<Path>(path_id++, src_d, dst_d, path, datavolume));
            // min_layer = std::min(min_layer, layer);
            // max_layer = std::max(max_layer, layer);
        }
    }
    std::sort(pathid_layer_map.begin(), pathid_layer_map.end(), [](const auto& a, const auto& b) {
        return a.first < b.first; // Sort by layer ascending
    });
    // allocate path and layer seg by pipeline depth
    const auto& depth_map = cg_ref->get_depth_map();
    std::map<int, std::vector<size_t>> path_seg_map;
    std::map<int, std::set<int>> layer_seg_map;
    for (const auto& [layer, path_id] : pathid_layer_map) {
        // calculate key based on layer and pipeline depth
        auto key = depth_map.at(layer) / pipeline_depth;
        // add path_id to path_seg_map
        path_seg_map[key].push_back(path_id);
        // add layer to layer_seg_map
        layer_seg_map[key].insert(layer);
    }
    // build path_segs and layer_segs
    for (const auto& [key, path_ids] : path_seg_map) {
        path_segs.emplace_back(path_ids);
    }
    for (const auto& [key, layers] : layer_seg_map) {
        layer_segs.emplace_back(layers);
    }
    // print layer_segs
    // for (const auto& seg : layer_segs) {
    //     std::cout << "Layer seg: ";
    //     for (const auto& layer : seg) {
    //         std::cout << layer << " ";
    //     }
    //     std::cout << std::endl;
    // }
}

std::vector<std::shared_ptr<Path>> DGraph::get_pathset(std::vector<size_t> path_ids) const {
    std::vector<std::shared_ptr<Path>> pathset{};
    std::transform(path_ids.begin(), path_ids.end(), std::back_inserter(pathset), [this](int idx) {
            return paths[idx];  // 根据id提取对应的shared_ptr
        });
    return pathset;
}

std::vector<std::shared_ptr<Path>> DGraph::get_pathset(std::vector<size_t> path_ids) {
    std::vector<std::shared_ptr<Path>> pathset{};
    std::transform(path_ids.begin(), path_ids.end(), std::back_inserter(pathset), [this](int idx) {
            return paths[idx];  // 根据id提取对应的shared_ptr
        });
    return pathset;
}

/**
 * @brief use path_seg to optimize routing
 * 
 */
void DGraph::bce_routing() {
    // schedule pathset-wise
    for (auto& seg : path_segs) {
        // std::cout << "Scheduling segment with " << seg.size() << " paths." << std::endl;
        auto pathset = get_pathset(seg);
        // std::cout << "Pathset size: " << pathset.size() << std::endl;
        scheduler.set_path_set(pathset);
        // std::cout << "Pathset set." << std::endl;
        auto schedinfo = scheduler.schedule();
        // std::cout << "Scheduling done." << std::endl;
        congestion_segs.push_back(schedinfo);
        for (const auto& path : pathset) {
            add_path(path);
        }
        // std::cout << "Segment scheduling done." << std::endl;
    }
}

void DGraph::xy_routing() {
    for (const auto& seg: path_segs) {
        // auto pathset_ptr = get_pathset(seg);
        // std::vector<Path> pathset{};
        // std::transform(pathset_ptr.begin(), pathset_ptr.end(), std::back_inserter(pathset), [](std::shared_ptr<Path> path_ptr) {
        //     return *path_ptr;
        // });
        auto pathset = get_pathset(seg);
        scheduler.set_path_set(pathset);
        auto schedinfo = scheduler.xy_routing();
        congestion_segs.push_back(schedinfo);
        for (const auto& path : pathset) {
            add_path(path);
        }
    }
}

void DGraph::add_path(const std::shared_ptr<Path> path_ptr) {
    auto path = *path_ptr;
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
    // for (const auto& [key, val] : path_map) {
    //     std::cout << "Path of tile " << key << std::endl;
    //     auto paths = get_pathset(val);
    //     for (const auto& path_ptr : paths) {
    //         auto path = *path_ptr;
    //         std::cout << "Path: ";
    //         for (const auto& node : path.via) {
    //             std::cout << node.first << "," << node.second << " ";
    //         }
    //         std::cout << "Volume: " << path.datavolume << std::endl;
    //     }
    // }
}

void DGraph::print_path_info() const {
    // print path info seg-wise
    int i = 0;
    for (const auto& seg : path_segs) {
        std::cout << "Segment " << i++ << std::endl;
        auto paths = get_pathset(seg);
        for (const auto& path_ptr : paths) {
            auto path = *path_ptr;
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
    for (const auto& [layer, volume] : tedge.layer_map) {
        os << "Layer " << layer << ": " << volume << std::endl;
    }
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

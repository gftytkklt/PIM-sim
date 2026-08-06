#include "graph.h"
CGraph::CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size, 
               std::shared_ptr<CStrategyBase> strategy)
    : BaseGraph<CGraph, CNode, CEdge>(strategy),
      kernels{kernels}, CNode_size{CNode_size}, cdeps{}, dup_num(kernels.size(), 1) {
    build_depth_map();
    this->analysis();
    create_cnodes();
    conn_accblk();
    inter_layer_conn();
    PIM_INFO("CGraph created");
}

CGraph::CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size, int CNode_capacity, 
               std::shared_ptr<CStrategyBase> strategy)
    : BaseGraph<CGraph, CNode, CEdge>(strategy),
      kernels{kernels}, CNode_size{CNode_size}, cnode_capacity{CNode_capacity}, cdeps{}, dup_num(kernels.size(), 1) {
    build_depth_map();
    this->analysis();
    create_cnodes();
    conn_accblk();
    inter_layer_conn();
    PIM_INFO("CGraph created with tile2.0 optimization");
}

void CGraph::build_graph_subset(){
    /*** determine the maximum subset under the cnode_capacity constraint ***/
    // build vec_id layer map
    std::map<int, int> layer_id_map;
    std::map<int, std::vector<int>> depth_id_map;
    for (size_t i = 0; i < kernels.size(); i++) {
        auto [_, inserted] = layer_id_map.try_emplace(kernels[i].layer, i);
        if(!inserted) {
            throw std::runtime_error("Duplicate layer found");
        }
        depth_id_map[depth_map[kernels[i].layer]].push_back(i);
    }
    // traverse kernel by depth, build subset by adding kernels until reaching cnode_capacity
    std::vector<int> qkernel; // kernel traversal order
    int max_cnode_num = 0;
    int max_compute_num = 0;
    std::pair<int, int> best_subset_range{}; // (start, end) kernel id of the maximum subset
    for (const auto& [depth, ker_ids] : depth_id_map) {
        qkernel.insert(qkernel.end(), ker_ids.begin(), ker_ids.end());
    }
    int qbegin = 0;
    for (const auto& ker_id : qkernel) {
        const auto& ker = kernels[ker_id];
        int node_num = compute_node_num(CNode_size, ker.wsize, ker.channel);
        // std::cout << "cur cnode num: " << node_num << " for kernel layer " << ker.layer << std::endl;
        if (node_num > cnode_capacity) {
            continue;
        }
        int cur_node_num = node_num;
        int compute_num = get_compute_num(ker.wsize, ker.channel, ker.ofmap_size);
        int qend = qbegin;
        for (auto succ_id = qbegin + 1; succ_id < kernels.size(); succ_id++) {
            const auto& succ_ker = kernels[succ_id];
            int succ_node_num = compute_node_num(CNode_size, succ_ker.wsize, succ_ker.channel);
            if (cur_node_num + succ_node_num > cnode_capacity) {
                break;
            }
            cur_node_num += succ_node_num;
            compute_num += get_compute_num(succ_ker.wsize, succ_ker.channel, succ_ker.ofmap_size);
            qend = succ_id;
        }
        // compute num first
        if (compute_num > max_compute_num) {
            max_compute_num = compute_num;
            max_cnode_num = cur_node_num;
            best_subset_range = {qbegin, qend};
        }
        else if (compute_num == max_compute_num && cur_node_num > max_cnode_num) {
            max_cnode_num = cur_node_num;
            best_subset_range = {qbegin, qend};
        }
        qbegin++;
    }
    // std::cout << "capacity constraint: " << cnode_capacity << std::endl;
    // std::cout << "max cnode num: " << max_cnode_num << ", max compute num: " << max_compute_num << std::endl;
    if (max_cnode_num == 0) {
        std::cout << "No valid kernel subset found under the cnode capacity constraint." << std::endl;
        return;
    }
    // keep kernels only in {kernels[qkernel[best_subset_range.first]], ..., kernels[qkernel[best_subset_range.second]]}
    std::vector<NNkernel> filtered_kernels;
    std::set<int> selected_layers;
    for (int i = best_subset_range.first; i <= best_subset_range.second; i++) {
        const auto& ker = kernels[qkernel[i]];
        filtered_kernels.push_back(ker);
        selected_layers.insert(ker.layer);
    }
    kernels = filtered_kernels;
    // erase deps of unselected layers
    for (auto& ker : kernels) {
        auto& depinfo = ker.depinfo;
        depinfo.erase(std::remove_if(depinfo.begin(), depinfo.end(), [&](const Depinfo& dep){
            return selected_layers.find(dep.dep_layer) == selected_layers.end();
        }), depinfo.end());
        if (depinfo.empty()) {
            depinfo.push_back(Depinfo{-1, {-1, -1}}); // add dummy dep for output layer
        }
    }
    // rebuild depth_map
    std::set<int> selected_depth;
    for (auto i : selected_layers) {
        // build_depth_map guarantees the existence of depth_map[i]
        selected_depth.insert(depth_map[i]);
    }

    std::unordered_map<int, int> depth_remapping;
    int new_depth = 0;
    for (const auto& depth : selected_depth) {
        depth_remapping[depth] = new_depth++;
    }
    std::map<int, int> new_depth_map;
    for (auto layer : selected_layers) {
        int old_depth = depth_map[layer];
        new_depth_map[layer] = depth_remapping[old_depth];
    }
    depth_map = std::move(new_depth_map);
    // print the subset result
    std::cout << "[CG] Selected kernel subset for tile2.0 optimization:" << std::endl;
    for (const auto& ker : kernels) {
        std::cout << "[CG] Layer " << ker.layer << ": wsize(" << ker.wsize.first << ", " << ker.wsize.second << "), channel(" << ker.channel.first << ", " << ker.channel.second << "), depth: " << depth_map[ker.layer] << std::endl;
    }
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
    // std::cout << "Total node num: " << total_node_num << std::endl;
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

void CGraph::build_depth_map() {
    // build depth map for each layer
    for (const auto& i : kernels) {
        auto cur_layer = i.layer;
        const auto& cur_dep = i.depinfo;
        depth_map.try_emplace(cur_layer, 0);
        int child_depth = depth_map[cur_layer] + 1;
        std::for_each(cur_dep.begin(), cur_dep.end(), [&](const Depinfo& dep) {
            if (dep.dep_layer != -1) { // skip output dep
                depth_map[dep.dep_layer] = std::max(depth_map[dep.dep_layer], child_depth);
            }
        });
    }
}

void CGraph::create_cnodes() {
    // create cnodes kernel-wise
    for (const auto& i: kernels) {
        // determine in/out chan num of a cnode
        int window_size = i.wsize.first * i.wsize.second;
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
                    node_ifm = std::max(cur_ifm.first * cur_ifm.second / cur_dup, 1) * (ci_end - ci_begin);
                    if(ci_end == ker_in) {
                        node_ofm = std::max(cur_ofm.first * cur_ofm.second / cur_dup, 1) * (co_end - co_begin);
                    }
                    // other accblk: generate ifm
                    else {
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
            accblks_group.emplace_back(accblks);
        }
        cdeps.emplace_back(CDep{accblks_group, cur_l, cur_dep});
    }
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
                auto co_src = src.cout_id;
                auto cur_cout_num = co_src.second - co_src.first + 1;
                // get dst node grp
                for (const auto& dep : deps) {
                    // skip output dep, represent by -1
                    if (dep.dep_layer == -1) {continue;}
                    auto dep_dupnum = dup_num[dep.dep_layer];
                    auto conn_id = dup_to_dup(cur_dupnum, dep_dupnum, grp_id);
                    // get dep layer info struct
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

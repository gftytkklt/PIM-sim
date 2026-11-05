#include "graph.h"

namespace {
// floor(elements * offset / channels), without overflowing the product.
int64_t stream_prefix(int64_t elements, int offset, int channels) {
    return (elements / channels) * offset + (elements % channels) * offset / channels;
}

std::pair<int64_t, int64_t> channel_stream(std::pair<int, int> channels,
                                        std::pair<int, int> full, int64_t elements) {
    const int count = full.second - full.first + 1;
    return {stream_prefix(elements, channels.first - full.first, count),
            stream_prefix(elements, channels.second - full.first + 1, count)};
}
} // namespace

CGraph::CGraph(const std::vector<NNkernel> kernels, std::pair<int, int> CNode_size, 
               std::shared_ptr<CStrategyBase> strategy)
    : BaseGraph<CGraph, CNode, CEdge>(strategy),
      kernels{kernels}, CNode_size{CNode_size}, cdeps{}, dup_num{} {
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
      kernels{kernels}, CNode_size{CNode_size}, cnode_capacity{CNode_capacity}, cdeps{}, dup_num{} {
    build_depth_map();
    this->analysis();
    create_cnodes();
    conn_accblk();
    inter_layer_conn();
    PIM_INFO("CGraph created with tile2.0 optimization");
}

void CGraph::build_graph_subset(){
    if (cnode_capacity <= 0) throw pim::ConfigError("crossbar capacity must be positive");
    if (kernels.empty()) return;
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
    long long max_compute_num = 0;
    std::pair<int, int> best_subset_range{}; // (start, end) kernel id of the maximum subset
    for (const auto& [depth, ker_ids] : depth_id_map) {
        qkernel.insert(qkernel.end(), ker_ids.begin(), ker_ids.end());
    }
    int qbegin = 0;
    for (; qbegin < static_cast<int>(qkernel.size()); ++qbegin) {
        const auto& ker = kernels[qkernel[qbegin]];
        int node_num = compute_node_num(CNode_size, ker.wsize, ker.channel);
        // std::cout << "cur cnode num: " << node_num << " for kernel layer " << ker.layer << std::endl;
        if (node_num > cnode_capacity) {
            continue;
        }
        int cur_node_num = node_num;
        long long compute_num = get_compute_num(ker.wsize, ker.channel, ker.ofmap_size);
        int qend = qbegin;
        for (auto succ_id = qbegin + 1; succ_id < kernels.size(); succ_id++) {
            const auto& succ_ker = kernels[qkernel[succ_id]];
            int succ_node_num = compute_node_num(CNode_size, succ_ker.wsize, succ_ker.channel);
            if (succ_node_num > cnode_capacity - cur_node_num) {
                break;
            }
            cur_node_num += succ_node_num;
            auto extra_compute = get_compute_num(succ_ker.wsize, succ_ker.channel, succ_ker.ofmap_size);
            if (compute_num > std::numeric_limits<long long>::max() - extra_compute) throw pim::GraphError("subset compute count overflow");
            compute_num += extra_compute;
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
    }
    // std::cout << "capacity constraint: " << cnode_capacity << std::endl;
    // std::cout << "max cnode num: " << max_cnode_num << ", max compute num: " << max_compute_num << std::endl;
    if (max_cnode_num == 0) {
        throw pim::MappingError("no complete kernel fits the crossbar capacity");
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
        depinfo.erase(std::remove_if(depinfo.begin(), depinfo.end(), [&](const DepInfo& dep){
            return selected_layers.find(dep.dep_layer) == selected_layers.end();
        }), depinfo.end());
        if (depinfo.empty()) {
            depinfo.push_back(DepInfo{-1, {-1, -1}}); // add dummy dep for output layer
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
    throw pim::GraphError("weight replication requires spatial ranges and halo dependencies; not supported by NNkernel");
}

void CGraph::build_depth_map() {
    if (CNode_size.first <= 0 || CNode_size.second <= 0) {
        throw pim::ConfigError("crossbar dimensions must be positive");
    }
    depth_map.clear();
    std::map<int, size_t> layer_index;
    std::map<int, size_t> indegree;
    for (size_t index = 0; index < kernels.size(); ++index) {
        const auto& k = kernels[index];
        if (k.layer < 0 || !layer_index.emplace(k.layer, index).second) {
            throw pim::GraphError("layer identities must be unique nonnegative integers");
        }
        auto positive = [](auto shape) { return shape.first > 0 && shape.second > 0; };
        if (!positive(k.wsize) || !positive(k.channel) || !positive(k.ifmap_size) || !positive(k.ofmap_size)) {
            throw pim::GraphError("kernel dimensions must be positive, layer " + std::to_string(k.layer));
        }
        if (static_cast<long long>(k.wsize.first) * k.wsize.second > CNode_size.first) {
            throw pim::GraphError("kernel window exceeds crossbar rows; spatial kernel splitting is unsupported");
        }
        for (auto shape : {k.ifmap_size, k.ofmap_size}) {
            if (static_cast<long long>(shape.first) * shape.second >
                std::numeric_limits<int>::max() / std::max(k.channel.first, k.channel.second)) {
                throw pim::GraphError("feature-map element count exceeds supported integer range");
            }
        }
        depth_map[k.layer] = 0;
        indegree[k.layer] = 0;
        dup_num[k.layer] = 1;
    }
    for (const auto& k : kernels) {
        for (const auto& dep : k.depinfo) {
            if (dep.dep_layer == -1) continue;
            auto target = layer_index.find(dep.dep_layer);
            if (target == layer_index.end() || dep.dep_layer == k.layer) {
                throw pim::GraphError("dependency must reference a different existing layer");
            }
            const auto dst_chan = dep.dst_chan == std::pair{0, 0} ? dep.dep_chan : dep.dst_chan;
            if (dep.dep_chan.first < 1 || dep.dep_chan.second < dep.dep_chan.first ||
                dep.dep_chan.second > k.channel.second || dst_chan.first < 1 ||
                dst_chan.second < dst_chan.first ||
                dst_chan.second > kernels[target->second].channel.first || dep.elements < -1) {
                throw pim::GraphError("dependency channel interval is outside source/destination channels");
            }
            ++indegree.at(dep.dep_layer);
        }
    }
    std::priority_queue<int, std::vector<int>, std::greater<int>> ready;
    for (const auto& [layer, degree] : indegree) if (degree == 0) ready.push(layer);
    size_t visited = 0;
    while (!ready.empty()) {
        int layer = ready.top();
        ready.pop();
        ++visited;
        for (const auto& dep : kernels[layer_index.at(layer)].depinfo) {
            if (dep.dep_layer == -1) continue;
            depth_map.at(dep.dep_layer) = std::max(depth_map.at(dep.dep_layer), depth_map.at(layer) + 1);
            if (--indegree.at(dep.dep_layer) == 0) ready.push(dep.dep_layer);
        }
    }
    if (visited != kernels.size()) throw pim::GraphError("cycle in layer dependencies");
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
        std::vector<std::vector<AccBlock>> accblks_group{};
        // create duplicated dep
        for (int d = 0; d < cur_dup; d++) {
            int co_begin = 0;
            std::vector<AccBlock> accblks{};
            while (co_begin < ker_out) {
                // create the segmentation along the output channel dimension
                int co_end = co_begin + std::min(out_chan, ker_out - co_begin);
                auto co_id = std::make_pair(co_begin+1, co_end);
                int ci_begin = 0;
                std::vector<Node> accblk{};
                while(ci_begin < ker_in) {
                    // create the segmentation along the input channel dimension
                    int ci_end = ci_begin + std::min(in_chan, ker_in - ci_begin);
                    auto ci_id = std::make_pair(ci_begin+1, ci_end);
                    // instantiate a cnode
                    // Partial and final sums refer to the same output positions.
                    int node_ifm = cur_ifm.first * cur_ifm.second * (ci_end - ci_begin);
                    int node_ofm = cur_ofm.first * cur_ofm.second * (co_end - co_begin);
                    auto cnode = CNode{cur_l, node_ifm, node_ofm, ci_id, co_id};
                    // add node to accblk
                    accblk.emplace_back(add_node(cnode, cg));
                    // update ci_begin
                    ci_begin = ci_end;
                }
                // add accblk to accblks group
                accblks.emplace_back(AccBlock{accblk, co_id});
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
    std::map<int, const CDep*> deps_by_layer;
    for (const auto& dep : cdeps) deps_by_layer.emplace(dep.layer, &dep);
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
                auto co_src = src.cout_id;
                auto cur_cout_num = co_src.second - co_src.first + 1;
                // get dst node grp
                for (const auto& dep : deps) {
                    // skip output dep, represent by -1
                    if (dep.dep_layer == -1) {continue;}
                    DepInfo projection = dep;
                    if (projection.dst_chan == std::pair{0, 0}) projection.dst_chan = projection.dep_chan;
                    const auto source_count = projection.dep_chan.second - projection.dep_chan.first + 1;
                    if (projection.elements == -1) {
                        projection.elements = static_cast<int64_t>(src_property.ofmap_size / cur_cout_num) * source_count;
                    }
                    auto source_channels = std::pair{
                        std::max(co_src.first, projection.dep_chan.first),
                        std::min(co_src.second, projection.dep_chan.second)};
                    if (source_channels.first > source_channels.second) continue;
                    const auto source_stream = channel_stream(source_channels, projection.dep_chan, projection.elements);
                    auto dep_dupnum = dup_num[dep.dep_layer];
                    auto conn_id = dup_to_dup(cur_dupnum, dep_dupnum, grp_id);
                    // get dep layer info struct
                    for (auto i : conn_id) {
                        const auto& dst_grp = deps_by_layer.at(dep.dep_layer)->acc_blks.at(i);
                        for (const auto& dst_layer : dst_grp) {
                            for (const auto& dst_node : dst_layer.vertex_id) {
                                auto ci_dst = get_node_property(dst_node, cg).id_cin;
                                auto destination_channels = std::pair{
                                    std::max(ci_dst.first, projection.dst_chan.first),
                                    std::min(ci_dst.second, projection.dst_chan.second)};
                                if (destination_channels.first > destination_channels.second) continue;
                                const auto destination_stream = channel_stream(
                                    destination_channels, projection.dst_chan, projection.elements);
                                const auto begin = std::max(source_stream.first, destination_stream.first);
                                const auto end = std::min(source_stream.second, destination_stream.second);
                                const bool empty_slice = source_stream.first == source_stream.second ||
                                                         destination_stream.first == destination_stream.second;
                                if (begin < end || (empty_slice && begin == end)) {
                                    auto envelope = source_channels;
                                    if (begin < end && projection.elements % source_count == 0) {
                                        const auto density = projection.elements / source_count;
                                        envelope.first = projection.dep_chan.first + begin / density;
                                        envelope.second = projection.dep_chan.first + (end - 1) / density;
                                    }
                                    auto elements = checked_volume(end - begin);
                                    add_edge(src_node, dst_node,
                                             CEdge{DepType::Prop, envelope, elements, projection, {begin, end}}, cg);
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
    std::cout << "AccBlock info:" << std::endl;
    for(const auto&v : cdeps) {
        std::cout << "Layer: " << v.layer << std::endl;
        for (const auto& blk_grp : v.acc_blks) {
            for(const auto& blk : blk_grp) {
                std::cout << "AccBlock: " << blk.cout_id.first << " - " << blk.cout_id.second << std::endl;
                for(const auto& node : blk.vertex_id) {
                    std::cout << "Node: " << node << std::endl;
                }
            }
        }
    }
}

#include "graph.h"

HGraph::HGraph(std::shared_ptr<const TGraph> tg, std::shared_ptr<const CGraph> cg, 
               std::pair<int, int> tile_size, std::shared_ptr<HStrategyBase> strategy)
    : BaseGraph<HGraph, HNode, HEdge>(strategy),
      hg{}, tg_ref{tg}, cg_ref{cg}, tile_size{tile_size}, paths{}, mapper{} {
    
    auto num_tile = tg_ref->num_nodes(tg_ref->get_graph());
    std::cout << "[HG]: num tile: " << num_tile << std::endl;
    
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



#include "analyzer.h"
Analyzer::Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt) :
    opt_type{gen_opt_type(opt)},
    cg{kernels, info.xbar_size},
    tg{std::make_shared<CGraph>(cg), info.xbar_num, opt_type},
    hg{std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.tile_size, opt_type},
    dg{std::make_shared<HGraph>(hg), std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.pipeline_depth, opt_type}
    {}

OptType Analyzer::gen_opt_type(OptInfo opt) {
    if(opt.mapping_opt && opt.sched_opt){
        return OptType::PIMAPPING;
    }
    else if(opt.mapping_opt && !opt.sched_opt){
        return OptType::SPATEM;
    }
    else if(!opt.mapping_opt && opt.sched_opt){
        return OptType::HITM;
    }
    else{
        return OptType::MNSIM;
    }
}

void Analyzer::generate_analysis_result(){
    // get deploy_info
    auto cgraph = cg.get_graph();
    auto tgraph = tg.get_graph();
    // traverse tnodes to generate deploy info
    for (const auto& v : boost::make_iterator_range(boost::vertices(tgraph))) {
        // get tile id
        auto tile_id = hg.get_hnode(v);
        // get paths
        auto path_map = dg.get_path_map(v);
        std::map<int, std::vector<Path>> cur_layer_paths_map;
        for (const auto& [layer, path_id] : path_map) {
            // get paths with this tile as source, only one path per iter actually
            auto paths = dg.get_pathset({path_id});
            cur_layer_paths_map[layer].push_back(*paths[0]); // get the only path
        }
        // get cnode
        auto cnode_id = tg.get_node_property(v, tgraph).cnode_id;
        std::vector<CNode> cnode;
        std::transform(cnode_id.begin(), cnode_id.end(), std::back_inserter(cnode), [&](auto& node){
            return cg.get_node_property(node, cgraph);
        });
        // store the result
        result.deploy_info.push_back(DeployInfo{tile_id, cur_layer_paths_map, cnode});
    }
    // auto hgraph = hg.get_graph();
    // auto dgraph = dg.get_graph();
    // traverse tnodes in topo order
    // std::vector<size_t> topo_order;
    // try {
    //     boost::topological_sort(tgraph, std::back_inserter(topo_order));
    // }
    // catch(boost::not_a_dag& e) {
    //     std::cerr << "Not a DAG!" << std::endl;
    //     return;
    // }
    // std::reverse(topo_order.begin(), topo_order.end());
    // // for (const auto& v : boost::make_iterator_range(boost::vertices(tgraph))) {
    // for (const auto& v : topo_order) {
    //     auto tile_id = hg.get_hnode(v);
    //     auto child_tnodes = tg.get_adjacent_nodes(v, tgraph);
    //     std::vector<std::pair<int, int>> child_tile;
    //     child_tile.reserve(child_tnodes.size());
    //     std::transform(child_tnodes.begin(), child_tnodes.end(), std::back_inserter(child_tile), [&](auto& node){
    //         return hg.get_hnode(node);
    //     });
    //     // get paths
    //     // std::vector<Path> paths = dg.get_tpath(v);
    //     auto paths = dg.get_tpath(v);
    //     // get cnode
    //     auto cnode_id = tg.get_node_property(v, tgraph).cnode_id;
    //     // get layer
    //     auto layer = cg.get_node_property(cnode_id[0], cgraph).layer;
    //     std::vector<CNode> cnode;
    //     std::transform(cnode_id.begin(), cnode_id.end(), std::back_inserter(cnode), [&](auto& node){
    //         const auto& cnode = cg.get_node_property(node, cgraph);
    //         return cnode;
    //         // return cg.get_node_property(node, cgraph);
    //     });
    //     std::vector<Path> path_vec;
    //     for (const auto& path : paths) {
    //         path_vec.push_back(*path);
    //     }
    //     result.deploy_info.push_back(DeployInfo{layer, tile_id, child_tile, path_vec, cnode});
    // }
    // create data matrix
    auto [rows, cols] = hg.get_shape();
    auto path_segs = dg.get_path_segs();
    auto layer_segs = dg.get_layer_segs();
    for (auto i = 0;i < path_segs.size(); i++) {
        DataMatrix data(rows*cols, std::vector<int>(rows*cols, 0));
        auto paths = dg.get_pathset(path_segs[i]);
        for (const auto& path : paths) {
            auto datavolume = path->datavolume;
            for (int i = 0; i < path->via.size()-1; i++) {
                auto src = hg.xy_to_id(path->via[i]);
                auto dst = hg.xy_to_id(path->via[i+1]);
                data[src][dst] += datavolume;
            }
        }
        auto layer_seg_vec = std::vector<int>(layer_segs[i].begin(), layer_segs[i].end());
        result.comm_segs.push_back(CommSeg{layer_seg_vec, data});
    }
}

void Analyzer::generate_comm_info() {
    // get path info from path_seg
    auto path_segs = dg.get_path_segs();
    int path_num = 0;
    int datavolume = 0;
    int total_hops = 0;
    long long total_congestion = 0;
    for (const auto& seg : path_segs) {
        path_num += seg.size();
        auto paths = dg.get_pathset(seg);
        // for (const auto& path : seg) {
        for (const auto& path : paths) {
            // datavolume += path.datavolume;
            // total_hops += path.via.size() - 1;
            datavolume += path->datavolume;
            total_hops += path->via.size() - 1;
        }
    }
    // get total congestion
    for (const auto& congestion : dg.get_congestion_segs()) {
        total_congestion += congestion;
    }
    comm_info = CommInfo{path_num, datavolume, total_hops, total_congestion};
}

void Analyzer::print_result() const {
    std::cout << "Analysis Result:" << std::endl;
    // for (const auto& info : result.deploy_info) {
    //     std::cout << "Layer: " << info.layer << std::endl;
    //     std::cout << "Tile: " << info.tile_id.first << "," << info.tile_id.second << std::endl;
    //     std::cout << "Child Tile: ";
    //     for (const auto& child : info.child_tile) {
    //         std::cout << child.first << "," << child.second << " ";
    //     }
    //     std::cout << std::endl;
    //     std::cout << "Paths: " << std::endl;
    //     for (const auto& path : info.paths) {
    //         std::cout << "Path: ";
    //         for (const auto& via : path.via) {
    //             std::cout << via.first << "," << via.second << " ";
    //         }
    //         std::cout << "Volume: " << path.datavolume << std::endl;
    //     }
    //     std::cout << "CNode: " << std::endl;
    //     for (const auto& cnode : info.cnode) {
    //         std::cout << "CNode: " << cnode << std::endl;
    //     }
    // }
    // std::cout << "Data Matrix:" << std::endl;
    // for (const auto& data : result.datas) {
    //     for (const auto& row : data) {
    //         for (const auto& col : row) {
    //             std::cout << col << " ";
    //         }
    //         std::cout << std::endl;
    //     }
    //     std::cout << std::endl;
    // }
    std::cout << "Comm Segs:" << std::endl;
    for (const auto& seg : result.comm_segs) {
        std::cout << "Layers: ";
        for (const auto& layer : seg.layers) {
            std::cout << layer << " ";
        }
        std::cout << std::endl;
        for (const auto& row : seg.datas) {
            for (const auto& col : row) {
                std::cout << col << " ";
            }
            std::cout << std::endl;
        }
    }
    std::cout << "Comm Info:" << std::endl;
    std::cout << "Path num: " << comm_info.path_num << std::endl;
    std::cout << "Data volume: " << comm_info.datavolume << std::endl;
    std::cout << "Total hops: " << comm_info.total_hops << std::endl;
    std::cout << "Total congestion: " << comm_info.total_congestion << std::endl;
}
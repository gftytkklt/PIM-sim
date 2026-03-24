#include "analyzer.h"
#include <iostream>

static OptType gen_opt_type_from_optinfo(const OptInfo& opt, const bool tile2_0_flag) {
    OptType opt_type; // default optimization type
    if (tile2_0_flag) {
        opt_type = OptType::TILE2_0;
        std::cout << "Tile 2.0 optimization enabled, overriding other optimization flags." << std::endl;
        return opt_type;
    }
    if(opt.mapping_opt && opt.sched_opt){
        opt_type = OptType::PIMAPPING;
    }
    else if(opt.mapping_opt && !opt.sched_opt){
        opt_type = OptType::SPATEM;
    }
    else if(!opt.mapping_opt && opt.sched_opt){
        opt_type = OptType::HITM;
    }
    else{
        opt_type = OptType::MNSIM;
    }
    std::cout << "Opt type: " << opt_type_to_string(opt_type) << std::endl;
    return opt_type;
}

Analyzer::Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt, bool tile2_0_flag) {
    opt_type = gen_opt_type_from_optinfo(opt, tile2_0_flag);

    auto cg_strategy = createStrategy<CGraph>(opt_type);
    auto tg_strategy = createStrategy<TGraph>(opt_type);
    auto hg_strategy = createStrategy<HGraph>(opt_type);
    auto dg_strategy = createStrategy<DGraph>(opt_type);
    if (!cg_strategy || !tg_strategy || !hg_strategy || !dg_strategy) {
        throw std::runtime_error("Failed to create strategy for one of the graphs.");
    }
    if(tile2_0_flag){
        std::cout << "Using tile2.0 optimization for CGraph." << std::endl;
    }
    int cnode_capacity = info.xbar_num * info.tile_size.first * info.tile_size.second;
    cg = std::make_shared<CGraph>(kernels, info.xbar_size, cnode_capacity, cg_strategy);
    tg = std::make_shared<TGraph>(cg, info.xbar_num, tg_strategy);
    hg = std::make_shared<HGraph>(tg, cg, info.tile_size, hg_strategy);
    dg = std::make_shared<DGraph>(hg, tg, cg, info.pipeline_depth, dg_strategy);
}

void Analyzer::generate_analysis_result(){
    // get deploy_info
    auto cgraph = cg->get_graph();
    auto tgraph = tg->get_graph();
    // traverse tnodes to generate deploy info
    for (const auto& v : boost::make_iterator_range(boost::vertices(tgraph))) {
        // get tile id
        auto tile_id = hg->get_hnode(v);
        // get paths
        auto path_map = dg->get_path_map(v);
        std::map<int, std::vector<Path>> cur_layer_paths_map;
        for (const auto& [layer, path_id] : path_map) {
            // get paths with this tile as source, only one path per iter actually
            auto paths = dg->get_pathset({path_id});
            cur_layer_paths_map[layer].push_back(*paths[0]); // get the only path
        }
        // get cnode
        auto cnode_id = tg->get_node_property(v, tgraph).cnode_id;
        std::vector<CNode> cnode;
        std::transform(cnode_id.begin(), cnode_id.end(), std::back_inserter(cnode), [&](auto& node){
            return cg->get_node_property(node, cgraph);
        });
        // store the result
        result.deploy_info.push_back(DeployInfo{tile_id, cur_layer_paths_map, cnode});
    }

    // create data matrix
    auto [rows, cols] = hg->get_shape();
    auto path_segs = dg->get_path_segs();
    auto layer_segs = dg->get_layer_segs();
    for (auto i = 0;i < path_segs.size(); i++) {
        DataMatrix data(rows*cols, std::vector<int>(rows*cols, 0));
        auto paths = dg->get_pathset(path_segs[i]);
        for (const auto& path : paths) {
            auto datavolume = path->datavolume;
            for (int i = 0; i < path->via.size()-1; i++) {
                auto src = hg->xy_to_id(path->via[i]);
                auto dst = hg->xy_to_id(path->via[i+1]);
                data[src][dst] += datavolume;
            }
        }
        auto layer_seg_vec = std::vector<int>(layer_segs[i].begin(), layer_segs[i].end());
        result.comm_segs.push_back(CommSeg{layer_seg_vec, data});
    }
}

void Analyzer::generate_comm_info() {
    // get path info from path_seg
    auto path_segs = dg->get_path_segs();
    int path_num = 0;
    int datavolume = 0;
    int total_hops = 0;
    long long total_congestion = 0;
    for (const auto& seg : path_segs) {
        path_num += seg.size();
        auto paths = dg->get_pathset(seg);
        // for (const auto& path : seg) {
        for (const auto& path : paths) {
            // datavolume += path.datavolume;
            // total_hops += path.via.size() - 1;
            datavolume += path->datavolume;
            total_hops += path->via.size() - 1;
        }
    }
    // get total congestion
    for (const auto& congestion : dg->get_congestion_segs()) {
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
    // std::cout << "Comm Segs:" << std::endl;
    // for (const auto& seg : result.comm_segs) {
    //     std::cout << "Layers: ";
    //     for (const auto& layer : seg.layers) {
    //         std::cout << layer << " ";
    //     }
    //     std::cout << std::endl;
    //     for (const auto& row : seg.datas) {
    //         for (const auto& col : row) {
    //             std::cout << col << " ";
    //         }
    //         std::cout << std::endl;
    //     }
    // }
    std::cout << "Comm Info:" << std::endl;
    std::cout << "Path num: " << comm_info.path_num << std::endl;
    std::cout << "Data volume: " << comm_info.datavolume << std::endl;
    std::cout << "Total hops: " << comm_info.total_hops << std::endl;
    std::cout << "Total congestion: " << comm_info.total_congestion << std::endl;
}
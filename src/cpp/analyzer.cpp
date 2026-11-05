#include "analyzer.h"
#include <iostream>
#include "logger.h"
#include "runtime_paths.h"
#include <filesystem>
#include <fstream>

static OptType gen_opt_type_from_optinfo(const OptInfo& opt, const bool tile2_0_flag) {
    OptType opt_type;
    if (tile2_0_flag) {
        opt_type = OptType::TILE2_0;
        PIM_INFO("Tile 2.0 optimization enabled, overriding other optimization flags.");
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
    PIM_INFO("Opt type: " << opt_type_to_string(opt_type));
    return opt_type;
}

static void init_logger() {
    static bool initialized = false;
    if (initialized) return;
    initialized = true;
    pim::Logger::instance().set_log_file(pim::runtime_paths::log_file("cpp_analysis.log"));
}

Analyzer::Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt, bool tile2_0_flag) {
    init_logger();
    const bool auto_mesh = info.tile_size == std::pair{0, 0};
    if (tile2_0_flag && auto_mesh) throw pim::ConfigError("Tile2 subset selection requires an explicit hardware budget");
    if (info.xbar_size.first <= 0 || info.xbar_size.second <= 0 || info.xbar_num <= 0 ||
        (!auto_mesh && (info.tile_size.first <= 0 || info.tile_size.second <= 0)) || info.pipeline_depth <= 0) {
        throw pim::ConfigError("crossbar, mesh, capacity and pipeline depth must be positive");
    }
    if (static_cast<long long>(info.tile_size.first) * info.tile_size.second >
        std::numeric_limits<int>::max() / info.xbar_num) {
        throw pim::ConfigError("total crossbar capacity exceeds supported integer range");
    }
    opt_type = gen_opt_type_from_optinfo(opt, tile2_0_flag);

    auto cg_strategy = createStrategy<CGraph>(opt_type);
    auto tg_strategy = createStrategy<TGraph>(opt_type);
    auto hg_strategy = createStrategy<HGraph>(opt_type);
    auto dg_strategy = createStrategy<DGraph>(opt_type);
    if (!cg_strategy || !tg_strategy || !hg_strategy || !dg_strategy) {
        throw std::runtime_error("Failed to create strategy for one of the graphs.");
    }
    if(tile2_0_flag){
        PIM_INFO("Using tile2.0 optimization for CGraph.");
    }
    int cnode_capacity = info.xbar_num * info.tile_size.first * info.tile_size.second;
    int tile_num = tile2_0_flag ? info.tile_size.first * info.tile_size.second : 0;
    cg = std::make_shared<CGraph>(kernels, info.xbar_size, cnode_capacity, cg_strategy);
    tg = std::make_shared<TGraph>(cg, info.xbar_num, tg_strategy, tile_num);
    if (auto_mesh) {
        const auto nodes = boost::num_vertices(tg->get_graph());
        const auto side = std::max(1, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(nodes)))));
        if (static_cast<long long>(side) * side > std::numeric_limits<int>::max() / info.xbar_num) {
            throw pim::ConfigError("automatic mesh capacity exceeds supported integer range");
        }
        info.tile_size = {side, side};
    }
    hg = std::make_shared<HGraph>(tg, cg, info.tile_size, hg_strategy);
    dg = std::make_shared<DGraph>(hg, tg, cg, info.pipeline_depth, dg_strategy);
}

void Analyzer::generate_analysis_result(){
    result = AnalysisResult{};
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
            for (size_t i = 0; i + 1 < path->via.size(); i++) {
                auto src = hg->xy_to_id(path->via[i]);
                auto dst = hg->xy_to_id(path->via[i+1]);
                data[src][dst] = add_volume(data[src][dst], datavolume);
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
        path_num = checked_volume(static_cast<long long>(path_num) + seg.size());
        auto paths = dg->get_pathset(seg);
        // for (const auto& path : seg) {
        for (const auto& path : paths) {
            // datavolume += path.datavolume;
            // total_hops += path.via.size() - 1;
            datavolume = add_volume(datavolume, path->datavolume);
            total_hops = checked_volume(static_cast<long long>(total_hops) + path->via.size() - 1);
        }
    }
    // get total congestion
    for (const auto& congestion : dg->get_congestion_segs()) {
        if (congestion < 0 || total_congestion > std::numeric_limits<long long>::max() - congestion) {
            throw std::overflow_error("total congestion exceeds supported integer range");
        }
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

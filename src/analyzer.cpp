#include "analyzer.h"
Analyzer::Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt) :
    mapping_opt{opt.mapping_opt},
    sched_opt{opt.sched_opt},
    cg{kernels, info.xbar_size},
    tg{std::make_shared<CGraph>(cg), info.xbar_num, mapping_opt},
    hg{std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.tile_size, mapping_opt},
    dg{std::make_shared<HGraph>(hg), std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.pipeline_depth, sched_opt}
    {}

void Analyzer::generate_analysis_result(){
    // get deploy_info
    auto cgraph = cg.get_graph();
    auto tgraph = tg.get_graph();
    // auto hgraph = hg.get_graph();
    // auto dgraph = dg.get_graph();
    // traverse tnodes
    for (const auto& v : boost::make_iterator_range(boost::vertices(tgraph))) {
        auto tile_id = hg.get_hnode(v);
        auto child_tnodes = tg.get_adjacent_nodes(v, tgraph);
        std::vector<std::pair<int, int>> child_tile;
        child_tile.reserve(child_tnodes.size());
        std::transform(child_tnodes.begin(), child_tnodes.end(), std::back_inserter(child_tile), [&](auto& node){
            return hg.get_hnode(node);
        });
        // get paths
        std::vector<Path> paths = dg.get_path(v);
        // get cnode
        auto cnode_id = tg.get_node_property(v, tgraph).cnode_id;
        // get layer
        auto layer = cg.get_node_property(cnode_id[0], cgraph).layer;
        std::vector<CNode> cnode;
        std::transform(cnode_id.begin(), cnode_id.end(), std::back_inserter(cnode), [&](auto& node){
            const auto& cnode = cg.get_node_property(node, cgraph);
            return cnode;
            // return cg.get_node_property(node, cgraph);
        });
        result.deploy_info.push_back(DeployInfo{layer, tile_id, child_tile, paths, cnode});
    }
    // create data matrix
    auto [rows, cols] = hg.get_shape();
    for (const auto& seg : dg.get_path_segs()) {
        DataMatrix data(rows*cols, std::vector<int>(rows*cols, 0));
        for (const auto& path : seg) {
            auto datavolume = path.datavolume;
            for (int i = 0; i < path.via.size()-1; i++) {
                auto src = hg.xy_to_id(path.via[i]);
                auto dst = hg.xy_to_id(path.via[i+1]);
                data[src][dst] += datavolume;
            }
        }
        result.datas.push_back(data);
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
        for (const auto& path : seg) {
            datavolume += path.datavolume;
            total_hops += path.via.size() - 1;
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
    std::cout << "Comm Info:" << std::endl;
    std::cout << "Path num: " << comm_info.path_num << std::endl;
    std::cout << "Data volume: " << comm_info.datavolume << std::endl;
    std::cout << "Total hops: " << comm_info.total_hops << std::endl;
    std::cout << "Total congestion: " << comm_info.total_congestion << std::endl;
}
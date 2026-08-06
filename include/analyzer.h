#ifndef ANALYZER_H
#define ANALYZER_H
/// @file analyzer.h
/// @brief Top-level Analyzer orchestrating the full mapping pipeline.

#include "graph.h"
#include "strategy/StrategyBase.h" 

using DataMatrix = std::vector<std::vector<int>>;

/// Deployment info for a single tile: physical location, path routes, and CNode assignments.
struct DeployInfo {
    std::pair<int, int> tile_id; // location
    std::map<int, std::vector<Path>> layer_paths_map; // all paths with this tile as source
    std::vector<CNode> cnode; // cnode info
};

/// Hardware configuration parameters (crossbar size, tile grid, pipeline depth).
struct HWInfo {
    std::pair<int, int> xbar_size;  // (WL, BL) of crossbar
    int xbar_num;                   // number of xbars per tile
    std::pair<int, int> tile_size;  // (W, H) of tile array
    int pipeline_depth;             // pipeline depth for DHCG segmentation
};

/// One pipeline segment's communication data: layer indices and injection matrix.
struct CommSeg {
    std::vector<int> layers;
    DataMatrix datas;
};

/// Aggregate communication statistics for a mapping result.
struct CommInfo {
    int path_num;
    int datavolume;
    int total_hops;
    long long total_congestion;
};

/// Mapping result: tile deployment info and per-segment communication data.
struct AnalysisResult {
    std::vector<DeployInfo> deploy_info;
    std::vector<CommSeg> comm_segs;
};

/// Optimization flags controlling mapping and scheduling strategies.
struct OptInfo {
    bool mapping_opt;  // true: PIMAPPING/SPATEM mapping, false: MNSIM baseline
    bool sched_opt;    // true: BCE congestion-aware routing, false: XY routing
};

/// Top-level orchestrator: builds the four-level graph hierarchy and generates results.
class Analyzer {
public:
    Analyzer() = default;
    /// Constructs the full analysis pipeline: CGraph → TGraph → HGraph → DGraph.
    Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt, bool tile2_0_flag = false);
    AnalysisResult get_analysis_result() const { return result; }
    void generate_analysis_result();
    CommInfo get_comm_info() const { return comm_info; }
    void generate_comm_info();
    void print_result() const;
private:
    OptType opt_type;
    // CGraph cg;
    // TGraph tg;
    // HGraph hg;
    // DGraph dg;
    std::shared_ptr<CGraph> cg;
    std::shared_ptr<TGraph> tg;
    std::shared_ptr<HGraph> hg;
    std::shared_ptr<DGraph> dg;
    AnalysisResult result;
    CommInfo comm_info;
    // OptType gen_opt_type(OptInfo opt);
};
#endif
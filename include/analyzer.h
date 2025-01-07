#ifndef ANALYZER_H
#define ANALYZER_H
#include "graph.h"

using DataMatrix = std::vector<std::vector<int>>;

struct DeployInfo {
    std::pair<int, int> tile_id; // location
    std::vector<std::pair<int, int>> child_tile; // child location
    std::vector<Path> paths; // all paths with this tile as source
    int compute_volume; // compute volume
};

struct HWInfo {
    // (WL, BL) of tile array
    std::pair<int, int> xbar_size;
    // number of xbar in a tile
    int xbar_num;
    // (W, H) of tile array
    std::pair<int, int> tile_size;
    // pipeline depth
    int pipeline_depth;
};

struct OptInfo {
    bool mapping_opt;
    bool sched_opt;
};

class Analyzer {
public:
    Analyzer() = default;
    Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt);
    struct AnalysisResult {
        std::vector<DeployInfo> deploy_info; // deployment info of chip
        std::vector<DataMatrix> datas; // transfer data volume matricies
    };
    AnalysisResult get_analysis_result() const { return result; }
    void generate_analysis_result();
private:
    bool mapping_opt = true;
    bool sched_opt = true;
    CGraph cg;
    TGraph tg;
    HGraph hg;
    DGraph dg;
    AnalysisResult result;
};
#endif
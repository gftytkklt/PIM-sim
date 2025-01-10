#ifndef ANALYZER_H
#define ANALYZER_H
#include "graph.h"

using DataMatrix = std::vector<std::vector<int>>;

struct DeployInfo {
    int layer;
    std::pair<int, int> tile_id; // location
    std::vector<std::pair<int, int>> child_tile; // child location
    std::vector<Path> paths; // all paths with this tile as source
    std::vector<CNode> cnode; // cnode info
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

struct AnalysisResult {
    std::vector<DeployInfo> deploy_info; // deployment info of chip
    std::vector<DataMatrix> datas; // transfer data volume matricies
};

struct OptInfo {
    bool mapping_opt;
    bool sched_opt;
};

class Analyzer {
public:
    Analyzer() = default;
    Analyzer(const std::vector<NNkernel> kernels, HWInfo info, OptInfo opt);
    AnalysisResult get_analysis_result() const { return result; }
    void generate_analysis_result();
    void print_result() const;
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
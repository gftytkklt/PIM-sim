#ifndef ANALYZER_H
#define ANALYZER_H
#include "graph.h"

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

class Analyzer {
public:
    Analyzer() = default;
    Analyzer(const std::vector<NNkernel> kernels, HWInfo info);
private:
    CGraph cg;
    TGraph tg;
    HGraph hg;
    DGraph dg;
};

#endif
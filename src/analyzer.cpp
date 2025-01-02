#include "analyzer.h"
Analyzer::Analyzer(const std::vector<NNkernel> kernels, HWInfo info) :
    cg{kernels, info.xbar_size},
    tg{std::make_shared<CGraph>(cg), info.xbar_num},
    hg{std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.tile_size},
    dg{std::make_shared<HGraph>(hg), std::make_shared<TGraph>(tg), std::make_shared<CGraph>(cg), info.pipeline_depth}
    {}
#include "strategy/StrategyBase.h"
#include "graph.h"

void CStrategyDefault::analysis(CGraph& graph) {
    // Replication needs explicit spatial ranges and halo dependencies. The
    // current NNkernel/CNode contract cannot represent these, so optimization
    // must not silently duplicate weights and divide feature-map volumes.
}

void CStrategyMNSIM::analysis(CGraph& graph) {
}

void CStrategyTILE2_0::analysis(CGraph& graph) {
    graph.build_graph_subset();
}

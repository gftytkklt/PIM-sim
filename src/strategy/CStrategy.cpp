#include "strategy/StrategyBase.h"
#include "graph.h"

void CStrategyDefault::analysis(CGraph& graph) {
    graph.create_dup_num();
}

void CStrategyMNSIM::analysis(CGraph& graph) {
}

void CStrategyTILE2_0::analysis(CGraph& graph) {
    graph.build_graph_subset();
}
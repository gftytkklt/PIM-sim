#include "strategy/StrategyBase.h"
#include "graph.h"

void DStrategyDefault::analysis(DGraph& graph) {
    graph.set_harbor();
    graph.set_sdg();
    graph.xy_routing();
}

void DStrategyPIMAPPING::analysis(DGraph& graph) {
    graph.set_harbor();
    graph.set_sdg();
    graph.bce_routing();
}

void DStrategyTILE2_0::analysis(DGraph& graph) {
    graph.set_harbor();
    graph.set_sdg();
    graph.bce_routing();
}
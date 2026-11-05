#include "strategy/StrategyBase.h"
#include "graph.h"

void HStrategyMNSIM::analysis(HGraph& graph) {
    graph.init_hw_setting();
    graph.zigzag_mapping();
    graph.init_path();
}

void HStrategyPIMAPPING::analysis(HGraph& graph) {
    graph.init_hw_setting();
    graph.greedy_mapping();
    graph.init_path();
}

void HStrategySPATEM::analysis(HGraph& graph) {
    graph.init_hw_setting();
    graph.SPATEM_mapping();
    graph.init_path();
}
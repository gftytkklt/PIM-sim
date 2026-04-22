#include "strategy/StrategyBase.h"
// #include "strategy/StrategyFactory.h"
#include "graph.h"

// CGraph策略实现
void CStrategyDefault::analysis(CGraph& graph) {
    graph.create_dup_num();
}

void CStrategyMNSIM::analysis(CGraph& graph) {
}

void CStrategyTILE2_0::analysis(CGraph& graph) {
    /* Note: create_dup_num will extend the tile_num for duplication,
     * which is conflicted with the fixed tile_num constraint hold in build_graph_subset.
     * Future work is to unify the two methods by modify some exec conditions.
     */
    graph.build_graph_subset();
    // graph.create_dup_num();
}

// TGraph策略实现
void TStrategyMNSIM::analysis(TGraph& graph) {
    graph.create_tnodes_MNSIM();
    graph.create_TDep();
    graph.inter_tile_conn();
}

void TStrategyPIMAPPING::analysis(TGraph& graph) {
    graph.create_tnodes_PIMAPPING();
    graph.create_TDep();
    graph.inter_tile_conn();
}

void TStrategySPATEM::analysis(TGraph& graph) {
    graph.create_tnodes_SPATEM();
    graph.create_TDep();
    graph.inter_tile_conn();
}

void TStrategyPUMA::analysis(TGraph& graph) {
    graph.create_tnodes_PUMA();
    graph.create_TDep();
    graph.inter_tile_conn();
}

void TStrategyTILE2_0::analysis(TGraph& graph) {
    // graph.create_tnodes_PIMAPPING();
    graph.create_tnodes_TILE2_0();
    graph.create_TDep();
    graph.inter_tile_conn();
}

// HGraph策略实现
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

// DGraph策略实现
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
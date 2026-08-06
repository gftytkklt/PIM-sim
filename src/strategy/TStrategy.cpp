#include "strategy/StrategyBase.h"
#include "graph.h"

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

void TStrategyTILE2_0::analysis(TGraph& graph) {
    graph.create_tnodes_TILE2_0();
    graph.create_TDep();
    graph.inter_tile_conn();
}
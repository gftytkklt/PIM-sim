#include "strategy/StrategyBase.h"
#include "graph.h"

// CGraph策略实现
class CStrategyDefault : public CStrategyBase {
public:
    void analysis(CGraph& graph) override {
        graph.create_dup_num();
        graph.create_cnodes();
        graph.conn_accblk();
        graph.inter_layer_conn();
    }
};

class CStrategyMNSIM : public CStrategyBase {
public:
    void analysis(CGraph& graph) override {
        // graph.create_dup_num();
        graph.create_cnodes();
        graph.conn_accblk();
        graph.inter_layer_conn();
    }
};

// TGraph策略实现
class TStrategyMNSIM : public TStrategyBase {
public:
    void analysis(TGraph& graph) override {
        graph.create_tnodes_MNSIM();
        graph.create_TDep();
        graph.inter_tile_conn();
    }
};

class TStrategyPIMAPPING : public TStrategyBase {
public:
    void analysis(TGraph& graph) override {
        graph.create_tnodes_PIMAPPING();
        graph.create_TDep();
        graph.inter_tile_conn();
    }
};

class TStrategySPATEM : public TStrategyBase {
public:
    void analysis(TGraph& graph) override {
        graph.create_tnodes_SPATEM();
        graph.create_TDep();
        graph.inter_tile_conn();
    }
};

// HGraph策略实现
class HStrategyMNSIM : public HStrategyBase {
public:
    void analysis(HGraph& graph) override {
        graph.init_hw_setting();
        graph.zigzag_mapping();
        graph.init_path();
    }
};

class HStrategyPIMAPPING : public HStrategyBase {
public:
    void analysis(HGraph& graph) override {
        graph.init_hw_setting();
        graph.greedy_mapping();
        graph.init_path();
    }
};

class HStrategySPATEM : public HStrategyBase {
public:
    void analysis(HGraph& graph) override {
        graph.init_hw_setting();
        graph.SPATEM_mapping();
        graph.init_path();
    }
};

// DGraph策略实现
class DStrategyDefault : public DStrategyBase {
public:
    void analysis(DGraph& graph) override {
        graph.set_harbor();
        graph.set_sdg();
        graph.xy_routing();
    }
};

class DStrategyPIMAPPING : public DStrategyBase {
public:
    void analysis(DGraph& graph) override {
        graph.set_harbor();
        graph.set_sdg();
        graph.bce_routing();
    }
};

// 策略工厂函数实现
std::shared_ptr<CStrategyBase> createCStrategy(OptType opt_type) {
    switch (opt_type) {
        case OptType::MNSIM: return std::make_shared<CStrategyMNSIM>();
        default: return std::make_shared<CStrategyDefault>();
    }
}

std::shared_ptr<TStrategyBase> createTStrategy(OptType opt_type) {
    switch (opt_type) {
        case OptType::SPATEM: return std::make_shared<TStrategySPATEM>();
        case OptType::PIMAPPING: return std::make_shared<TStrategyPIMAPPING>();
        case OptType::MNSIM:
        default: return std::make_shared<TStrategyMNSIM>();
    }
}

std::shared_ptr<HStrategyBase> createHStrategy(OptType opt_type) {
    switch (opt_type) {
        case OptType::SPATEM: return std::make_shared<HStrategySPATEM>();
        case OptType::PIMAPPING:  return std::make_shared<HStrategyPIMAPPING>();
        case OptType::MNSIM:
        default:  return std::make_shared<HStrategyMNSIM>();
    }
}

std::shared_ptr<DStrategyBase> createDStrategy(OptType opt_type) {
    switch (opt_type) {
        case OptType::PIMAPPING: return std::make_shared<DStrategyPIMAPPING>();
        default: return std::make_shared<DStrategyDefault>();
    }
}
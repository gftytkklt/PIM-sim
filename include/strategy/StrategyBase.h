#ifndef STRATEGYBASE_H
#define STRATEGYBASE_H

#include <memory>
#include <unordered_map>
#include <functional>
#include <type_traits>
#include "util.h"

class CGraph;
class TGraph;
class HGraph;
class DGraph;

template <typename GraphType>
class StrategyBase {
public:
    virtual void analysis(GraphType& graph){
        
    }
    virtual ~StrategyBase() = default;
};


class CStrategyDefault : public StrategyBase<CGraph> {
public:
    void analysis(CGraph& graph) override;
};

class CStrategyMNSIM : public StrategyBase<CGraph> {
public:
    void analysis(CGraph& graph) override;
};

class CStrategyTILE2_0 : public StrategyBase<CGraph> {
public:
    void analysis(CGraph& graph) override;
};

class TStrategyMNSIM : public StrategyBase<TGraph> {
public:
    void analysis(TGraph& graph) override;
};

class TStrategyPIMAPPING : public StrategyBase<TGraph> {
public:
    void analysis(TGraph& graph) override;
};

class TStrategySPATEM : public StrategyBase<TGraph> {
public:
    void analysis(TGraph& graph) override;
};

class TStrategyTILE2_0 : public StrategyBase<TGraph> {
public:
    void analysis(TGraph& graph) override;
};

class HStrategyMNSIM : public StrategyBase<HGraph> {
public:
    void analysis(HGraph& graph) override;
};

class HStrategyPIMAPPING : public StrategyBase<HGraph> {
public:
    void analysis(HGraph& graph) override;
};

class HStrategySPATEM : public StrategyBase<HGraph> {
public:
    void analysis(HGraph& graph) override;
};

class DStrategyDefault : public StrategyBase<DGraph> {
public:
    void analysis(DGraph& graph) override;
};

class DStrategyPIMAPPING : public StrategyBase<DGraph> {
public:
    void analysis(DGraph& graph) override;
};

class DStrategyTILE2_0 : public StrategyBase<DGraph> {
public:
    void analysis(DGraph& graph) override;
};

// 统一创建策略的模板函数
template<typename GraphType>
std::shared_ptr<StrategyBase<GraphType>> createStrategy(OptType opt_type) {
    // 为每个GraphType特化处理
    if constexpr (std::is_same_v<GraphType, CGraph>) {
        switch (opt_type) {
            case OptType::MNSIM: return std::make_shared<CStrategyMNSIM>();
            case OptType::TILE2_0: return std::make_shared<CStrategyTILE2_0>();
            case OptType::HITM:
            case OptType::SPATEM:
            case OptType::PIMAPPING:
            default: return std::make_shared<CStrategyDefault>();
        }
    } 
    else if constexpr (std::is_same_v<GraphType, TGraph>) {
        switch (opt_type) {
            case OptType::PIMAPPING: return std::make_shared<TStrategyPIMAPPING>();
            case OptType::TILE2_0: return std::make_shared<TStrategyTILE2_0>();
            case OptType::SPATEM: return std::make_shared<TStrategySPATEM>();
            case OptType::MNSIM:
            case OptType::HITM:
            default: return std::make_shared<TStrategyMNSIM>();
        }
    }
    else if constexpr (std::is_same_v<GraphType, HGraph>) {
        switch (opt_type) {
            case OptType::SPATEM: return std::make_shared<HStrategySPATEM>();
            case OptType::TILE2_0:
            case OptType::PIMAPPING: return std::make_shared<HStrategyPIMAPPING>();
            case OptType::MNSIM:
            case OptType::HITM:
            default: return std::make_shared<HStrategyMNSIM>();
        }
    }
    else if constexpr (std::is_same_v<GraphType, DGraph>) {
        switch (opt_type) {
            case OptType::PIMAPPING: return std::make_shared<DStrategyPIMAPPING>();
            case OptType::TILE2_0: return std::make_shared<DStrategyTILE2_0>();
            case OptType::MNSIM:
            case OptType::HITM:
            case OptType::SPATEM:
            default: return std::make_shared<DStrategyDefault>();
        }
    }
    else {
        static_assert(sizeof(GraphType) == 0, "Unsupported GraphType");
        return nullptr;
    }
}

using CStrategyBase = StrategyBase<CGraph>;
using TStrategyBase = StrategyBase<TGraph>;
using HStrategyBase = StrategyBase<HGraph>;
using DStrategyBase = StrategyBase<DGraph>;

#endif
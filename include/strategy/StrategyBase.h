#ifndef STRATEGYBASE_H
#define STRATEGYBASE_H

#include <memory>
#include "util.h"

class CGraph;
class TGraph;
class HGraph;
class DGraph;

template <typename GraphType>
class StrategyBase {
public:
    virtual void analysis(GraphType& graph) = 0;
    virtual ~StrategyBase() = default;
};

class CStrategyBase : public StrategyBase<CGraph> {
public:
    virtual void analysis(CGraph& graph) override = 0;
};

class TStrategyBase : public StrategyBase<TGraph> {
public:
    virtual void analysis(TGraph& graph) override = 0;
};

class HStrategyBase : public StrategyBase<HGraph> {
public:
    virtual void analysis(HGraph& graph) override = 0;
};

class DStrategyBase : public StrategyBase<DGraph> {
public:
    virtual void analysis(DGraph& graph) override = 0;
};

std::shared_ptr<CStrategyBase> createCStrategy(OptType opt_type);
std::shared_ptr<TStrategyBase> createTStrategy(OptType opt_type);
std::shared_ptr<HStrategyBase> createHStrategy(OptType opt_type);
std::shared_ptr<DStrategyBase> createDStrategy(OptType opt_type);

#endif
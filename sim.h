#ifndef SIM_H
#define SIM_H

#include <memory>
#include <vector>
#include <unordered_map>
#include "module.h"
// abstract basemodel, define base tile programming status
template<typename Task>
class BasePIMModel{
public:
    BasePIMModel() = default;
    BasePIMModel(const BasePIMModel&) = delete;
    BasePIMModel(BasePIMModel&&) = delete;
    BasePIMModel& operator=(const BasePIMModel&) = delete;
    BasePIMModel& operator=(BasePIMModel&&) = delete;
    virtual ~BasePIMModel() = default;

    // virtual void exec_task(const Task& task) = 0;// exec abstract task, may include cal num, size...
    // virtual const Task& read_act() = 0;// read act already stored in the tile
    // virtual void write_result(Task& task) = 0;// write result caled by the tile
};

// abstract functional tile model, define base tile resources
template<typename Task, typename Memory>
class Simtile : public BasePIMModel<Task>{
public:
    Simtile() = default;
    virtual void run() = 0;// abstract simulator run
    // virtual void set_memory(std::shared_ptr<Memory> m) = 0;// set specific hierarchical memory
    // virtual void set_task(std::shared_ptr<Task> t) = 0;// set task property
    static std::shared_ptr<Simtile> create_simtile();
};

// abstract timing-tile model, define base clock behavior
template<typename Task, typename Memory>
class TimingSimtile : public Simtile<Task, Memory>{
public:
    virtual void clock() = 0;
    static std::shared_ptr<TimingSimtile> create_simtile();
};

// perf model interface, use this to instancing model
enum class NodeState {
    NotVisited,
    Visiting,
    Visited
};
template<typename Task, typename Memory, ModuleConcept Module>
class PerfModel : public TimingSimtile<Task, Memory>{
public:
    explicit PerfModel(std::shared_ptr<Module> root) : root(std::move(root)) {topologicalSort();}
    void clock() final;// run modl
    void run() final;
private:
    std::shared_ptr<Module> root;// root of hardware DAG(data fwd resolved by global data individually)
    std::vector<std::shared_ptr<Module>> simList;// sim order of module list
    uint64_t cur_cycle = 0;// sim time counter
    void topologicalSort();
    void dfs(std::shared_ptr<Module> node, std::unordered_map<Module*, NodeState>& states);
};


#include "sim.hpp"
#endif
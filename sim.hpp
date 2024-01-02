#include "sim.h"


// sort elem in DAG in simulation order
template<typename Task, typename Memory, ModuleConcept Module>
void PerfModel<Task, Memory, Module>::topologicalSort(){
    std::unordered_map<Module*, NodeState> states;
    dfs(root, states);
    simList.pop_back();
}

template<typename Task, typename Memory, ModuleConcept Module>
void PerfModel<Task, Memory, Module>::dfs(std::shared_ptr<Module> node, std::unordered_map<Module*, NodeState>& states){
    // end of DAG or visited node: do nothing
    if (!node || states[node.get()] == NodeState::Visited) {
        return;
    }
    // loop detection
    if (states[node.get()] == NodeState::Visiting) {
        throw std::runtime_error("Detected a cycle in the graph");
    }
    // mark node to visited
    states[node.get()] = NodeState::Visiting;
    // search node recursively
    for (auto& nextNode : node->next) {
        dfs(nextNode, states);
    }
    // leaf node: marked visited & add to list
    states[node.get()] = NodeState::Visited;
    simList.push_back(node);
}

template<typename Task, typename Memory, ModuleConcept Module>
void PerfModel<Task, Memory, Module>::clock() {
    for (auto& module : simList){
        module->exec();
    }
    cur_cycle++;
}

template<typename Task, typename Memory, ModuleConcept Module>
void PerfModel<Task, Memory, Module>::run(){
    clock();
}
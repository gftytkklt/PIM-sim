#include "sim.h"


// sort elem in DAG in simulation order
template<TaskConcept Task>
void PerfModel<Task>::topologicalSort(){
    std::unordered_map<Module*, NodeState> states;
    dfs(this->root, states);
    simList.pop_back();
}

// template<typename Task, typename Memory>
// void PerfModel<Task, Memory>::dfs(Module* node, std::unordered_map<Module*, NodeState>& states){
//     // end of DAG or visited node: do nothing
//     if (!node || states[node] == NodeState::Visited) {
//         return;
//     }
//     // loop detection
//     if (states[node] == NodeState::Visiting) {
//         throw std::runtime_error("Detected a cycle in the graph");
//     }
//     // mark node to visited
//     states[node] = NodeState::Visiting;
//     // search node recursively
//     for (auto& nextNode : node->getNext()) {
//         dfs(nextNode.get(), states);
//     }
//     // leaf node: marked visited & add to list
//     states[node] = NodeState::Visited;
//     simList.push_back(node);
// }

template<TaskConcept Task>
void PerfModel<Task>::dfs(std::shared_ptr<Module> node, std::unordered_map<Module*, NodeState>& states){
    // end of DAG or visited node: do nothing
    // std::cout << "dfs loop" << std::endl;
    if (!node || states[node.get()] == NodeState::Visited) {
        return;
    }
    // loop detection
    if (states[node.get()] == NodeState::Visiting) {
        throw std::runtime_error("Detected a cycle in the graph");
    }
    // mark node to visited
    // std::cout << "dfs loop2" << std::endl;
    states[node.get()] = NodeState::Visiting;
    // search node recursively
    for (auto& nextNode : node->getNext()) {
        // std::cout << "dfs inner loop" << std::endl;
        dfs(nextNode, states);
    }
    // leaf node: marked visited & add to list
    states[node.get()] = NodeState::Visited;
    // node.get()->exec();
    simList.push_back(node);
}

template<TaskConcept Task>
void PerfModel<Task>::clock() {
    for (auto& module : simList){
        module->exec();
    }
    cur_cycle++;
}

template<TaskConcept Task>
void PerfModel<Task>::run(){
    clock();
}
#ifndef MODULE_H
#define MODULE_H
#include <vector>
#include <memory>
#include <concepts>
#include <iostream>
// #include "sim.h"

// template<typename T>
// concept ModuleConcept = requires(T a) {
//     { a.next } -> std::same_as<std::vector<std::shared_ptr<T>>&>;
//     { a.exec() } -> std::same_as<void>;
// };
template<typename T>
concept TaskConcept = requires {
    typename T::Inst; // Task must have a Inst class for memory init
};

class Module{
public:
    // for PerfSim parent init
    Module(std::string n) : name(std::move(n)) {}
    
    // for module init
    Module(std::string n, Module* parent) : name(std::move(n)), parent(parent) {
        if(parent != nullptr){
            parent->addNext(std::shared_ptr<Module>(this));
        }
    }

    void addNext(std::shared_ptr<Module> module) {
        // std::cout << "addNext" << std::endl;
        next.push_back(module);
    }

    void addParent(Module* module) {
        parent = module;
    }

    auto getNext(){
        return next;
    }

    auto getName(){
        return name;
    }

    virtual void exec() {
        // 模拟执行模块的功能
        std::cout << "TODO: impl Exec func of module " << name << std::endl;
    }
private:
    std::string name;
    std::vector<std::shared_ptr<Module>> next; // pointer to DAG children
    Module* parent=nullptr; // pointer to PerfSim parent
};

#endif
#ifndef SCH_H
#define SCH_H
#include <iostream>
#include <vector>
#include "util.h"
struct Path;
template <typename GraphType>
class Scheduler {
public:
    Scheduler() = default;
    Scheduler(const GraphType& graph) : graph{graph} {}; 
    // use & to schedule via scheduler directly
    void schedule(std::vector<Path>& path_set);
private:
    GraphType graph;
};

#include "scheduler.hpp"
#endif
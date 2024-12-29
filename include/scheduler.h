#ifndef SCH_H
#define SCH_H
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <boost/graph/adjacency_list.hpp>
#include "util.h"
template <typename GraphType>
class Scheduler {
public:
    using Vertex = boost::graph_traits<GraphType>::vertex_descriptor;
    using Edge = boost::graph_traits<GraphType>::edge_descriptor;
    Scheduler() = default;
    Scheduler(const GraphType& graph, std::pair<int, int> tile_size) 
    : graph{graph}, tile_size(tile_size) {
        // init edge_map
        size_t id = 0;
        for (const auto& e : boost::make_iterator_range(edges(graph))) {
            edge_map[e] = id++;
        }
    };
    // use & to schedule via scheduler directly
    void set_path_set(std::vector<Path> path_set) {
        this->path_set = std::make_shared<std::vector<Path>>(path_set);
    }
    void schedule();
    auto id_to_xy(size_t id) const {
        return std::make_pair(id / tile_size.second, id % tile_size.second);
    }
    size_t xy_to_id(std::pair<int, int> xy) const {
        return xy.first * tile_size.second + xy.second;
    }
private:
    GraphType graph;
    std::shared_ptr<std::vector<Path>> path_set;
    std::pair<int, int> tile_size;
    std::map<Edge, size_t> edge_map;// edge, id map pair
    std::map<size_t, double> bce_map; // (path_id, bce)
    void init_bce();
};

#include "scheduler.hpp"
#endif
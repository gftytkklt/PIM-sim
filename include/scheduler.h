#ifndef SCH_H
#define SCH_H
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <boost/graph/adjacency_list.hpp>
#include <boost/graph/dijkstra_shortest_paths.hpp>
#include <boost/property_map/property_map.hpp>
#include <boost/property_map/function_property_map.hpp>
#include "util.h"
#include "errors.h"
class CSum {
public:
    void insert(int value) {
        if (value < 0) throw pim::SchedulingError("congestion volume must be nonnegative");
        values.insert(value);
        total += value;
    }

    // Equation (3-26): sorted v[0..n-1], sum(v[0..n-2]) + v[n-2].
    // A link with at most one positive-volume flow has no contention (cost 1).
    long long getCSum() const {
        if (values.size() < 2) return 1;
        auto largest = values.rbegin();
        auto second_largest = std::next(largest);
        return total - *largest + *second_largest;
    }

    long long getCSum(int value) const {
        if (value < 0) throw pim::SchedulingError("congestion volume must be nonnegative");
        if (values.empty()) return 1;
        auto largest = values.rbegin();
        if (values.size() == 1) return 2LL * std::min(*largest, value);
        if (value >= *largest) return total + *largest;
        return total + value - *largest + std::max(*std::next(largest), value);
    }

private:
    std::multiset<int> values;
    long long total = 0;
};

using SNode = std::pair<int,int>; // (x, y) of node
using SGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, SNode>;
using Vertex = boost::graph_traits<SGraph>::vertex_descriptor;
using Edge = boost::graph_traits<SGraph>::edge_descriptor;

// use for calculate weight of path to be scheduled
// only path within min manhattan distance will be scheduled
struct WeightCalculator {
    // scheduler member
    const SGraph& graph;
    const std::map<UnorderedPair, size_t>& edge_map;
    const std::map<size_t, double>& bce_map;
    const std::map<size_t, CSum>& congestion_map;
    std::pair<int, int> tile_size; // tile size
    std::pair<int, int> x_range, y_range; // constrained range
    // current path data volume
    int value;
    std::pair<int, int> id_to_xy(size_t id) const {
        return std::make_pair(id / tile_size.second, id % tile_size.second);
    }
    bool out_of_range(size_t id) const {
        auto xy = id_to_xy(id);
        return xy.first < x_range.first || xy.first > x_range.second || xy.second < y_range.first || xy.second > y_range.second;
    }
    
    double operator()(const Edge& e) const {
        size_t src = boost::source(e, graph);
        size_t dst = boost::target(e, graph);
        auto id = edge_map.at(UnorderedPair{src, dst});
        // size_t id = edge_map[UnorderedPair{src, dst}];
        // std::cout << "bce: " << bce_map.at(id) << " congestion: " << congestion_map.at(id).getCSum(value) << std::endl;
        if (bce_map.find(id) == bce_map.end()) {
            return std::numeric_limits<double>::infinity();
        }
        // edge out of range
        if (out_of_range(src) || out_of_range(dst)) {
            return std::numeric_limits<double>::infinity();
        }
        return bce_map.at(id) * congestion_map.at(id).getCSum(value);
    }
};

class Scheduler {
public:
    Scheduler() = default;
    Scheduler(std::pair<int, int> tile_size);
    void set_path_set(std::vector<std::shared_ptr<Path>> path_set);
    // return congestion volume of cur seg
    long long schedule();
    long long xy_routing();
    auto id_to_xy(size_t id) const {
        return std::make_pair(static_cast<int>(id / tile_size.second), static_cast<int>(id % tile_size.second));
    }
    size_t xy_to_id(std::pair<int, int> xy) const {
        return xy.first * tile_size.second + xy.second;
    }
private:
    SGraph graph;
    std::vector<std::shared_ptr<Path>> path_set;
    std::pair<int, int> tile_size{0, 0};
    std::map<UnorderedPair, size_t> edge_map;// edge, id map pair
    std::map<size_t, double> bce_map; // (path_id, bce)
    std::map<size_t, CSum> congestion_map; // (path_id, congestion)
    void init_bce();
    void congestion_aware_routing();
    
};


#endif

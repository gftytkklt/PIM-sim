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
class CSum {
public:
    CSum() : sumLower(0), n(0) {}
    // insert and update sum
    void insert(int value) {
        n++;

        // insert value to lowerSet or higherSet
        // first elem will be inserted to higherSet in fact
        if (lowerSet.empty() || value <= *lowerSet.rbegin()) {
            lowerSet.insert(value);
            sumLower += value;
        } else {
            higherSet.insert(value);
        }

        // move the largest element in lowerSet to higherSet
        // this will happen if value <= *lowerSet.rbegin()
        if (lowerSet.size() > n - 1) {
            auto it = --lowerSet.end();
            sumLower -= *it;
            higherSet.insert(*it);
            lowerSet.erase(it);
        }

        // move the smallest element in higherSet to lowerSet
        // this will happen if value > *lowerSet.rbegin()
        // I think the second condition is not necessary
        while (lowerSet.size() < n - 1 && !higherSet.empty()) {
            auto it = higherSet.begin();
            lowerSet.insert(*it);
            sumLower += *it;
            higherSet.erase(it);
        }
    }

    // get sum = sum(v[0],v[1],...,v[n-2]) + v[n-2]
    long long getCSum() const {
        // 0/1 elem case
        if (lowerSet.empty()) {
            return 1;
        }
        auto it = lowerSet.end();
        --it; // v[n-2]
        return sumLower + (*it);
    }

    // get Csum of original data and new value
    // value will not be inserted
    long long getCSum(int value) const {
        // empty set
        if (n < 1) {
            return 1;
        }
        // n >= 1, non-empty higherSet
        auto max_original = *higherSet.begin();
        // only elem in higherSet
        if (n == 1) {
            return 2*std::min(max_original, value);
        }
        // n >= 2, general case
        if (value >= max_original) {
            return sumLower + 2*max_original;
        }
        auto minmax_original = *lowerSet.rbegin();
        return sumLower + value + std::max(minmax_original, value);
    }
private:
    std::multiset <int> lowerSet;  // v[0],v[1],...,v[n-2]
    std::multiset <int> higherSet; // v[n-1], sometimes candidate
    long long sumLower;     // sum(v[0],v[1],...,v[n-2]) + v[n-2]
    size_t n;               // elem num of CSum
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

// using SchedInfo = std::pair<long long, std::vector<Path>>;
using SchedInfo = std::pair<long long, std::vector<std::shared_ptr<Path>>>;

class Scheduler {
public:
    Scheduler() = default;
    Scheduler(std::pair<int, int> tile_size);
    // use & to schedule via scheduler directly
    // void set_path_set(std::vector<Path> path_set) {
    //     this->path_set = std::make_shared<std::vector<Path>>(path_set);
    // }
    void set_path_set(std::vector<std::shared_ptr<Path>> path_set) {
        this->path_set = path_set;
    }
    // std::vector<Path> schedule();
    SchedInfo schedule();
    SchedInfo xy_routing();
    auto id_to_xy(size_t id) const {
        return std::make_pair(id / tile_size.second, id % tile_size.second);
    }
    size_t xy_to_id(std::pair<int, int> xy) const {
        return xy.first * tile_size.second + xy.second;
    }
private:
    SGraph graph;
    // std::shared_ptr<std::vector<Path>> path_set;
    std::vector<std::shared_ptr<Path>> path_set;
    std::pair<int, int> tile_size;
    std::map<UnorderedPair, size_t> edge_map;// edge, id map pair
    std::map<size_t, double> bce_map; // (path_id, bce)
    std::map<size_t, CSum> congestion_map; // (path_id, congestion)
    void init_bce();
    void congestion_aware_routing();
    
};

// struct constrained_dijkstra_visitor : boost::default_dijkstra_visitor {
//     std::pair<int, int> s, t;
//     std::pair<int, int> x_range, y_range;
//     const Scheduler& scheduler;

//     constrained_dijkstra_visitor(std::pair<int, int> s, std::pair<int, int> t, const Scheduler& scheduler) 
//     : s(s), t(t), x_range{std::min(s.first, t.first), std::max(s.first,t.first)}, 
//     y_range{std::min(s.second, t.second), std::max(s.second,t.second)}, scheduler(scheduler){}

//     template <typename Edge, typename Graph>
//     void edge_relaxed(Edge e, const Graph& g) const {
//         auto dst = boost::target(e, g);
//         auto dst_tile = scheduler.id_to_xy(dst);
//         if (dst_tile.first < x_range.first || dst_tile.first > x_range.second || dst_tile.second < y_range.first || dst_tile.second > y_range.second) {
//             throw std::runtime_error("Constrained edge found");
//         }
//     }
// };

#endif
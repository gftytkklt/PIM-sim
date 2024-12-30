#ifndef SCH_H
#define SCH_H
#include <iostream>
#include <vector>
#include <queue>
#include <stack>
#include <boost/graph/adjacency_list.hpp>
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

class Scheduler {
public:
    using SNode = std::pair<int,int>; // (x, y) of node
    // using SEdge = CSum; // datavolume list with metric computation
    // struct SEdge {
    //     std::vector<size_t> path_id;
    //     double bce;
    //     double congestion_volume; // init with 1 for non-zero multiplication
    // };
    using SGraph = boost::adjacency_list<boost::vecS, boost::vecS, boost::undirectedS, SNode>;
    using Vertex = boost::graph_traits<SGraph>::vertex_descriptor;
    using Edge = boost::graph_traits<SGraph>::edge_descriptor;
    Scheduler() = default;
    Scheduler(std::pair<int, int> tile_size);
    
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
    SGraph graph;
    std::shared_ptr<std::vector<Path>> path_set;
    std::pair<int, int> tile_size;
    std::map<UnorderedPair, size_t> edge_map;// edge, id map pair
    std::map<size_t, double> bce_map; // (path_id, bce)
    std::map<size_t, CSum> congestion_map; // (path_id, congestion)
    void init_bce();
    void congestion_aware_routing();
};

#endif
#ifndef MAPPER_H
#define MAPPER_H
#include <iostream>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <algorithm>
#include <limits>
#include <cmath>

// hash table for pair<int, int>
struct pair_hash {
    std::size_t operator()(const std::pair<int, int>& p) const {
        return std::hash<int>()(p.first) ^ (std::hash<int>()(p.second) << 1);
    }
};

// nodes group
struct Group {
    std::vector<int> nodes; // node index
};

// manhattan distance
int manhattan_distance(int x1, int y1, int x2, int y2) {
    return std::abs(x1 - x2) + std::abs(y1 - y2);
}

// Mapper class
class Mapper {
public:
    // ctor
    Mapper(int r, int c) : rows(r), cols(c), core_array(r, std::vector<int>(c, -1)) {}

    // print mappings
    void print_mappings() const;

    // print core array
    void print_core_array() const;

private:
    // core array, -1 means free, otherwise node index
    std::vector<std::vector<int>> core_array;
    int rows, cols;

    // bidirectional mapping between node and core
    std::unordered_map<int, std::pair<int, int>> node_to_core;
    std::unordered_map<std::pair<int, int>, int, pair_hash> core_to_node;

    // index validity check
    bool is_valid(int x, int y) const { return x >= 0 && x < rows && y >= 0 && y < cols; }

    // core availability check
    bool is_free(int x, int y) const { return is_valid(x, y) && core_array[x][y] == -1; }

    // node registration
    bool map_node_to_core(int node, int x, int y);

    // node unregistration
    bool unmap_node(int node);

    // find best contiguous block
    bool find_best_contiguous_block(int required_size, const std::vector<std::pair<int, int>>& ref_points, std::pair<int, int>& best_start, int& min_distance) const;

    // map a group of nodes
    bool map_group(const Group& group);


};
#endif
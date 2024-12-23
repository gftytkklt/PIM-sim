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
#include <set>
#include "util.h"

// nodes group
using Group = std::set<size_t>;

// Mapper class
class Mapper {
public:
    // ctor
    Mapper(int r, int c) : rows(r), cols(c), core_array(r, std::vector<size_t>(c, -1)) {}
    Mapper(std::pair<int, int> size) : Mapper(size.first, size.second) {}
    Mapper() = default;
    // print mappings
    void print_mappings() const;

    // print core array
    void print_core_array() const;

    // map a group of nodes
    bool map_group(const Group& group);

    // HNode getter
    std::pair<int, int> get_core(size_t node) const;

private:
    // core array, -1 means free, otherwise node index
    std::vector<std::vector<size_t>> core_array;
    int rows, cols;

    // bidirectional mapping between node and core
    std::unordered_map<size_t, std::pair<int, int>> node_to_core;
    std::unordered_map<std::pair<int, int>, size_t, pair_hash> core_to_node;

    // index validity check
    bool is_valid(int x, int y) const { return x >= 0 && x < rows && y >= 0 && y < cols; }

    // core availability check
    bool is_free(int x, int y) const { return is_valid(x, y) && core_array[x][y] == -1; }

    // node registration
    bool map_node_to_core(size_t node, int x, int y);

    // node unregistration
    bool unmap_node(size_t node);

    // find best contiguous block
    bool find_best_contiguous_block(int required_size, const std::vector<std::pair<int, int>>& ref_points, std::pair<int, int>& best_start, int& min_distance) const;

};
#endif
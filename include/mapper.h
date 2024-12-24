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
#include <queue>
#include <sstream>
#include "util.h"

// nodes group
using Group = std::set<size_t>;

// direction enum
enum Direction {
    LEFT = 0,
    UP,
    RIGHT,
    DOWN,
    DIRECTION_COUNT  // size of direction enum
};

// direction array
constexpr std::array<std::array<int, 2>, DIRECTION_COUNT> DIRS = {{
    {0, -1},   // LEFT
    {-1, 0},    // UP
    {0, 1},    // RIGHT
    {1, 0}    // DOWN
}};

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
    bool map_group(const Group& group, const Group& dep_set);

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

    // find tl corner of current available block
    std::pair<int, int> find_tl_corner() const;

    // find best contiguous block in one direction
    std::pair<bool, std::vector<std::pair<int,int>>> find_best_contiguous_block(int required_size, const std::vector<std::pair<int, int>>& ref_points) const;

    // find best contiguous block in dfs manner
    std::pair<bool, std::vector<std::pair<int,int>>> bfs_heuristic_mapping(int required_size, const std::vector<std::pair<int, int>>& ref_points) const;
};
#endif
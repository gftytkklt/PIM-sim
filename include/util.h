#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <tuple>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <queue>

std::vector<std::vector<std::pair<int, int>>> uniformsplit(int M, int N, int K);
int UniqueElements(const std::pair<int, int>& pair1, const std::pair<int, int>& pair2);
// manhattan distance
int manhattan_distance(int x1, int y1, int x2, int y2);
int manhattan_distance(std::pair<int, int> p1, std::pair<int, int> p2);
// xy init
std::vector<std::pair<int, int>> XYinit(std::pair<int, int> src, std::pair<int, int> dst);
// median compute
int compute_median(std::vector<int>& vec);
// fast median compute
int fast_compute_median(std::vector<int>& vec);
// fast median compute for pair
std::pair<int, int> fast_compute_median(std::vector<std::pair<int, int>>& vec);
// get median point, not coordinates
std::pair<int, int> get_median_point(std::vector<std::pair<int, int>>& vec);
// Combination C(n, k)
int Combination(int n, int k);
// 2D shortest path num
long long shortest_path_num(std::pair<int, int> src, std::pair<int, int> dst);
// M dup to N dup corresponding, dup_id is cur dup in [1, M]
std::vector<int> dup_to_dup(int M, int N, int dup_id);
std::vector<size_t> neighbor_ranking_sort(const std::unordered_map<size_t, std::unordered_map<size_t, int>>& conn_intensity_map);
std::vector<size_t> k_group_sort(const std::unordered_map<size_t, std::unordered_map<size_t, int>>& conn_intensity_map, size_t k);

// move Path to here for common use
struct Path{
    size_t id;
    std::pair<int,int> src, dst;
    std::vector<std::pair<int,int>> via;
    int datavolume;
};

// for undirected edge descriptor
struct UnorderedPair {
    size_t first;
    size_t second;

    UnorderedPair(size_t a, size_t b) : first(std::min(a, b)), second(std::max(a, b)) {}

    bool operator<(const UnorderedPair& other) const {
        return std::tie(first, second) < std::tie(other.first, other.second);
    }
};

// Hash function for UnorderedPair
struct UnorderedPairHash {
    std::size_t operator()(const UnorderedPair& p) const {
        return std::hash<size_t>()(p.first) ^ std::hash<size_t>()(p.second);
    }
};

// hash function for std::pair
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ hash2;
    }
};

// comparator for pair.first
struct pair_first_comparator {
    template <typename T1, typename T2>
    bool operator()(const std::pair<T1, T2>& p1, const std::pair<T1, T2>& p2) const {
        return p1.first < p2.first;
    }
};

// comparator for pair.second
struct pair_second_comparator {
    template <typename T1, typename T2>
    bool operator()(const std::pair<T1, T2>& p1, const std::pair<T1, T2>& p2) const {
        return p1.second < p2.second;
    }
};

// for paper experiments
enum class OptType {
    MNSIM,
    HITM,
    SPATEM,
    PIMAPPING,
    TILE2_0,
};

// convert OptType to string
inline std::string opt_type_to_string(OptType opt_type) {
    switch (opt_type) {
        case OptType::MNSIM: return "MNSIM";
        case OptType::HITM: return "HITM";
        case OptType::SPATEM: return "SPATEM";
        case OptType::PIMAPPING: return "PIMAPPING";
        case OptType::TILE2_0: return "TILE2_0";
        default: return "UNKNOWN";
    }
}

int compute_node_num (std::pair<int, int> xbar_size, std::pair<int, int> window_shape, std::pair<int, int> channel_shape);

int get_compute_num(std::pair<int, int> window_shape, std::pair<int, int> channel_shape, std::pair<int, int> fmap_size);

#endif
#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <utility>
#include <string>
#include <tuple>

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
int shortest_path_num(std::pair<int, int> src, std::pair<int, int> dst);

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

#endif
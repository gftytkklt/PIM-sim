#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <utility>
#include <string>

std::vector<std::vector<std::pair<int, int>>> uniformsplit(int M, int N, int K);
int UniqueElements(const std::pair<int, int>& pair1, const std::pair<int, int>& pair2);

// hash function for std::pair
struct pair_hash {
    template <class T1, class T2>
    std::size_t operator()(const std::pair<T1, T2>& p) const {
        auto hash1 = std::hash<T1>{}(p.first);
        auto hash2 = std::hash<T2>{}(p.second);
        return hash1 ^ hash2;
    }
};

#endif
#include "util.h"
#include <algorithm>
#include <iostream>
#include <vector>
#include <utility> // for std::pair
#include <random>
#include <chrono>
#include <cstdlib>

std::pair<int, int> getOverlap(const std::pair<int, int>& range1, const std::pair<int, int>& range2) {
    // 计算重叠区间的起始和终止点
    int start = std::max(range1.first, range2.first);
    int end = std::min(range1.second, range2.second);
    // std::cout << "overlap: " << start << ", " << end << std::endl;
    // 检查区间是否真的有重叠
    if (start <= end) {
        return {start, end};
    } else {
        // 如果没有重叠，返回一个无效的区间
        // 您可以根据需要调整这里的返回值
        return {0, 0};
    }
}

int twoDdist(std::pair<int, int> x, std::pair<int, int> y) {
    return std::abs(x.first-y.first) + std::abs(x.second-y.second);
}

std::pair<int, int> randomPointWithManhattanDistance(int m, int n, int startX, int startY, int distance) {
    std::vector<std::pair<int, int>> candidates;

    // 检查网格内的每个点
    for (int x = 0; x < m; ++x) {
        for (int y = 0; y < n; ++y) {
            // 计算曼哈顿距离
            int manhattanDist = std::abs(x - startX) + std::abs(y - startY);
            if (manhattanDist == distance) {
                candidates.emplace_back(x, y);
            }
        }
    }

    // 如果没有找到候选点
    if (candidates.empty()) {
        return {-1, -1}; // 返回一个表示无效的点
    }

    // 随机选择一个候选点
    unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
    std::default_random_engine engine(seed);
    std::uniform_int_distribution<int> dist(0, candidates.size() - 1);
    int index = dist(engine);

    return candidates[index];
}

std::shared_ptr<std::vector<std::pair<int, int>>> initXYRouting(std::pair<int, int> src, std::pair<int, int> dst){
    std::vector<std::pair<int, int>> route;
    if(src != dst){
        auto [srcx, srcy] = src;
        auto [dstx, dsty] = dst;
        int deltax = srcx > dstx ? -1 : 1;
        int deltay = srcy > dsty ? -1 : 1;
        route.emplace_back(src);
        while(srcx != dstx){
            srcx += deltax;
            route.emplace_back(srcx, srcy);
        }
        while(srcy != dsty){
            srcy += deltay;
            route.emplace_back(dstx, srcy);
        }
    }
    return std::make_shared<std::vector<std::pair<int, int>>>(route);
}
#ifndef UTIL_H
#define UTIL_H
#include <utility>
#include <cstdlib>
#include <cmath>
#include <vector>
#include <memory>
// get overlap between blk in layer & blk layer in layer+1
std::pair<int, int> getOverlap(const std::pair<int, int>& range1, const std::pair<int, int>& range2);
// Manhattan dist between two blk
int twoDdist(std::pair<int, int> x, std::pair<int, int> y);
// random point with Manhattan distance
std::pair<int, int> randomPointWithManhattanDistance(int m, int n, int startX, int startY, int distance);
// init path by default XY routing
std::shared_ptr<std::vector<std::pair<int, int>>> initXYRouting(std::pair<int, int> src, std::pair<int, int> dst);
// shortest path number between two blk
int shortestPathNum(std::pair<int, int> src, std::pair<int, int> dst);
// shortest path number between two blk via another blk
int shortestPathVia(std::pair<int, int> src, std::pair<int, int> dst, std::pair<int, int> via) {
    auto [srcx, srcy] = src;
    auto [dstx, dsty] = dst;
    auto [viax, viay] = via;
    auto [minx, maxx] = std::minmax(srcx, dstx);
    auto [miny, maxy] = std::minmax(srcy, dsty);
    if ((minx <= viax && viax <= maxx) && (miny <= viay && viay <= maxy)) {
        return shortestPathNum(src, via) * shortestPathNum(via, dst);
    }
    return 0;
}
#endif
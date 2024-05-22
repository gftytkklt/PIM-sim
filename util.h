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
std::pair<int, int> randomPointWithManhattanDistance(int m, int n, int startX, int startY, int distance);
std::shared_ptr<std::vector<std::pair<int, int>>> initXYRouting(std::pair<int, int> src, std::pair<int, int> dst);
#endif
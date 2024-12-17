#ifndef UTIL_H
#define UTIL_H

#include <iostream>
#include <vector>
#include <utility>
#include <string>

std::vector<std::vector<std::pair<int, int>>> uniformsplit(int M, int N, int K);
int UniqueElements(const std::pair<int, int>& pair1, const std::pair<int, int>& pair2);

#endif
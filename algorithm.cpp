#include "algorithm.h"

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : layer(layer), in_channel(in_channel), out_channel(out_channel), fmap_size(0), location{} {}

void Baseblk::set_location(std::pair<int, int> coord) {
    this->location = coord;
}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={})
    : kernels{kernels}, maxbaseblk{maxbaseblk} {}
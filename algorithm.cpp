#include "algorithm.h"

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : layer(layer), in_channel(in_channel), out_channel(out_channel), fmap_size(0), location{} {}

void Baseblk::set_location(std::pair<int, int> coord) {
    this->location = coord;
}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={})
    : kernels{kernels}, maxbaseblk{maxbaseblk} {
        create_SIMDblk();
}
// steps: 
// 1. decomp w*h to A*3*3
// 2. decomp in channel to 128*B
// 3. decomp out channnel to 256*C
// 4. baseblk = {A*B*C} elems' set for each layer
void DFG::create_baseblk(){
    for(auto &kernel : this->kernels){
        // impl step 1
        int iter = (kernel.w + 3 - 1) / 3 * (kernel.h + 3 - 1) / 3;
        
    }
}
// rules: merge baseblk with same layer and in channel
void DFG::create_SIMDblk(){

}

std::pair<int, int> DFG::get_blksize() const{
    return this->maxbaseblk;
}
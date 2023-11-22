#include "algorithm.h"
#include <iostream>
#include <map>

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : layer(layer), in_channel(in_channel), out_channel(out_channel), fmap_size(0), location{} {}

void Baseblk::set_location(std::pair<int, int> coord) {
    this->location = coord;
}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={})
    : kernels{kernels}, maxbaseblk{maxbaseblk} {
    create_baseblk();
    create_SIMDblk();
}
// steps: 
// 1. decomp w*h to A*3*3
// 2. decomp in channel to 128*B
// 3. decomp out channnel to 256*C
// 4. baseblk = {A*B*C} elems' set for each layer
void DFG::create_baseblk(){
    const int maxInChannels = this->maxbaseblk.first/9;
    const int maxOutChannels = this->maxbaseblk.second;
    for(auto &kernel : this->kernels){
        // impl step 1
        int layer_id = kernel.layer;
        int iter = (kernel.w + 3 - 1) / 3 * (kernel.h + 3 - 1) / 3;
        int numInSplits = (kernel.in_channel + maxInChannels - 1) / maxInChannels;
        int numOutSplits = (kernel.out_channel + maxOutChannels - 1) / maxOutChannels;
        for (int i=0; i<iter; i++){
            for(int j=0; j<numInSplits; j++){
                for(int k=0; k<numOutSplits; k++){
                    std::pair<int, int> in_id = std::make_pair(j*maxInChannels+1, std::min((j+1)*maxInChannels, kernel.in_channel));
                    std::pair<int, int> out_id = std::make_pair(k*maxOutChannels+1, std::min((k+1)*maxOutChannels, kernel.out_channel));
                    this->baseblks.emplace_back(layer_id, in_id, out_id);
                }
            }
        }
    }
}
// rules: merge baseblk with same layer and in channel
// must exec after create_baseblk()
void DFG::create_SIMDblk(){
    // 遍历baseblks，按layer和in_channel分组
    std::map<std::pair<int, std::pair<int, int>>, std::vector<Baseblk>> groupedBlks;

    for (const auto& blk : baseblks) {
        groupedBlks[std::make_pair(blk.getLayer(), blk.getInChannel())].push_back(blk);
    }

    // 创建SIMDblk对象并添加到SIMDblks
    for (const auto& group : groupedBlks) {
        SIMDblks.emplace_back(group.second);
    }
}

std::pair<int, int> DFG::get_blksize() const{
    return this->maxbaseblk;
}

void DFG::print_baseblks() const{
    for (const auto& blk : baseblks) {
        std::cout << "Layer: " << blk.getLayer()
            << ", In Channel: " << blk.getInChannel().first << " - " << blk.getInChannel().second
            << ", Out Channel: " << blk.getOutChannel().first << " - " << blk.getOutChannel().second
            << std::endl;
    }
}

void DFG::print_SIMDblks() const {
    int i = 0;
    for (const auto& simdBlk : SIMDblks) {
        std::cout << "SIMD blk: " << ++i << std::endl;
        for (const auto& baseBlk : simdBlk.getBaseblks()) {
            std::cout << "Layer: " << baseBlk.getLayer()
                      << ", In Channel: " << baseBlk.getInChannel().first << " - " << baseBlk.getInChannel().second
                      << ", Out Channel: " << baseBlk.getOutChannel().first << " - " << baseBlk.getOutChannel().second
                      << std::endl;
        }
    }
}
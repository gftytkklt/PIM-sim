#include "algorithm.h"
#include <iostream>
#include <map>

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : layer{layer}, in_channel{in_channel}, out_channel{out_channel}, fmap_size{0}, location{} {}

void Baseblk::set_location(std::pair<int, int> coord) {
    this->location = coord;
}

SIMDblk::SIMDblk(const std::vector<Baseblk>& blks, int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : baseblks{blks}, layer{layer}, in_channel{in_channel}, out_channel{out_channel}{}

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
void DFG::create_SIMDblk() {
    // store extra info of SIMD blk
    struct SIMDInfo {
        std::vector<Baseblk> baseblks;
        int layer;
        std::pair<int, int> inChannel;
        std::pair<int, int> outChannel;
    };

    std::map<std::pair<int, std::pair<int, int>>, SIMDInfo> groupedBlks;

    for (const auto& blk : baseblks) {
        auto key = std::make_pair(blk.getLayer(), blk.getInChannel());
        auto& info = groupedBlks[key];
        info.baseblks.push_back(blk);

        // init layer & in channel
        info.layer = blk.getLayer();
        info.inChannel = blk.getInChannel();

        // init & update out channel
        if (info.outChannel.first == 0 && info.outChannel.second == 0) {
            info.outChannel = blk.getOutChannel();
        } else {
            info.outChannel.first = std::min(info.outChannel.first, blk.getOutChannel().first);
            info.outChannel.second = std::max(info.outChannel.second, blk.getOutChannel().second);
        }
    }

    // 步骤3: 使用收集的信息构造 SIMDblk 对象
    for (const auto& group : groupedBlks) {
        const auto& info = group.second;
        SIMDblk simdBlk(info.baseblks, info.layer, info.inChannel, info.outChannel);
        this->SIMDblks.emplace_back(simdBlk);
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
        std::cout << "Layer: " << simdBlk.getLayer()
                  << ", In Channel: " << simdBlk.getInChannel().first << " - " << simdBlk.getInChannel().second
                  << ", Out Channel: " << simdBlk.getOutChannel().first << " - " << simdBlk.getOutChannel().second
                  << std::endl;
                  
    }
}
#include "algorithm.h"
#include <iostream>
#include <map>

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : layer{layer}, in_channel{in_channel}, out_channel{out_channel}, fmap_size{0}, location{} {}

void Baseblk::set_location(std::pair<int, int> coord) {
    this->location = coord;
}

SIMDblk::SIMDblk(const std::vector<Baseblk>& blks, int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : baseblks{blks}, layer{layer}, in_channel{in_channel}, out_channel{out_channel}, parents{}, children{}, fanout{0}{}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={})
    : kernels{kernels}, maxbaseblk{maxbaseblk} {
    create_baseblk();
    create_SIMDblk();
    connect_SIMDblk();
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
    // use layer(int) & out_channel(std::pair<int, int>) for labelling
    std::map<std::pair<int, std::pair<int, int>>, SIMDInfo> groupedBlks;

    for (const auto& blk : baseblks) {
        auto key = std::make_pair(blk.getLayer(), blk.getOutChannel());
        auto& info = groupedBlks[key];
        info.baseblks.push_back(blk);

        // init layer & in channel
        info.layer = blk.getLayer();
        info.outChannel = blk.getOutChannel();

        // init & update out channel
        if (info.inChannel.first == 0 && info.inChannel.second == 0) {
            info.inChannel = blk.getInChannel();
        } else {
            info.inChannel.first = std::min(info.inChannel.first, blk.getInChannel().first);
            info.inChannel.second = std::max(info.inChannel.second, blk.getInChannel().second);
        }
    }

    // build simd blk
    for (const auto& group : groupedBlks) {
        const auto& info = group.second;
        SIMDblk simdBlk(info.baseblks, info.layer, info.inChannel, info.outChannel);
        this->SIMDblks.emplace_back(simdBlk);
    }
}

std::pair<int, int> getOverlap(const std::pair<int, int>& range1, const std::pair<int, int>& range2) {
    // 计算重叠区间的起始和终止点
    int start = std::max(range1.first, range2.first);
    int end = std::min(range1.second, range2.second);

    // 检查区间是否真的有重叠
    if (start <= end) {
        return {start, end};
    } else {
        // 如果没有重叠，返回一个无效的区间
        // 您可以根据需要调整这里的返回值
        return {0, 0};
    }
}
// based on SIMDblk is sorted by ascending order of SIMD.layer
void DFG::connect_SIMDblk() {
    for (auto it = this->SIMDblks.begin(); it != this->SIMDblks.end(); ++it) {
        int cur_layer = it->getLayer();
        auto parent_channel = it->getOutChannel();
        int child_layer = cur_layer + 1;
        for (auto innerIt = std::next(it); innerIt != this->SIMDblks.end(); ++innerIt){
            int node_layer = innerIt->getLayer();
            // do nothing for same layer blk
            if(node_layer < child_layer){continue;}
            // end searching for subsequent layers
            else if(node_layer > child_layer){break;}
            // child layer: may have connection
            auto child_channel = innerIt->getInChannel();
            // check if parent out overlapped with child in
            auto overlap = getOverlap(parent_channel, child_channel);
            // overlap don't have zero if overlap exists
            if(overlap.first != 0){
                // addchild
                it->addChild(&*innerIt);
                innerIt->addParent(&*it);
                // didn't consider non-128-channel-aligned kernel(fix it in the future)
                it->incrFanout((overlap.second+1-overlap.first)/(maxbaseblk.first/9));
            }
        }
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
                  << ", Fanout: " << simdBlk.getFanout()
                  << std::endl;
        const auto parent = simdBlk.getParent();
        int j = 0;
        for(const auto p : parent) {
            if(p){
                std::cout << "parent" << ++j << ", Layer: " << p->getLayer()
                << ", In Channel: " << p->getInChannel().first << " - " << p->getInChannel().second
                << ", Out Channel: " << p->getOutChannel().first << " - " << p->getOutChannel().second
                << std::endl;
            }
        }
        const auto child = simdBlk.getChild();
        int k = 0;
        for(const auto p : child) {
            if(p){
                std::cout << "child" << ++k << ", Layer: " << p->getLayer()
                << ", In Channel: " << p->getInChannel().first << " - " << p->getInChannel().second
                << ", Out Channel: " << p->getOutChannel().first << " - " << p->getOutChannel().second
                << std::endl;
            }
        }
    }
}
#include "algorithm.h"
#include "util.h"
#include <iostream>
#include <map>

Baseblk::Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel, std::pair<int, int> fmap_size)
    : layer{layer}, in_channel{in_channel}, out_channel{out_channel}, fmap_size{fmap_size}, location{} {}

void Baseblk::setLocation(std::pair<int, int> coord) {
    this->location = coord;
}

void Baseblk::printBaseblkInfo() const {
    std::cout << "Layer: " << this->getLayer()
                  << ", In Channel: " << this->getInChannel().first << " - " << this->getInChannel().second
                  << ", Out Channel: " << this->getOutChannel().first << " - " << this->getOutChannel().second
                  << ", Input fmap size: (" << this->getFmapSize().first << " , " << this->getFmapSize().second
                  << "), child size: " << this->successors.size() << std::endl;
}

/**
 * @brief In principle, find will always return valid key,
 * this is guaranteed by addSuccessor() func,
 * however, if code structure is changed,
 * this condition may not be guaranteed in the future
 */
void Baseblk::printSuccessorInfo() const {
    auto i = 0;
    for(const auto& successor : successors){
        auto it = dataflow.find(successor);
        if (it != dataflow.end()) {
            std::cout << "Successor " << i++ << ": " << it->second << ", ";
            successor->printBaseblkInfo();
        }
    }
}

SIMDblk::SIMDblk(const std::vector<Baseblk*> blks, int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel)
    : baseblks{blks}, layer{layer}, in_channel{in_channel}, out_channel{out_channel},
    parents{}, children{}, fanout{0}, fanout_loc{std::make_pair(-1, -1)}, ismapped{false}{}

/**
 * @brief create connection relationship & data amount
 * 
 */
void SIMDblk::connectBaseblk() {
    for(auto it = baseblks.begin(); it < baseblks.end()-1 ; ++it){
        (*it)->addSuccessor(*(it+1), true);
    }
    auto outblk = baseblks.end()-1;
    // std::cout << "test outblk info:\n";
    // (*outblk)->printBaseblkInfo();
    // std::cout << "test childblk info:\n";
    for(auto child : children){
        for(auto childblk : child->getBaseblks()){
            if(getOverlap(childblk->getInChannel(), (*outblk)->getOutChannel()) != std::make_pair(0, 0)){
                // childblk->printBaseblkInfo();
                (*outblk)->addSuccessor(childblk, false);
            }
        }
    }
    // std::cout << "test final outblk info:\n";
    // (*outblk)->printBaseblkInfo();
}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={})
    : kernels{kernels}, maxbaseblk{maxbaseblk} {
    createBaseblk();
    createSIMDblk();
    connectSIMDblk();
    connectBaseblk();
}

DFG::DFG(std::vector<Convkernel> kernels={}, std::pair<int, int> maxbaseblk={}, std::pair<int, int> input_size = {})
    : kernels{kernels}, maxbaseblk{maxbaseblk}, input_size{input_size} {
    createBaseblk();
    createSIMDblk();
    connectSIMDblk();
    connectBaseblk();
}
// steps: 
// 1. decomp w*h to A*3*3
// 2. decomp in channel to 128*B
// 3. decomp out channnel to 256*C
// 4. baseblk = {A*B*C} elems' set for each layer
// new feature: build (input) fmap_size for each baseblk
void DFG::createBaseblk(){
    const int maxInChannels = this->maxbaseblk.first/9;
    const int maxOutChannels = this->maxbaseblk.second;
    auto fmap_size = this->input_size;
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
                    this->baseblks.emplace_back(layer_id, in_id, out_id, fmap_size);
                }
            }
        }
        fmap_size.first = fmap_size.first / kernel.stride / kernel.pooling_factor;
        fmap_size.second = fmap_size.second / kernel.stride / kernel.pooling_factor;
    }
}
// rules: merge baseblk with same layer and in channel
// must exec after create_baseblk()
void DFG::createSIMDblk() {
    // store extra info of SIMD blk
    struct SIMDInfo {
        std::vector<Baseblk*> baseblks;
        int layer;
        std::pair<int, int> inChannel;
        std::pair<int, int> outChannel;
    };
    // use layer(int) & out_channel(std::pair<int, int>) for labelling
    std::map<std::pair<int, std::pair<int, int>>, SIMDInfo> groupedBlks;

    for (auto& blk : baseblks) {
        auto key = std::make_pair(blk.getLayer(), blk.getOutChannel());
        auto& info = groupedBlks[key];
        info.baseblks.push_back(&blk);

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


// based on SIMDblk is sorted by ascending order of SIMD.layer
void DFG::connectSIMDblk() {
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

// 1. get each baseblk in SIMDblk
// 2. get child SIMDblk of parent SIMDblk
// 3. check corresponding relationship
void DFG::connectBaseblk(){
    // std::cout << "test\n";
    // this->printBaseblks();
    // std::cout << "test end\n";
    for (auto& simdBlk : SIMDblks) {
        simdBlk.connectBaseblk();
    }
}

std::pair<int, int> DFG::getBlksize() const{
    return this->maxbaseblk;
}

void DFG::printBaseblks() const{
    for (const auto& blk : baseblks) {
        std::cout << "Node: \n";
        blk.printBaseblkInfo();
        std::cout << "successor: \n";
        blk.printSuccessorInfo();
    }
}

void DFG::printSIMDblks() const {
    int i = 0;
    for (const auto& simdBlk : SIMDblks) {
        std::cout << "SIMD blk: " << ++i << std::endl;
        std::cout << "Layer: " << simdBlk.getLayer()
                  << ", In Channel: " << simdBlk.getInChannel().first << " - " << simdBlk.getInChannel().second
                  << ", Out Channel: " << simdBlk.getOutChannel().first << " - " << simdBlk.getOutChannel().second
                  << ", Fanout: " << simdBlk.getFanout()
                  << std::endl;
        // for (const auto& it : simdBlk.getBaseblks()){
        //     std::cout << "info in simdblks\n";
        //     it->printBaseblkInfo();
        // }
        
        const auto parent = simdBlk.getParent();
        int j = 0;
        for(const auto& p : parent) {
            if(p){
                std::cout << "parent" << ++j << ", Layer: " << p->getLayer()
                << ", In Channel: " << p->getInChannel().first << " - " << p->getInChannel().second
                << ", Out Channel: " << p->getOutChannel().first << " - " << p->getOutChannel().second
                << std::endl;
            }
        }
        const auto child = simdBlk.getChild();
        int k = 0;
        for(const auto& p : child) {
            if(p){
                std::cout << "child" << ++k << ", Layer: " << p->getLayer()
                << ", In Channel: " << p->getInChannel().first << " - " << p->getInChannel().second
                << ", Out Channel: " << p->getOutChannel().first << " - " << p->getOutChannel().second
                << std::endl;
            }
        }
    }
}
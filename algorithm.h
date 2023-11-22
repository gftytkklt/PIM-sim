#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <memory>
#include <vector>

struct Convkernel {
    // for data dependency
    int layer;
    // for calculate mapping
    int w, h; // window shape
    int stride; // stride
    int in_channel, out_channel; // kernel channel num
};

class Baseblk {
    private:
        int layer; // conv layer
        std::pair<int, int> in_channel, out_channel; // conv channel
        int fmap_size; // fmap size
        std::pair<int, int> location; // location on PIM-tile array
    public:
        Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel);
        void set_location(std::pair<int, int> coord);
};

class SIMDblk {
    private:

};

class DFG {
    private:
        std::vector<Convkernel> kernels;
        std::vector<Baseblk> blks;
        std::vector<SIMDblk> SIMDblks;
        std::pair<int, int> maxbaseblk; // <WL, BL> PIM array shape
    public:
        DFG(){}
        DFG(std::vector<Convkernel> kernels, std::pair<int, int> maxbaseblk);
};
#endif
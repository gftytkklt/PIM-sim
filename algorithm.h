#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <memory>

struct Convkernel{
    // for data dependency
    int layer;
    // for calculate mapping
    int w, h; // window shape
    int stride; // stride
    int in_channel, out_channel; // kernel channel num
};

class Algorithm{
    private:

};

class Baseblk{
    private:
        int layer; // conv layer
        std::pair<int, int> in_channel, out_channel; // conv channel
        int fmap_size; // fmap size
        std::pair<int, int> location; // location on PIM-tile array
};

#endif
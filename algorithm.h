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
        int getLayer() const { return layer; }
        std::pair<int, int> getInChannel() const { return in_channel; }
        std::pair<int, int> getOutChannel() const { return out_channel; }
};

class SIMDblk {
    private:
        int layer;
        std::pair<int, int> in_channel, out_channel;
        std::vector<Baseblk> baseblks;
        std::vector<SIMDblk*> parents, children;
        int fanout;

    public:
        SIMDblk(const std::vector<Baseblk>& blks, int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel);
        const std::vector<Baseblk>& getBaseblks() const {return baseblks;}
        int getLayer() const {return layer;}
        std::pair<int, int> getInChannel() const {return in_channel;}
        std::pair<int, int> getOutChannel() const {return out_channel;}
        void addParent(SIMDblk* parent){parents.push_back(parent);}
        void addChild(SIMDblk* child) {children.push_back(child);}
        std::vector<SIMDblk*> getParent() const {return parents;}
        std::vector<SIMDblk*> getChild() const {return children;}
        void incrFanout(int size) {fanout += size;};
        int getFanout() const {return fanout;}
};

class DFG {
    private:
        std::vector<Convkernel> kernels;
        std::vector<Baseblk> baseblks;
        std::vector<SIMDblk> SIMDblks;
        std::pair<int, int> maxbaseblk; // <WL, BL> PIM array shape
    public:
        DFG(std::vector<Convkernel> kernels, std::pair<int, int> maxbaseblk);
        // connection impl, split each step for generalize
        void create_baseblk(); // init blks
        void create_SIMDblk(); // init SIMDblks
        void connect_SIMDblk(); // build dependence map
        std::pair<int, int> get_blksize() const;
        void print_baseblks() const;
        void print_SIMDblks() const;
};
#endif
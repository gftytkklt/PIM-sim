#ifndef ALGORITHM_H
#define ALGORITHM_H

#include <memory>
#include <vector>
#include <iostream>
#include <unordered_map>

struct Convkernel {
    // for data dependency
    int layer;
    // for calculate mapping
    int w, h; // window shape
    int stride; // stride
    int in_channel, out_channel; // kernel channel num
    int pooling_factor = 2; // downsampling factor, default = 2(for debugging)
};

struct path {
    std::shared_ptr<std::vector<std::pair<int, int>>> route; // hardware connection
    int data_size; // communication data amount
    path(){}; // for compilation
    path(int data_size) : data_size{data_size}{};
    path(std::shared_ptr<std::vector<std::pair<int, int>>> route, int data_size) : route{route}, data_size{data_size}{};
    friend std::ostream& operator<<(std::ostream& os, const path& p) {
        os << "Data size: " << p.data_size << ", Route: ";
        for (const auto& coord : *p.route) {
            os << "(" << coord.first << ", " << coord.second << ") ";
        }
        return os;
    }
};

class Baseblk {
    private:
        int layer; // conv layer
        std::pair<int, int> in_channel, out_channel; // conv channel
        std::pair<int, int> fmap_size; // input fmap size
        std::pair<int, int> location; // location on PIM-tile array
        std::vector<Baseblk*> successors; // indicate data dependency
        // std::unordered_map<Baseblk*, int> dataflow; // data transformation amount
        // hardware mapping info
        std::unordered_map<Baseblk*, path> dataflow; // path[successors].xxx = ...
    public:
        Baseblk(int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel, std::pair<int, int> fmap_size);
        void setLocation(std::pair<int, int> coord);
        int getLayer() const { return layer; }
        /**
         * @brief add child & data_size
         * 
         * @param blk child object
         * @param intralayer use parent out_channel
         */
        
        auto getInChannel() const { return in_channel; }
        auto getOutChannel() const { return out_channel; }
        auto getFmapSize() const {return fmap_size;}
        auto getLocation() const {return location;}
        auto getSuccessors() const {return successors;}
        void printBaseblkInfo() const;
        void printSuccessorInfo() const;
        void addSuccessor(Baseblk* blk, bool intralayer) { 
            successors.push_back(blk);
            auto [in1, in2] = intralayer ? getOutChannel() : blk->getInChannel();
            auto [w, h] = blk->getFmapSize();
            auto data_size = (in2 - in1 + 1) * w * h;
            dataflow[blk] = path(data_size);
            // std::cout << "size: " << successors.size() << "\n";
        }

        void addRoute(Baseblk* blk, std::shared_ptr<std::vector<std::pair<int, int>>> route){
            auto it = dataflow.find(blk);
            if(it != dataflow.end()){
                dataflow[blk].route = route;
            }
        }

        auto getDatasize(Baseblk* blk) const {
            auto it = dataflow.find(blk);
            if(it != dataflow.end()){
                return it->second.data_size;
            }
            return -1;
        }
        
};

class SIMDblk {
    private:
        int layer;
        std::pair<int, int> in_channel, out_channel;
        std::vector<Baseblk*> baseblks;
        std::vector<SIMDblk*> parents, children;
        int fanout;
        std::pair<int, int> fanout_loc;
        bool ismapped;
    public:
        SIMDblk(const std::vector<Baseblk*> blks, int layer, std::pair<int, int> in_channel, std::pair<int, int> out_channel);
        auto& getBaseblks() { return baseblks; }
        const auto& getBaseblks() const {return baseblks;}
        int getLayer() const {return layer;}
        std::pair<int, int> getInChannel() const {return in_channel;}
        std::pair<int, int> getOutChannel() const {return out_channel;}
        void addParent(SIMDblk* parent){parents.push_back(parent);}
        void addChild(SIMDblk* child) {children.push_back(child);}
        std::vector<SIMDblk*> getParent() const {return parents;}
        std::vector<SIMDblk*> getChild() const {return children;}
        void incrFanout(int size) {fanout += size;}
        int getFanout() const {return fanout;}
        std::pair<int, int> getFanoutloc() const {return fanout_loc;}
        void setFanoutloc(std::pair<int, int> loc) {fanout_loc = loc;}
        void connectBaseblk();
};

class DFG {
    private:
        std::vector<Convkernel> kernels;
        std::vector<Baseblk> baseblks;
        std::vector<SIMDblk> SIMDblks;
        std::pair<int, int> maxbaseblk; // <WL, BL> PIM array shape
        std::pair<int, int> input_size; // w*h
    public:
        DFG(std::vector<Convkernel> kernels, std::pair<int, int> maxbaseblk);
        DFG(std::vector<Convkernel> kernels, std::pair<int, int> maxbaseblk, std::pair<int, int> input_size);
        // connection impl, split each step for generalize
        void createBaseblk(); // init blks
        void createSIMDblk(); // init SIMDblks
        void connectSIMDblk(); // build dependence map
        void connectBaseblk(); // build baseblk level connection abstration
        // void addHWConnection(); // HW connection impl entry
        std::pair<int, int> getBlksize() const;
        std::vector<Baseblk>& getBaseblk() {return this->baseblks;}
        std::vector<SIMDblk> getSIMDblk() const {return this->SIMDblks;}
        void printBaseblks() const;
        void printSIMDblks() const;
};
#endif
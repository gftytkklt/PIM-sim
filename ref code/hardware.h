#ifndef HARDWARE_H
#define HARDWARE_H

#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <list>
#include <functional>
#include "algorithm.h"

// Direction enum
enum class Direction{
    TopLeft, Top, TopRight,
    Left, Self, Right,
    BottomLeft, Bottom, BottomRight
};

struct Connection{
    std::map<Direction, int> port;// direction & used num
    int num;// number of port
};

extern std::string toString(Direction dir);

enum class connect_type{SIMD, SRAM};
class PIM_tile{
    friend class PIM_chip;
    private:
        int memsize;// local SRAM size
        int blk_num;// basic blk num, now blk size is 1152*256, num is 4
        int available_blk;// free blk can be allocated
        int available_mem;// free mem can be allocated
        std::pair<int, int> blk_size; // WL * BL
        Connection SIMD_connect, SRAM_connect;
        std::vector<Baseblk*> mapped_blks;
    public:
        explicit PIM_tile(int memsize, int blk_num, std::pair<int, int> blk_size); // ctor
        int getMemsize() const; // mem capacity of tile
        int getBlknum() const; // basic blk num of tile
        int getFreeblk() const; // current free blk num of tile
        int getFreemem() const; // current free mem of tile
        int getPortnum(connect_type type) const;
        std::pair<int, int> getBlksize() const;
        bool allocateBlk(int num); // alloc free blk
        void allocateMem(int size); // alloc free mem
        void freeBlk(int num); // free blk
        void freeMem(int size); // free mem
        void initConnection(int i, int j, int w, int h); // init tile connection
        void incConnection(Direction direct, connect_type type); // inc type.used
        void delConnection(Direction direct, connect_type type); // del type.used
        void clrConnection(); // clr all used
        void mapBlk(Baseblk &blk); // must map blk to available tile by dfg algorithm
        void printMappedblks() const;
        friend std::ostream& operator<<(std::ostream& out,const PIM_tile& tile);
};

class PIM_chip{
    private:
        int row, col;// w*h tiles are deployed
        std::vector<std::vector<PIM_tile>> tiles; // tile array, wrapped by std::vector
        // std::list<std::shared_ptr<std::vector<std::pair<int, int>>>> paths; // paths, use list for insert & delete
        std::list<path> paths;
        DFG dfg; // TODO: use kernels to init dfg
        std::pair<int, int> input_size; // input fmap size
        std::vector<std::vector<int>> transfer_matrix; // transfer matrix for dataflow
    public:
        // explicit PIM_chip(int row, int col, int memsize, int blknum, std::pair<int, int> blksize, std::vector<Convkernel> &&kernels, std::function<void(SIMDblk&)> func); // ctor
        /**
         * @brief Construct a new pim chip object
         * 
         * @param row 
         * @param col 
         * @param blknum 
         * @param blksize 
         * @param kernels 
         * @param input_size input_w * input_h is enough
         */
        explicit PIM_chip(int row, int col, int blknum, std::pair<int, int> blksize, std::vector<Convkernel> &&kernels, std::pair<int, int> input_size); /// ctor for mapping test
        std::pair<int, int> getShape() const; // w, h pair
        void initConnection(); // init connection between tiles
        // void addConnection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type);// TODO: add connection from src to dst
        // void removeConnection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type);// TODO: delete connection from src to dst
        void clrConnection();
        void allocMem(int xdst, int ydst, int size); // alloc mem for conv
        void freeMem(int xdst, int ydst, int size); // free mem
        bool allocBlk(int xdst, int ydst, int num); // alloc blk for kernel
        void freeBlk(int xdst, int ydst, int num); // free blk(maybe useless)
        std::function<void(SIMDblk&)> deploySIMDhandler;
        // void deploySIMD(SIMDblk &blk);
        // std::vector<std::pair<int, int>> getNodeIndex(std::vector<std::pair<int, int>> fanins, int size, int fanout);
        void mapDFG(); // impl DFG->tile mapping
        // void addHWConnection(){dfg.addHWConnection();} // build NoC dataflow for opt
        void addHWConnection();// this func must be implmented in chip level, not DFG level
        bool mapBaseblk(Baseblk& blk, int x, int y);
        void addPath(std::shared_ptr<std::vector<std::pair<int, int>>> path, int data_size){paths.emplace_back(path, data_size);}
        friend std::ostream& operator<<(std::ostream& out,const PIM_chip& chip);
        void printDFG(){this->dfg.printBaseblks();}
        void printSIMD(){this->dfg.printSIMDblks();}
        void printMappedblks() const;
        auto& getTransMatrix() const {return transfer_matrix;}
        void setTransMatrix();
        auto& getPaths() const {return paths;}
        void printTransMatrix() const{
            for (const auto& row : transfer_matrix) {
                for (const auto& col : row) {
                    std::cout << col << " ";
                }
                std::cout << std::endl;
            }
        };
        // get tile index to be allocated to SIMDblk
};

#endif
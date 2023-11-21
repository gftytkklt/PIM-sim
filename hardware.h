#ifndef HARDWARE_H
#define HARDWARE_H

#include <memory>
#include <iostream>
#include <vector>
#include <map>
#include <list>

// Direction enum
enum class Direction{
    TopLeft, Top, TopRight,
    Left, Self, Right,
    BottomLeft, Bottom, BottomRight
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
        std::map<Direction, int> SIMD_connect, SRAM_connect;
    public:
        explicit PIM_tile(int memsize, int blk_num); // ctor
        int get_memsize() const; // mem capacity of tile
        int get_blknum() const; // basic blk num of tile
        int get_freeblk() const; // current free blk num of tile
        int get_freemem() const; // current free mem of tile
        void allocate_blk(int num); // alloc free blk
        void allocate_mem(int size); // alloc free mem
        void free_blk(int num); // free blk
        void free_mem(int size); // free mem
        void init_connection(int i, int j, int w, int h); // init tile connection
        void inc_connection(Direction direct, connect_type type); // inc type.used
        void del_connection(Direction direct, connect_type type); // del type.used
        void clr_connection(); // clr all used
        friend std::ostream& operator<<(std::ostream& out,const PIM_tile& tile);
};

class PIM_chip{
    private:
        int row, col;// w*h tiles are deployed
        std::vector<std::vector<PIM_tile>> tiles; // tile array, wrapped by std::vector
        std::list<std::vector<std::pair<int, int>>> paths; // paths 
    public:
        explicit PIM_chip(int row, int col, int memsize, int blknum); // ctor
        std::pair<int, int> get_shape() const; // w, h pair
        void init_connection(); // init connection between tiles
        void add_connection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type);// TODO: add connection from src to dst
        void remove_connection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type);// TODO: delete connection from src to dst
        void clr_connection();
        void alloc_mem(int xdst, int ydst, int size); // alloc mem for conv
        void free_mem(int xdst, int ydst, int size); // free mem
        void alloc_blk(int xdst, int ydst, int num); // alloc blk for kernel
        void free_blk(int xdst, int ydst, int num); // free blk(maybe useless)
        friend std::ostream& operator<<(std::ostream& out,const PIM_chip& chip);
};

#endif
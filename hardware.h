#ifndef HARDWARE_H
#define HARDWARE_H

#include <memory>
#include <iostream>
#include <vector>

struct connectinfo{
    int has_connect;
    int used_num;
};
std::ostream& operator<<(std::ostream& out,const connectinfo& info);

class PIM_tile{
    friend class PIM_chip;
    private:
        int memsize;// local SRAM size
        int blk_num;// basic blk num, now blk size is 1152*256, num is 4
        int free_blk;// free blk can be allocated
        int free_mem;// free mem can be allocated
        struct connectinfo SIMD_connect[3][3];// SIMD-SIMD datapath
        struct connectinfo SRAM_connect[3][3];// SIMD-SRAM datapath
    public:
        explicit PIM_tile(int memsize, int blk_num);
        int get_memsize() const;
        int get_blknum() const;
        int get_freeblk() const;
        int get_freemem() const;
        void allocate_freeblk(int num);
        void allocate_free_mem(int size);
        friend std::ostream& operator<<(std::ostream& out,const PIM_tile& tile);
};

class PIM_chip{
    private:
        int w, h;// w*h tiles are deployed
        std::vector<std::vector<PIM_tile>> tiles;
        // std::unique_ptr<std::unique_ptr<PIM_tile[]>[]> tiles;
    public:
        explicit PIM_chip(int w, int h, int memsize, int blknum);
        std::pair<int, int> get_shape() const;
        friend std::ostream& operator<<(std::ostream& out,const PIM_chip& tile);
};

#endif
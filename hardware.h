#ifndef HARDWARE_H
#define HARDWARE_H

#include <memory>
#include <iostream>

class OPU_tile{
    private:
        int memsize;// local SRAM size
        int blk_num;// basic blk num, now blk size is 1152*256, num is 4
        int free_blk;// free blk can be allocated
        int free_mem;// free mem can be allocated
        int SIMD_connect[3][3];// SIMD-SIMD datapath
        int SRAM_connect[3][3];// SIMD-SRAM datapath
    public:
        explicit OPU_tile(int memsize, int blk_num);
        int get_memsize() const;
        int get_blknum() const;
        int get_freeblk() const;
        int get_freemem() const {return this->free_mem;}
        void allocate_freeblk(int num);
        void allocate_free_mem(int size);
};

class OPU_chip{
    private:
        int w, h;// w*h tiles are deployed
        std::unique_ptr<std::unique_ptr<OPU_tile[]>[]> tiles;
    public:
        explicit OPU_chip(int w, int h, int memsize, int blknum);
        int get_shape() const;
};

std::ostream& operator<<(std::ostream& out,const OPU_tile& tile);
#endif
#include "hardware.h"

// tile impl
PIM_tile::PIM_tile(int memsize=0, int blk_num=0) : 
    memsize(memsize), blk_num(blk_num), free_blk(blk_num), free_mem(memsize), SIMD_connect{}, SRAM_connect{}{}

int PIM_tile::get_memsize() const {
    return this->memsize;
}

int PIM_tile::get_blknum() const {
    return this->blk_num;
}

int PIM_tile::get_freeblk() const {
    return this->free_blk;
}

int PIM_tile::get_freemem() const {
    return this->free_mem;
}

void PIM_tile::allocate_freeblk(int num) {
    if (num > this->free_blk){std::cout << "Failed: out of free blk!" << std::endl;}
    else {this->free_blk -= num;}
}

void PIM_tile::allocate_free_mem(int size) {
    if (size > this->free_mem){std::cout << "Failed: out of free mem!" << std::endl;}
    else {this->free_mem -= size;}
}

// tile array impl

PIM_chip::PIM_chip(int w=0, int h=0, int memsize=0, int blknum=0) : w(w), h(h) {
    // TODO: create (w, h) tile array, init memsize, blknum, connection of each tile
}
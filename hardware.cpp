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

PIM_chip::PIM_chip(int w=0, int h=0, int memsize=0, int blknum=0) : 
    w(w), h(h), tiles(w, std::vector<PIM_tile>(h, PIM_tile(memsize, blknum))) {
    // TODO: create (w, h) tile array, init memsize, blknum, connection of each tile
    // this->tiles = std::make_unique<std::unique_ptr<PIM_tile[]>[]>(w);
    // for (int i = 0; i < w; ++i) {
    //     this->tiles[i] = std::make_unique<PIM_tile[]>(h);
    // }
    for (int i = 0; i < w; ++i) {
        for (int j = 0; j < h; ++j) {
            if (i > 0) this->tiles[i][j].SIMD_connect[0][1].has_connect = 1;
            if (i < w - 1) this->tiles[i][j].SIMD_connect[2][1].has_connect = 1;
            if (j > 0) this->tiles[i][j].SIMD_connect[1][0].has_connect = 1;
            if (j < h - 1) this->tiles[i][j].SIMD_connect[1][2].has_connect = 1;
        }
    }
}

std::pair<int, int> PIM_chip::get_shape() const {
    return std::pair(this->w, this->h);
}

// << overload impl: print info of each tile

std::ostream& operator<<(std::ostream& out,const PIM_chip& chip) {
    auto shape = chip.get_shape();
    for (int i = 0; i < shape.first; ++i){
        for(int j = 0; j < shape.second; ++j){
            out << "tile: " << i << ", " << j << std::endl << chip.tiles[i][j];
        }
        out << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,const PIM_tile& tile) {
    // out << "memsize: " << tile.get_memsize() << " ";
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            out << tile.SIMD_connect[i][j];
        }
        out << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,const connectinfo& info) {
    out << "(" << info.has_connect << "connected, " << info.used_num << "used) ";
    return out;
}
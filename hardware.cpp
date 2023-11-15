#include "hardware.h"

const char* directname[3][3] = {
    "tl", "top", "tr", "left", "self", "right", "bl", "bot", "br"
};
// tile impl
PIM_tile::PIM_tile(int memsize=0, int blk_num=0) : 
    memsize(memsize), blk_num(blk_num), available_blk(blk_num), available_mem(memsize), SIMD_connect{}, SRAM_connect{}{}

int PIM_tile::get_memsize() const {
    return this->memsize;
}

int PIM_tile::get_blknum() const {
    return this->blk_num;
}

int PIM_tile::get_freeblk() const {
    return this->available_blk;
}

int PIM_tile::get_freemem() const {
    return this->available_mem;
}

void PIM_tile::allocate_blk(int num) {
    if (num > this->available_blk){std::cout << "Failed: out of free blk!" << std::endl;}
    else {this->available_blk -= num;}
}

void PIM_tile::allocate_mem(int size) {
    if (size > this->available_mem){std::cout << "Failed: out of free mem!" << std::endl;}
    else {this->available_mem -= size;}
}

void PIM_tile::free_blk(int num) {
    int new_num = num + this->available_blk;
    if(new_num > this->blk_num){std::cout << "Failed: invalid free blk" << std::endl;}
    else {this->available_blk = new_num;}
}

void PIM_tile::free_mem(int size) {
    int new_size = size + this->available_mem;
    if(new_size > this->memsize){std::cout << "Failed: invalid free mem" << std::endl;}
    else {this->available_mem = new_size;}
}

// ith row, jth col with in row*col tile array
void PIM_tile::init_connection(int i, int j, int row, int col) {
    // not top row, add up connection
    if (i > 0) {
        this->SIMD_connect[0][1].has_connect = 1;
        this->SRAM_connect[0][1].has_connect = 1;
    }
    // not bottom row, add down connection
    if (i < row - 1) {
        this->SIMD_connect[2][1].has_connect = 1;
        this->SRAM_connect[2][1].has_connect = 1;
    }
    // not leftmost col, add left connection
    if (j > 0) {
        this->SIMD_connect[1][0].has_connect = 1;
        this->SRAM_connect[1][0].has_connect = 1;
    }
    // not rightmost col, add right connection
    if (j < col - 1) {
        this->SIMD_connect[1][2].has_connect = 1;
        this->SRAM_connect[1][2].has_connect = 1;
    }
}

void PIM_tile::inc_connection(direction direct, connect_type type) {
    auto &info = (type == SIMD) ? this->SIMD_connect : this->SRAM_connect;
    int x, y;
    switch (direct) {
    case LEFT:
        x = 1; y = 0;
        break;
    case RIGHT:
        x = 1; y = 2;
        break;
    case TOP:
        x = 0; y = 1;
        break;
    case DOWN:
        x = 2; y = 1;
        break;
    default:
        std::cout << "Error: should not reach here!" << std::endl;
        break;
    }
    if (info[x][y].has_connect) info[x][y].used_num++;
    else std::cout << "Failed: No path exist!" << std::endl;
}

void PIM_tile::del_connection(direction direct, connect_type type) {
    auto &info = (type == SIMD) ? this->SIMD_connect : this->SRAM_connect;
    int x, y;
    switch (direct) {
    case LEFT:
        x = 1; y = 0;
        break;
    case RIGHT:
        x = 1; y = 2;
        break;
    case TOP:
        x = 0; y = 1;
        break;
    case DOWN:
        x = 2; y = 1;
        break;
    default:
        std::cout << "Error: should not reach here!" << std::endl;
        break;
    }
    if (info[x][y].used_num > 0) info[x][y].used_num--;
    else std::cout << "Failed: No path to delete!" << std::endl;
}

void PIM_tile::clr_connection(){
    for(int i=0; i<3; ++i){
        for(int j=0; j<3; ++j){
            this->SIMD_connect[i][j].used_num = 0;
            this->SRAM_connect[i][j].used_num = 0;
        }
    }
}

// tile array impl
PIM_chip::PIM_chip(int row=0, int col=0, int memsize=0, int blknum=0) : 
    row(row), col(col), tiles(row, std::vector<PIM_tile>(col, PIM_tile(memsize, blknum))) {
    init_connection();
}

std::pair<int, int> PIM_chip::get_shape() const {
    return std::pair(this->row, this->col);
}

void PIM_chip::init_connection() {
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            tiles[i][j].init_connection(i, j, row, col);
        }
    }
}

// default strategy for debug: vertical first, hori next
void PIM_chip::add_connection(int xsrc, int ysrc, int xdst, int ydst) {
    //TODO: impl applicable func
    for (int i=xsrc; i<xdst; i++) {
        this->tiles[i][ysrc].inc_connection(DOWN, SRAM);
        this->tiles[i+1][ysrc].inc_connection(TOP, SRAM);
    }
    for (int j=ysrc; j<ydst; j++){
        this->tiles[xdst][j].inc_connection(RIGHT, SRAM);
        this->tiles[xdst][j+1].inc_connection(LEFT, SRAM);
    }
}

// default strategy for debug: hori first, vertical next
void PIM_chip::remove_connection(int xsrc, int ysrc, int xdst, int ydst) {
    //TODO
}

void PIM_chip::alloc_mem(int xdst, int ydst, int size) {
    auto &tile = this->tiles[xdst][ydst];
    tile.allocate_mem(size);
}
void PIM_chip::free_mem(int xdst, int ydst, int size) {
    auto &tile = this->tiles[xdst][ydst];
    tile.free_mem(size);
}
void PIM_chip::alloc_blk(int xdst, int ydst, int num) {
    auto &tile = this->tiles[xdst][ydst];
    tile.allocate_blk(num);
}
void PIM_chip::free_blk(int xdst, int ydst, int num) {
    auto &tile = this->tiles[xdst][ydst];
    tile.free_blk(num);
}

// << overload impl: print info of each tile

std::ostream& operator<<(std::ostream& out,const PIM_chip& chip) {
    auto shape = chip.get_shape();
    for (int i = 0; i < shape.first; ++i){
        for(int j = 0; j < shape.second; ++j){
            out << "tile: (" << i << ", " << j << ")" << std::endl << chip.tiles[i][j] << std::endl;
        }
        // out << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,const PIM_tile& tile) {
    // out << "memsize: " << tile.get_memsize() << " ";
    out << "SRAM connection: " << std::endl;
    for(int i = 0; i < 3; i++){
        for(int j = 0; j < 3; j++){
            if (tile.SIMD_connect[i][j].has_connect)
                out << directname[i][j] << ": " << tile.SRAM_connect[i][j] << std::endl;
        }
        // out << std::endl;
    }
    out << "freeblk: " << tile.get_freeblk() << ", freemem: " << tile.get_freemem() << std::endl;
    return out;
}

std::ostream& operator<<(std::ostream& out,const connectinfo& info) {
    out << "(" << info.has_connect << "connected, " << info.used_num << "used) ";
    return out;
}
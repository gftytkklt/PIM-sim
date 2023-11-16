#include "hardware.h"
#include "util.h"

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
        this->SIMD_connect[Direction::Top] = 0;
        this->SRAM_connect[Direction::Top] = 0;
    }
    // not bottom row, add down connection
    if (i < row - 1) {
        this->SIMD_connect[Direction::Bottom] = 0;
        this->SRAM_connect[Direction::Bottom] = 0;
    }
    // not leftmost col, add left connection
    if (j > 0) {
        this->SIMD_connect[Direction::Left] = 0;
        this->SRAM_connect[Direction::Left] = 0;
    }
    // not rightmost col, add right connection
    if (j < col - 1) {
        this->SIMD_connect[Direction::Right] = 0;
        this->SRAM_connect[Direction::Right] = 0;
    }
}

void PIM_tile::inc_connection(Direction direct, connect_type type) {
    auto &info = (type == connect_type::SIMD) ? this->SIMD_connect : this->SRAM_connect;
    auto it = info.find(direct);
    if (it != info.end()) it->second += 1;
    else std::cout << "Failed: No available path exist!" << std::endl;
}

void PIM_tile::del_connection(Direction direct, connect_type type) {
    auto &info = (type == connect_type::SIMD) ? this->SIMD_connect : this->SRAM_connect;
    auto it = info.find(direct);
    if (it != info.end()) {
        if(it->second > 0) it->second -= 1;
    }
    else std::cout << "Failed: No path to delete!" << std::endl;
}

void PIM_tile::clr_connection(){
    for (auto& it : SIMD_connect){
        it.second = 0;
    }
    for (auto& it : SRAM_connect){
        it.second = 0;
    }
}

// tile array impl
PIM_chip::PIM_chip(int row=0, int col=0, int memsize=0, int blknum=0) : 
    row(row), col(col), tiles(row, std::vector<PIM_tile>(col, PIM_tile(memsize, blknum))), paths() {
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
    std::vector<std::pair<int, int>> path;
    path.push_back(std::pair(xsrc, ysrc));
    for (int i=xsrc; i<xdst; i++) {
        this->tiles[i][ysrc].inc_connection(Direction::Bottom, connect_type::SRAM);
        this->tiles[i+1][ysrc].inc_connection(Direction::Top, connect_type::SRAM);
        path.push_back(std::pair(i+1, ysrc));
    }
    for (int j=ysrc; j<ydst; j++){
        this->tiles[xdst][j].inc_connection(Direction::Right, connect_type::SRAM);
        this->tiles[xdst][j+1].inc_connection(Direction::Left, connect_type::SRAM);
        path.push_back(std::pair(xdst, j+1));
    }
    this->paths.push_back(path);
}

// default strategy for debug: hori first, vertical next
void PIM_chip::remove_connection(int xsrc, int ysrc, int xdst, int ydst) {
    for (auto it = paths.begin(); it != paths.end(); ) {
        // 检查当前vector是否为空，以及第一个元素是否是pair(0, 0)
        if (!it->empty() && it->front() == std::make_pair(xsrc, ysrc) && it->back() == std::make_pair(xdst, ydst)) {
            it = paths.erase(it); // 删除这个vector并更新迭代器
            return;
        } else {
            ++it; // 否则，继续遍历
        }
    }
    std::cout << "Failed: No such connection!" << std::endl;
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
std::string toString(Direction dir) {
    switch (dir) {
        case Direction::TopLeft:     return "TopLeft";
        case Direction::Top:         return "Top";
        case Direction::TopRight:    return "TopRight";
        case Direction::Left:        return "Left";
        case Direction::Self:        return "Self";
        case Direction::Right:       return "Right";
        case Direction::BottomLeft:  return "BottomLeft";
        case Direction::Bottom:      return "Bottom";
        case Direction::BottomRight: return "BottomRight";
        default:                     return "Unknown";
    }
}

std::ostream& operator<<(std::ostream& out,const PIM_chip& chip) {
    auto shape = chip.get_shape();
    for (int i = 0; i < shape.first; ++i){
        for(int j = 0; j < shape.second; ++j){
            out << "tile: (" << i << ", " << j << ")" << std::endl << chip.tiles[i][j] << std::endl;
        }
    }
    for (const auto& path : chip.paths) {
        std::cout << "Path:" << std::endl;
        for (const auto& p : path) {
            std::cout << "(" << p.first << ", " << p.second << ") -> ";
        }
        std::cout << "end" << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,const PIM_tile& tile) {
    out << "SRAM connection: " << std::endl;
    for(const auto &it : tile.SRAM_connect){
        out << toString(it.first) << ": " << it.second << " connected" << std::endl;
    }
    out << "freeblk: " << tile.get_freeblk() << ", freemem: " << tile.get_freemem() << std::endl;
    return out;
}
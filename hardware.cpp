#include "hardware.h"
#include "util.h"
#include <random>
#include <chrono>
#include <algorithm>
#include <cassert>

// tile impl
PIM_tile::PIM_tile(int memsize=0, int blk_num=0, std::pair<int, int> blk_size = {})
    : memsize{memsize}, blk_num{blk_num}, available_blk{blk_num}, blk_size{blk_size},
    available_mem{memsize}, SIMD_connect{}, SRAM_connect{}, mapped_blks{}{}

int PIM_tile::getMemsize() const {
    return this->memsize;
}

int PIM_tile::getBlknum() const {
    return this->blk_num;
}

int PIM_tile::getFreeblk() const {
    return this->available_blk;
}

int PIM_tile::getFreemem() const {
    return this->available_mem;
}

int PIM_tile::getPortnum(connect_type type) const {
    return (type == connect_type::SIMD) ? this->SIMD_connect.num : this->SRAM_connect.num;
}

std::pair<int, int> PIM_tile::getBlksize() const {
    return this->blk_size;
}

bool PIM_tile::allocateBlk(int num) {
    if (num > this->available_blk) {
        std::cout << "Failed: out of free blk!" << std::endl;
        return false;
    }
    this->available_blk -= num;
    return true;
}

void PIM_tile::allocateMem(int size) {
    if (size > this->available_mem){std::cout << "Failed: out of free mem!" << std::endl;}
    else {this->available_mem -= size;}
}

void PIM_tile::freeBlk(int num) {
    int new_num = num + this->available_blk;
    if(new_num > this->blk_num){std::cout << "Failed: invalid free blk" << std::endl;}
    else {this->available_blk = new_num;}
}

void PIM_tile::freeMem(int size) {
    int new_size = size + this->available_mem;
    if(new_size > this->memsize){std::cout << "Failed: invalid free mem" << std::endl;}
    else {this->available_mem = new_size;}
}

// ith row, jth col with in row*col tile array
void PIM_tile::initConnection(int i, int j, int row, int col) {
    // not top row, add up connection
    if (i > 0) {
        this->SIMD_connect.port[Direction::Top] = 0;
        this->SRAM_connect.port[Direction::Top] = 0;
        this->SIMD_connect.num++;
        this->SRAM_connect.num++;
    }
    // not bottom row, add down connection
    if (i < row - 1) {
        this->SIMD_connect.port[Direction::Bottom] = 0;
        this->SRAM_connect.port[Direction::Bottom] = 0;
        this->SIMD_connect.num++;
        this->SRAM_connect.num++;
    }
    // not leftmost col, add left connection
    if (j > 0) {
        this->SIMD_connect.port[Direction::Left] = 0;
        this->SRAM_connect.port[Direction::Left] = 0;
        this->SIMD_connect.num++;
        this->SRAM_connect.num++;
    }
    // not rightmost col, add right connection
    if (j < col - 1) {
        this->SIMD_connect.port[Direction::Right] = 0;
        this->SRAM_connect.port[Direction::Right] = 0;
        this->SIMD_connect.num++;
        this->SRAM_connect.num++;
    }
}

void PIM_tile::incConnection(Direction direct, connect_type type) {
    auto &info = (type == connect_type::SIMD) ? this->SIMD_connect.port : this->SRAM_connect.port;
    auto it = info.find(direct);
    if ((it != info.end()) && ((it->second == 0) || type == connect_type::SRAM)) {
        it->second += 1;
    }
    else std::cout << "Failed: No available path exist!" << std::endl;
}

void PIM_tile::delConnection(Direction direct, connect_type type) {
    auto &info = (type == connect_type::SIMD) ? this->SIMD_connect.port : this->SRAM_connect.port;
    auto it = info.find(direct);
    if (it != info.end()) {
        if(it->second > 0) it->second -= 1;
    }
    else std::cout << "Failed: No path to delete! " << toString(direct) << std::endl;
}

void PIM_tile::clrConnection(){
    for (auto& it : SIMD_connect.port){
        it.second = 0;
    }
    for (auto& it : SRAM_connect.port){
        it.second = 0;
    }
}

void PIM_tile::mapBlk(Baseblk &blk) {
    this->mapped_blks.push_back(&blk);
}

void PIM_tile::printMappedblks() const {
    for(const auto& it: mapped_blks) {
        it->printBaseblkInfo();
    }
}

// tile array impl
// DEPRECATED NOW, DON'T USE IT!
// PIM_chip::PIM_chip(int row=0, int col=0, int memsize=0, int blknum=0, std::pair<int, int> blksize={}, std::vector<Convkernel> &&kernels = {}, std::function<void(SIMDblk&)> func = {})
//     : row{row}, col{col}, tiles(row, std::vector<PIM_tile>(col, PIM_tile{memsize, blknum, blksize})), paths{}, dfg{std::move(kernels), blksize}, deploySIMDhandler{func} {
//     initConnection();
//     mapDFG();
//     // for debug
//     // auto size = this->dfg.get_blksize();
//     // std::cout << "DFG: " << size.first << " " << size.second << std::endl;
// }

PIM_chip::PIM_chip(int row=0, int col=0, int blknum=0, std::pair<int, int> blksize={}, std::vector<Convkernel> &&kernels = {}, std::pair<int, int> input_size={})
    : row{row}, col{col}, tiles(row, std::vector<PIM_tile>(col, PIM_tile{0, blknum, blksize})), paths{}, dfg{std::move(kernels), blksize, input_size}, input_size{input_size}, transfer_matrix{std::vector<std::vector<int>>(row*col, std::vector<int>(row*col, 0))} {
    initConnection();
    mapDFG();
    addHWConnection();
    setTransMatrix();
}

std::pair<int, int> PIM_chip::getShape() const {
    return std::make_pair(this->row, this->col);
}

void PIM_chip::initConnection() {
    for (int i = 0; i < row; ++i) {
        for (int j = 0; j < col; ++j) {
            tiles[i][j].initConnection(i, j, row, col);
        }
    }
}

// // default strategy for debug: vertical first, hori next
// void PIM_chip::addConnection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type) {
//     //TODO: impl applicable func
//     std::vector<std::pair<int, int>> path;
//     path.push_back(src);
//     for (int i=src.first; i<dst.first; i++) {
//         this->tiles[i][src.second].incConnection(Direction::Bottom, type);
//         this->tiles[i+1][src.second].incConnection(Direction::Top, type);
//         path.push_back(std::make_pair(i+1, src.second));
//     }
//     for (int j=src.second; j<dst.second; j++){
//         this->tiles[dst.first][j].incConnection(Direction::Right, type);
//         this->tiles[dst.first][j+1].incConnection(Direction::Left, type);
//         path.push_back(std::make_pair(dst.first, j+1));
//     }
//     this->paths.push_back(path);
// }

// std::pair<Direction, Direction> getDirection(const std::pair<int, int>& delta) {
//     static const std::map<std::pair<int, int>, std::pair<Direction, Direction>> directionMap = {
//         {{0, 1}, {Direction::Left, Direction::Right}},
//         {{0, -1}, {Direction::Right, Direction::Left}},
//         {{1, 0}, {Direction::Top, Direction::Bottom}},
//         {{-1, 0}, {Direction::Bottom, Direction::Top}}
//     };

//     auto it = directionMap.find(delta);
//     if (it != directionMap.end()) {
//         return it->second;
//     }

//     throw std::runtime_error("Invalid delta value");
// }

// // remove connection of adjacent tiles in a path
// void PIM_chip::removeConnection(std::pair<int, int> src, std::pair<int, int> dst, connect_type type) {
//     for (auto it = paths.begin(); it != paths.end(); ) {
//         // non-empty list && <src, dest> match
//         if (!it->empty() && it->front() == src && it->back() == dst) {
//             auto path = *it;
//             for (size_t i = 0; i < path.size() - 1; ++i) {
//                 auto start = path[i];
//                 auto end = path[i+1];
//                 auto delta = std::make_pair(start.first-end.first, start.second-end.second);
//                 auto direct = getDirection(delta);
//                 auto &tile_start = this->tiles[start.first][start.second];
//                 auto &tile_end = this->tiles[end.first][end.second];
//                 std::cout << "direction: " << toString(direct.first) << ", " << toString(direct.second) << std::endl;
//                 tile_start.delConnection(direct.first, type);
//                 tile_end.delConnection(direct.second, type);
//             }
//             it = paths.erase(it);
//             return;
//         }
//         ++it;
//     }
//     std::cout << "Failed: No such connection!" << std::endl;
// }

void PIM_chip::allocMem(int xdst, int ydst, int size) {
    auto &tile = this->tiles[xdst][ydst];
    tile.allocateMem(size);
}
void PIM_chip::freeMem(int xdst, int ydst, int size) {
    auto &tile = this->tiles[xdst][ydst];
    tile.freeMem(size);
}
bool PIM_chip::allocBlk(int xdst, int ydst, int num) {
    auto &tile = this->tiles[xdst][ydst];
    return tile.allocateBlk(num);
}
void PIM_chip::freeBlk(int xdst, int ydst, int num) {
    auto &tile = this->tiles[xdst][ydst];
    tile.freeBlk(num);
} 
// // impl allocate freeblk here
// std::vector<std::pair<int, int>> PIM_chip::getNodeIndex(std::vector<std::pair<int, int>> fanins, int size, int fanout) {
//     for(size_t i = 0; i < tiles.size(); ++i){
//         for (size_t j = 0; j < tiles[i].size(); ++j) {
//             auto &tile = tiles[i][j];
//             if(tile.get_portnum(connect_type::SRAM) >= fanout){

//             }
//         }
//     }
// }
// // under layer by layer deploy logic
// void PIM_chip::deploySIMD(SIMDblk &blk) {
//     int size = blk.getBaseblks().size();
//     std::cout << "size: " << size << std::endl;
//     int fanout = blk.getFanout();
//     auto parents = blk.getParent();
//     std::vector<std::pair<int, int>> fanin{};
//     std::vector<std::pair<int, int>> locations{};
//     if(!parents.empty()) {
//         for (const auto &parent : parents){
//             fanin.emplace_back(parent->getFanoutloc());
//         }
//     }
//     unsigned seed = std::chrono::system_clock::now().time_since_epoch().count();
//     std::default_random_engine engine(seed);

//     // 生成行索引的随机顺序
//     std::vector<int> rowIndexes(tiles.size());
//     std::iota(rowIndexes.begin(), rowIndexes.end(), 0); // 填充0到tiles.size()-1
//     std::shuffle(rowIndexes.begin(), rowIndexes.end(), engine);
//     bool findfanout = false;
//     std::pair<int, int> fanout_loc{};
//     for(int row : rowIndexes) {
//         // 生成列索引的随机顺序
//         std::vector<int> colIndexes(tiles[row].size());
//         std::iota(colIndexes.begin(), colIndexes.end(), 0); // 填充0到tiles[row].size()-1
//         std::shuffle(colIndexes.begin(), colIndexes.end(), engine);
//         for(int col : colIndexes) {
//             auto &tile = tiles[row][col];
//             if((tile.get_portnum(connect_type::SRAM) >= fanout) && (!tile.allocate_blk(1))){
//                 fanout_loc = std::make_pair(row, col);
//                 findfanout = true;
//                 blk.setFanoutloc(fanout_loc);
//                 locations.push_back(fanout_loc);
//                 break;
//             }
//         }
//         if(findfanout) {
//             break;
//         }
//     }
//     // deploy other blks: gen ramdom pts, connect it
//     if (size > 1){
//         auto dst = randomPointWithManhattanDistance(this->row, this->col, fanout_loc.first, fanout_loc.second, size-1);
//         // add_connection(fanout_loc, dst, connect_type::SIMD);
//         // std::cout << "from (" << fanout_loc.first << ", " << fanout_loc.second << ") to (" << dst.first << ", " << dst.second << std::endl;
//         int deltax = dst.first > fanout_loc.first ? 1 : -1;
//         int deltay = dst.second > fanout_loc.second ? 1 : -1;
//         while((fanout_loc.first != dst.first) || (fanout_loc.second != dst.second)){
//             if(fanout_loc.second != dst.second){
//                 fanout_loc.second += deltay;
//                 locations.push_back(fanout_loc);
//                 // std::cout << "push (" << fanout_loc.first << ", " << fanout_loc.second << std::endl;
//                 continue;
//             }
//             fanout_loc.first += deltax;
//             locations.push_back(fanout_loc);
//             // std::cout << "push (" << fanout_loc.first << ", " << fanout_loc.second << std::endl;
//         }
//     }
//     // locations.size should be equal to baseblks
//     auto baseblks = blk.getBaseblks();
//     // std::cout << "size: " << locations.size() << " vs " << baseblks.size() << std::endl;
//     assert(locations.size() == baseblks.size());
//     for (int i=0; i<locations.size(); i++){
//         int x = locations[i].first;
//         int y = locations[i].second;
//         tiles[x][y].allocate_blk(1);
//         tiles[x][y].map_blk(baseblks[i]);
//     }
//     // bool findfanout = false;
//     // std::pair<int, int> fanout_loc{};
//     // // find fan out location
//     // for(int i = 0; i < tiles.size(); ++i) {
//     //     for (int j = 0; j < tiles[i].size(); ++j) {
//     //         auto &tile = tiles[i][j];
//     //         if((tile.get_portnum(connect_type::SRAM) >= fanout) && (!tile.allocate_blk(1))){
//     //             fanout_loc = std::make_pair(i, j);
//     //             findfanout = true;
//     //             locations.push_back(fanout_loc);
//     //             break;
//     //         }
//     //     }
//     //     if(findfanout) {
//     //         break;
//     //     }
//     // }
// }

// rand strategy now
// impl hardware data struct now
// TODO: impl DFG data struct
// algorithm preprocess may depend on DFG struct only
void PIM_chip::mapDFG() {
    // TODO: impl me
    if (1) {
        auto [x, y] = this->getShape();
        auto size = x*y;
        auto i = 0;
        for(auto& blk : dfg.getBaseblk()){
            auto cur_index = (i++) % size;
            auto cur_x = cur_index / y;
            auto cur_y = cur_index % y;
            mapBaseblk(blk, cur_x, cur_y);
        }
        return;
    }
    if(!deploySIMDhandler) {
        std::cout << "No func handler provided!\n";
        return;
    }
    auto SIMDblks = this->dfg.getSIMDblk();
    for (auto it = SIMDblks.begin(); it!= SIMDblks.end(); ++it){
        // int cursize = it->getBaseblks().size();
        // int fanout = it->getFanout();
        // // have parents
        // std::vector<std::pair<int, int>> fanin{};
        // auto parents = it->getParent();
        // if(!parents.empty()){
        //     for (const auto &parent : parents){
        //         fanin.emplace_back(parent->getFanoutloc());
        //     }
        // }
        deploySIMDhandler(*it);
        // auto index = getNodeIndex(fanin, cursize, fanout);
    }
}

/**
 * @brief notice that chip.paths & baseblk.dataflow.path.route is different lists
 * so add/remove elements of two lists should occur together
 * 
 */
void PIM_chip::addHWConnection(){
    auto& blks = dfg.getBaseblk();
    for (auto& blk: blks){
        auto src = blk.getLocation();
        for (auto& successor: blk.getSuccessors()){
            auto dst = successor->getLocation();
            auto path = initXYRouting(src, dst);
            blk.addRoute(successor, path);
            addPath(path, blk.getDatasize(successor));
        }
    }
}

bool PIM_chip::mapBaseblk(Baseblk& blk, int x, int y){
    if(allocBlk(x, y, 1)){
        tiles[x][y].mapBlk(blk);
        blk.setLocation(std::make_pair(x, y));
        return true;
    }
    return false;
}

void PIM_chip::printMappedblks() const {
    for (size_t i = 0; i < tiles.size(); ++i) {
        for (size_t j = 0; j < tiles[i].size(); ++j) {
            std::cout << "tile(" << i << ", " << j << "): " << std::endl;
            tiles[i][j].printMappedblks();
        }
    }
}

void PIM_chip::setTransMatrix() {
    for(auto& path: paths){
        auto& route = *path.route;
        for(size_t i = 0; i < route.size() - 1; ++i){
            auto start = route[i];
            auto end = route[i+1];
            transfer_matrix[start.first*col+start.second][end.first*col+end.second] += path.data_size;
        }
    }
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
    auto shape = chip.getShape();
    for (int i = 0; i < shape.first; ++i){
        for(int j = 0; j < shape.second; ++j){
            out << "tile: (" << i << ", " << j << ")" << std::endl << chip.tiles[i][j] << std::endl;
        }
    }
    for (const auto& path : chip.paths) {
        std::cout << "Path: " << std::endl;
        for (const auto& p : *path.route) {
            std::cout << "(" << p.first << ", " << p.second << ") -> ";
        }
        std::cout << "end, " << "data size: " << path.data_size << std::endl;
    }
    return out;
}

std::ostream& operator<<(std::ostream& out,const PIM_tile& tile) {
    out << "SRAM connection: " << std::endl;
    for(const auto &it : tile.SRAM_connect.port){
        if(it.second > 0)
            out << toString(it.first) << ": " << it.second << " connected" << std::endl;
    }
    out << "freeblk: " << tile.getFreeblk() << ", freemem: " << tile.getFreemem() << std::endl;
    return out;
}
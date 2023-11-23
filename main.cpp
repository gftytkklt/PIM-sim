#include <iostream>
#include "hardware.h"
int main(){
    /*
    // TEST1: test tile func impl(tile may become private class of chip)
    PIM_tile tile(5, 4);
    std::cout << "Hello world! " << std::endl;
    std::cout << tile.get_memsize() << " ";
    std::cout << tile.get_blknum() << " ";
    std::cout << tile.get_freeblk() << std::endl;
    tile.allocate_blk(1);
    std::cout << "after alloc1: " << tile.get_freeblk() << std::endl;
    tile.allocate_blk(2);
    std::cout << "after alloc2: " << tile.get_freeblk() << std::endl;
    tile.allocate_blk(3);
    std::cout << "after alloc3: " << tile.get_freeblk() << std::endl;
    */
   /*
    // TEST2: test chip func impl
    struct Convkernel kernel{1, 3, 3, 1, 256, 384};
    PIM_chip chip(3, 3, 10, 4, std::make_pair(1152, 256), std::vector<Convkernel>{kernel});
    std::cout << "init: " << std::endl << chip << std::endl;
    auto shape = chip.get_shape();
    int row = shape.first;
    int col = shape.second;
    std::cout << "shape(w, h): " << row << " " << col << std::endl;
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            chip.alloc_mem(i, j, i*row + j);
            chip.alloc_blk(i, j, row*col-i*row - j);
            if(i == 0) chip.add_connection(std::make_pair(i, j), std::make_pair(i+1, j), connect_type::SIMD);
            if(j == 0) chip.add_connection(std::make_pair(i, j), std::make_pair(i, j+1), connect_type::SRAM);
        }
    }
    std::cout << "deploy: " << std::endl << chip << std::endl;
    chip.remove_connection(std::make_pair(0, 0), std::make_pair(1, 0), connect_type::SIMD);
    chip.remove_connection(std::make_pair(0, 0), std::make_pair(1, 0), connect_type::SIMD);
    chip.remove_connection(std::make_pair(0, 1), std::make_pair(1, 1), connect_type::SIMD);
    std::cout << "remove: " << std::endl << chip << std::endl;
    */
    // TEST3: test conv kernel init
    struct Convkernel kernel1{1, 3, 3, 1, 256, 384};
    struct Convkernel kernel2{2, 3, 3, 1, 384, 384};
    struct Convkernel kernel3{3, 3, 3, 1, 384, 256};
    PIM_chip chip(3, 3, 10, 4, std::make_pair(1152, 256), std::vector<Convkernel>{kernel1, kernel2, kernel3});
    chip.printDFG();
    // TEST4: test SIMD blk init
    chip.printSIMD();
    // TEST5: test SIMD blk mapping
    return 0;
}
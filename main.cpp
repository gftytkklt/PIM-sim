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
    // TEST2: test chip func impl
    PIM_chip chip(3, 3, 10, 4);
    std::cout << "init: " << std::endl << chip << std::endl;
    auto shape = chip.get_shape();
    int row = shape.first;
    int col = shape.second;
    std::cout << "shape(w, h): " << row << " " << col << std::endl;
    for (int i = 0; i < row; i++) {
        for (int j = 0; j < col; j++) {
            chip.alloc_mem(i, j, i*row + j);
            chip.alloc_blk(i, j, 4-i*row - j);
            if(i == 0) chip.add_connection(i, j, i+1, j);
            if(j == 0) chip.add_connection(i, j, i, j+1);
        }
    }
    std::cout << "deploy: " << std::endl << chip << std::endl;
    return 0;
}
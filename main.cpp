#include <iostream>
#include "hardware.h"
int main(){
    OPU_tile tile(5, 4);
    std::cout << "Hello world! " << std::endl;
    std::cout << tile.get_memsize() << " ";
    std::cout << tile.get_blknum() << " ";
    std::cout << tile.get_freeblk() << std::endl;
    tile.allocate_freeblk(1);
    std::cout << "after alloc1: " << tile.get_freeblk() << std::endl;
    tile.allocate_freeblk(2);
    std::cout << "after alloc2: " << tile.get_freeblk() << std::endl;
    tile.allocate_freeblk(3);
    std::cout << "after alloc3: " << tile.get_freeblk() << std::endl;
    tile.~OPU_tile();
    return 0;
}
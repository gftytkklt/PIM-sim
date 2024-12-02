#include "graph.h"
#include <iostream>
#include <vector>
#include <memory>

int main() {
    std::vector<NNkernel> kernels = { { {3,3}, {256,384}, 1 }, { {3,3}, {384,384}, 2 } };
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels);
    cg->analysis();
    // print cg info
    std::cout << "CGraph info:" << std::endl;
    // cg->print_graph_info();
    return 0;
}
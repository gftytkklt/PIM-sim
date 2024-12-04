// #ifndef GRAPH_HPP
// #define GRAPH_HPP

#include "graph.h"
CGraph::CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size) : cg{}, kernels{kernels}, CNode_size{CNode_size} {
    
}

std::ostream& operator<<(std::ostream& os, const CNode& cnode) {
    os << "Layer: " << cnode.layer << std::endl;
    // os << "Size: (" << cnode.size.first << ", " << cnode.size.second << ")" << std::endl;
    os << "Ofmap size: " << cnode.ofmap_size << std::endl;
    os << "Channel index: (" << cnode.id_cin.first << ", " << cnode.id_cin.second << ")" << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const CEdge& cedge) {
    os << "Dependency type: ";
    switch (cedge.c_type) {
        case DepType::Accum:
            os << "Accumulation";
            break;
        case DepType::Prop:
            os << "Propagation";
            break;
        default:
            os << "Unknown";
            break;
    }
    os << std::endl;
    os << "Data volume: " << cedge.datavolume << std::endl;
    return os;
}
// #endif
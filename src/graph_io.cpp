#include "graph.h"

std::ostream& operator<<(std::ostream& os, const CNode& cnode) {
    os << "Layer: " << cnode.layer << std::endl;
    os << "Ofmap size: " << cnode.ofmap_size << std::endl;
    os << "In Channel index: (" << cnode.id_cin.first << ", " << cnode.id_cin.second << ")" << std::endl;
    os << "Out Channel index: (" << cnode.id_cout.first << ", " << cnode.id_cout.second << ")" << std::endl;
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
    os << "Channel id: (" << cedge.channel_id.first << ", " << cedge.channel_id.second << ")" << std::endl;
    os << "Data volume: " << cedge.datavolume << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const TNode& tnode) {
    os << "CNode id: ";
    for (const auto& i : tnode.cnode_id) {
        os << i << " ";
    }
    os << std::endl;
    os << "Parent id: ";
    for (const auto& i : tnode.parent_id) {
        os << i << " ";
    }
    os << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const TEdge& tedge) {
    os << "Dependency type: ";
    switch (tedge.t_type) {
        case DepType::Accum:
            os << "Accumulation";
            break;
        case DepType::Prop:
            os << "Propagation";
            break;
        case DepType::Mixed:
            os << "Mixed";
            break;
        default:
            os << "Unknown";
            break;
    }
    os << std::endl;
    os << "Total Data volume: " << tedge.accvolume + tedge.propvolume << std::endl;
    os << "Accumulation Data volume: " << tedge.accvolume << std::endl;
    os << "Propagation Data volume: " << tedge.propvolume << std::endl;
    for (const auto& [layer, volume] : tedge.layer_map) {
        os << "Layer " << layer << ": " << volume << std::endl;
    }
    return os;
}

std::ostream& operator<<(std::ostream& os, const HNode& hnode) {
    if (hnode.mapped) {
        os << "TNode id: " << hnode.tnode_id << std::endl;
    } else {
        os << "TNode id: " << "unmapped" << std::endl;
    }
    os << "Tile id: (" << hnode.tile_id.first << ", " << hnode.tile_id.second << ")" << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const HEdge& hedge) {
    os << "Path set: ";
    for (const auto& i : hedge.pathset) {
        os << "(" << i.first << ", " << i.second << ") ";
    }
    os << "Data volume: " << hedge.datavolume << std::endl;
    return os;
}

std::ostream& operator<<(std::ostream& os, const DNode& dnode) {
    os << "TNode id: " << dnode.tnode_id << std::endl;
    os << "Tile id: (" << dnode.tile_id.first << ", " << dnode.tile_id.second << ")" << std::endl;
    return os;
}
// #ifndef GRAPH_HPP
// #define GRAPH_HPP

#include "graph.h"
#include <boost/graph/dijkstra_shortest_paths.hpp>
CGraph::CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size) : cg{}, kernels{kernels}, CNode_size{CNode_size} {
    for (int i = 0; i < kernels.size(); i++) {
        add_node(CNode{i, kernels[i].fmap_size, std::make_pair(1, kernels[i].channel.first), std::make_pair(1, kernels[i].channel.second),kernels[i].depinfo},cg);
    }
    for (int i = 0; i < kernels.size(); i++) {
        for (int j = 0; j < kernels.size(); j++) {
            if (i != j) {
                add_edge(i, j, CEdge{DepType::Prop, i+10*j},cg);
            }
        }
    }
}

void CGraph::analysis(){
    auto coords_map = boost::get(&CNode::id_cin, cg);
    for(auto v : boost::make_iterator_range(vertices(cg))){
        auto sth = coords_map[v]; // attribute getter
        if(sth == std::make_pair(1,384)){
            std::cout << "Find Node: " << v << std::endl;
            std::cout << get_node_property(v,cg) << std::endl;
            cg[v].ofmap_size += 1; // setter
            std::cout << get_node_property(v,cg) << std::endl;
        }
        // std::cout << "Node " << v << " : input channel" << coords_map[v].first << coords_map[v].second << std::endl;
    }
    std::vector<int> dist(boost::num_vertices(cg));
    std::vector<Node> pred(num_vertices(cg));
    auto weight_map = boost::get(&CEdge::datavolume, cg);
    auto source = boost::vertex(0, cg);

    boost::dijkstra_shortest_paths(cg, source, 
            // boost::predecessor_map(boost::make_iterator_property_map(pred.begin(), boost::get(boost::vertex_index, cg)))
            // .distance_map(boost::make_iterator_property_map(dist.begin(), boost::get(boost::vertex_index, cg)))
            boost::predecessor_map(&pred[0])
            .distance_map(&dist[0])
            .weight_map(weight_map)
    );

    std::cout << "Distances from node 1:" << std::endl;
    for (size_t i = 0; i < dist.size(); ++i) {
        std::cout << "Node " << i + 1 << ": " << dist[i] << std::endl;
    }

    // 输出路径
    std::cout << "Paths:" << std::endl;
    for (size_t i = 0; i < pred.size(); ++i) {
        std::cout << "Node " << i + 1 << ": ";
        if (pred[i] != boost::graph_traits<Graph>::null_vertex()) {
            std::cout << pred[i] + 1 << std::endl;  // 打印前驱节点
        } else {
            std::cout << "No predecessor (source node)" << std::endl;
        }
    }
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
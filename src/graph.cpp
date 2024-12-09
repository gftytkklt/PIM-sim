// #ifndef GRAPH_HPP
// #define GRAPH_HPP

#include "graph.h"
#include <boost/graph/dijkstra_shortest_paths.hpp>
CGraph::CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size) 
    : cg{}, kernels{kernels}, CNode_size{CNode_size}, dep_infos{} {
    analysis();
}

void CGraph::analysis() {
    create_cnodes();
    conn_accblk();
    inter_layer_conn();
}

void CGraph::create_cnodes(){
    // create cnodes kernel-wise
    for (const auto& i: kernels) {
        // determine in/out chan num of a cnode
        int window_size = i.wsize.first * i.wsize.second;
        int in_chan = (CNode_size.first + window_size - 1) / window_size;
        int out_chan = CNode_size.second;
        // cur layer info
        auto [ker_in, ker_out] = i.channel;
        auto cur_l = i.layer;
        const auto& cur_dep = i.depinfo;
        auto cur_ifm = i.ifmap_size;
        auto cur_ofm = i.ofmap_size;
        // construct nodes under the size constraints of (in_chan, out_chan)
        int co_begin = 0;
        std::vector<AccBlk> accblks{};
        while (co_begin < ker_out) {
            // create the segmentation along the output channel dimension
            int co_end = std::min(co_begin + out_chan, ker_out);
            auto co_id = std::make_pair(co_begin+1, co_end);
            int ci_begin = 0;
            std::vector<Node> accblk{};
            while(ci_begin < ker_in){
                // create the segmentation along the input channel dimension
                int ci_end = std::min(ci_begin + in_chan, ker_in);
                auto ci_id = std::make_pair(ci_begin+1, ci_end);
                // instantiate a cnode
                // TODO: impl ofm calculation
                int cur_fmap = -1;
                // last accblk: generate ofm
                if(ci_end == ker_in) {
                    cur_fmap = cur_ofm.first * cur_ofm.second * (co_end - co_begin);
                }
                // other accblk: generate ifm
                else {
                    cur_fmap = cur_ifm.first * cur_ifm.second * (co_end - co_begin);
                }
                auto cnode = CNode{cur_l, cur_fmap, ci_id, co_id};
                // add node to accblk
                accblk.emplace_back(add_node(cnode, cg));
                // update ci_begin
                ci_begin = ci_end;
            }
            // add accblk to accblks group
            accblks.emplace_back(AccBlk{accblk, co_id});
            // update co_begin
            co_begin = co_end;
        }
        // add accblks gropu to dep_infos
        dep_infos.emplace_back(CDep{accblks, cur_l, cur_dep});
    }
}

void CGraph::conn_accblk(){
    for (const auto& v : dep_infos){
        for (const auto& blk : v.acc_blks){
            const auto vertexs = blk.vertex_id;
            auto node_num = vertexs.size();
            // skip conn for empty(should not happen) or single node group
            if (node_num <= 1) {continue;}
            const auto cur_ofm = get_node_property(vertexs[0], cg).ofmap_size;
            // TODO: impl fmap cal
            for (int i=0;i<node_num-1;i++){
                // since input channel are impl in order
                // and accum order is commutable
                add_edge(vertexs[i], vertexs[i+1], CEdge{DepType::Accum,cur_ofm},cg);
            }
        }
    }
}

void CGraph::inter_layer_conn(){
    for (const auto& v : dep_infos){
        // get acc blks and dep info
        const auto& deps = v.dep_info;
        for (const auto& src : v.acc_blks){
            // get cur data volume and output channel id
            auto src_node = src.vertex_id.back();
            const auto src_property = get_node_property(src_node,cg);
            auto cur_datavolume = src_property.ofmap_size;
            auto cur_cin_num = src_property.id_cin.second - src_property.id_cin.first + 1;
            auto co_src = src.cout_id;
            // get dst node
            for (const auto& dep : deps){
                // get dep layer info struct
                for (const auto& dst_layer : dep_infos[dep.dep_layer].acc_blks){
                    for (const auto& dst_node : dst_layer.vertex_id){
                        auto ci_dst = get_node_property(dst_node, cg).id_cin;
                        // get intersection num
                        auto start = std::max(co_src.first, ci_dst.first);
                        auto end = std::min(co_src.second, ci_dst.second);
                        if(start <= end){
                            auto overlap = end - start + 1;
                            // add edge and corresponding data volume
                            add_edge(src_node, dst_node, CEdge{DepType::Prop, cur_datavolume * overlap / cur_cin_num}, cg);
                        }
                    }
                }
            }
        }
    }
}

void CGraph::print_graph_info() const{ 
    std::cout << "Graph info:" << std::endl;
    BaseGraph<CNode, CEdge>::print_graph_info(cg);
    std::cout << "AccBlk info:" << std::endl;
    for(const auto&v : dep_infos){
        std::cout << "Layer: " << v.layer << std::endl;
        for(const auto& blk : v.acc_blks){
            std::cout << "AccBlk: " << blk.cout_id.first << " - " << blk.cout_id.second << std::endl;
            for(const auto& node : blk.vertex_id){
                std::cout << "Node: " << node << std::endl;
            }
        }
    }
}

void CGraph::debug(){
    auto coords_map = boost::get(&CNode::id_cin, cg);
    for(auto v : boost::make_iterator_range(vertices(cg))){
        auto sth = coords_map[v]; // attribute getter
        if(sth == std::make_pair(1,384)){
            std::cout << "Find Node: " << v << std::endl;
            std::cout << get_node_property(v,cg) << std::endl;
            cg[v].ofmap_size += 1; // setter
            std::cout << get_node_property(v,cg) << std::endl;
        }
    }
    std::vector<int> dist(boost::num_vertices(cg));
    std::vector<Node> pred(num_vertices(cg));
    auto weight_map = boost::get(&CEdge::datavolume, cg);
    auto source = boost::vertex(0, cg);

    boost::dijkstra_shortest_paths(cg, source, 
            boost::predecessor_map(&pred[0])
            .distance_map(&dist[0])
            .weight_map(weight_map)
    );

    std::cout << "Distances from node 1:" << std::endl;
    for (size_t i = 0; i < dist.size(); ++i) {
        std::cout << "Node " << i + 1 << ": " << dist[i] << std::endl;
    }

    // output paths
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
    os << "Data volume: " << cedge.datavolume << std::endl;
    return os;
}
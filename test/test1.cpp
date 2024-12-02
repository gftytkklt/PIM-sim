#include "graph.h"
#include <iostream>
#include <vector>
#include <memory>
// this test will be deprecated after the graph class is implemented
// since all func in basegraph will be setted to protected
int main() {
    if(0){
        std::cout << "##### deprecated test #####" << std::endl;
        return 0;
    }

    std::cout << "\n##### test basic func #####" << std::endl;
    std::vector<NNkernel> kernels = { { {3,3}, {256,384}, 1 }, { {3,3}, {384,384}, 2 } };
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels);
    cg->analysis();
    // print cg info
    std::cout << "test get graph attributes:" << std::endl;
    std::cout << "cg size: " << cg->get_graph().m_vertices.size() << std::endl;
    std::cout << "cg edge size: " << cg->get_graph().m_edges.size() << std::endl;
    auto& graph = cg->get_graph();
    cg->add_edge(0, 1,  CEdge{DepType::Accum, 1}, graph);
    std::cout << "cg edge size: " << cg->get_graph().m_edges.size() << std::endl;
    auto node = cg->get_graph().m_vertices[0];
    auto test = node.m_property.m_value;
    std::cout << "node: " << test << std::endl;
    // auto node1 = cg->get_node_property(1, cg->get_graph());
    auto node1 = cg->get_node_property(1);
    std::cout << "node1: " << node1 << std::endl;
    // const auto& edge = cg->get_edge_property(0, 1, graph);
    // std::cout << "edge: " << edge << std::endl;
    cg->remove_edge(0, 1, graph);
    std::cout << "cg edge size: " << cg->get_graph().m_edges.size() << std::endl;
    cg->add_node(CNode{0, {3,3}, 1, {256,384}, {256,384}}, graph);
    std::cout << "cg size: " << cg->get_graph().m_vertices.size() << std::endl;
    cg->remove_node(0, graph);
    std::cout << "cg size: " << cg->get_graph().m_vertices.size() << std::endl;
    // cg->print_graph_info();
    std::cout << "##### test basic func end #####\n" << std::endl;
    return 0;
}
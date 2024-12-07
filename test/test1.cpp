#include "graph.h"
#include <gtest/gtest.h>

// deprecated test because graph op is setted to protected
class GraphTest : public ::testing::Test {
protected:
    std::vector<NNkernel> kernels = { 
    {1, {3,3}, {256,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},1000 }, 
    {2, {3,3}, {384,384}, std::vector<Depinfo>{{3,std::make_pair(1,384)}},2000 },
    {3, {3,3}, {384,256}, std::vector<Depinfo>{{0,std::make_pair(0,0)}},3000 } 
    };
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256));
    decltype(cg->get_graph()) graph = cg->get_graph();
    // void SetUp() override {
    //     graph = cg->get_graph();
    // }
};

TEST_F(GraphTest, TESTMAP){
    cg->debug();
    cg->print_graph_info();
}

// TEST_F(GraphTest, DEPRECATED){
//     std::cout << "##### deprecated test for protected graph operations in debug mode #####" << std::endl;
// }   

// TEST_F(GraphTest, TEST0){
//     ASSERT_EQ(2, boost::num_vertices(graph));
//     ASSERT_EQ(0, boost::num_edges(graph));
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 100}, graph);
//     cg->add_edge(1, 0,  CEdge{DepType::Accum, 200}, graph);
//     cg->add_node(CNode{0, {3,3}, 1, {256,384}, {256,384}}, graph);
//     cg->add_edge(0, 2,  CEdge{DepType::Accum, 300}, graph);
//     cg->print_graph_info();
//     cg->remove_node(1, graph);
//     cg->print_graph_info();
//     cg->remove_node(20, graph);
//     cg->print_graph_info();
// }

// TEST_F(GraphTest, TEST01){
//     // cg->remove_node(30, graph);
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 100}, graph);
//     cg->remove_edge(0, 10, graph);
//     cg->print_graph_info();
//     cg->remove_node(10, graph);
//     cg->print_graph_info();
// }

// TEST_F(GraphTest, TEST02) {
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 100}, graph);
//     cg->add_edge(1, 0,  CEdge{DepType::Accum, 200}, graph);
//     cg->remove_node(0, graph);
//     // auto data = cg->get_adjacent_nodes(0, graph);
//     // for (auto& d : data) {
//     //     std::cout << "fuck" << d << std::endl;
//     // }
//     // auto edges = cg->get_adjacent_edges(1, graph);
//     // std::cout << edges.size() << std::endl;
//     cg->print_graph_info();
// }

// TEST_F(GraphTest, TEST1){
//     ASSERT_EQ(2, boost::num_vertices(graph));
//     ASSERT_EQ(0, boost::num_edges(graph));
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 100}, graph);
//     ASSERT_EQ(1, boost::num_edges(graph));
//     auto adj_nodes = cg->get_adjacent_nodes(0, graph);
//     ASSERT_EQ(1, adj_nodes.size());
//     const auto& node1 = cg->get_node_property(1, graph);
//     ASSERT_EQ(81, node1.layer);
//     cg->set_node_property(1, CNode{1000, {30,30}, 2, {384,384}, {384,384}}, graph);
//     ASSERT_EQ(1000, cg->get_node_property(1, graph).layer);
//     const auto& edge = cg->get_edge_property(0, 1, graph);
//     ASSERT_EQ(DepType::Accum, edge.c_type);
//     cg->set_edge_property(0, 1, CEdge{DepType::Prop, 200}, graph);
//     ASSERT_EQ(DepType::Prop, cg->get_edge_property(0, 1, graph).c_type);
//     auto adj_edges = cg->get_adjacent_edges(0, graph);
//     ASSERT_EQ(1, adj_edges.size());
//     cg->remove_edge(0, 1, graph);
//     ASSERT_EQ(0, boost::num_edges(graph));
//     const auto& edge1 = cg->get_edge_property(0, 1, graph);
//     ASSERT_EQ(DepType::ErrorType, edge1.c_type);
//     cg->add_node(CNode{0, {3,3}, 1, {256,384}, {256,384}}, graph);
//     ASSERT_EQ(3, boost::num_vertices(graph));
//     cg->remove_node(0, graph);
//     ASSERT_EQ(2, boost::num_vertices(graph));
//     cg->print_graph_info();
// }

// TEST_F(GraphTest, TEST2){
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 200}, graph);
//     cg->add_edge(1, 0,  CEdge{DepType::Prop, 300}, graph);
//     auto adj_nodes = cg->get_adjacent_nodes(0, graph);
//     ASSERT_EQ(1, adj_nodes.size());
//     auto adj_edges = cg->get_adjacent_edges(0, graph);
//     ASSERT_EQ(1, adj_edges.size());
//     cg->print_graph_info();
//     cg->remove_edge(0, 1, graph);
//     cg->remove_edge(1, 0, graph);
//     ASSERT_EQ(0, boost::num_edges(graph));
//     ASSERT_EQ(2, boost::num_vertices(graph));
//     cg->print_graph_info();
// }

// TEST_F(GraphTest, TEST3){
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 400}, graph);
//     cg->print_graph_info();
//     cg->add_edge(1, 0,  CEdge{DepType::Prop, 500}, graph);
//     cg->print_graph_info();
//     cg->add_node(CNode{0, {3,3}, 1, {256,384}, {256,384}}, graph);
//     cg->print_graph_info();
//     cg->add_node(CNode{1, {3,3}, 1, {256,384}, {256,384}}, graph);
//     cg->print_graph_info();
//     cg->add_node(CNode{2, {3,3}, 1, {256,384}, {256,384}}, graph);
//     cg->print_graph_info();
//     cg->remove_node(0, graph);
//     cg->print_graph_info();
//     cg->remove_node(1, graph);
//     cg->print_graph_info();
//     cg->remove_node(2, graph);
//     cg->print_graph_info();
//     ASSERT_EQ(0, boost::num_edges(graph));
//     ASSERT_EQ(0, boost::num_vertices(graph));
// }

// this test will be deprecated after the graph class is implemented
// since all func in basegraph will be setted to protected
// int main() {
//     if(0){
//         std::cout << "##### deprecated test #####" << std::endl;
//         return 0;
//     }

//     std::cout << "\n##### test basic func #####" << std::endl;
//     std::vector<NNkernel> kernels = { { {3,3}, {256,384}, 1 }, { {3,3}, {384,384}, 2 } };
//     std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels);
//     cg->analysis();
//     auto& graph = cg->get_graph();
//     // print cg info
//     std::cout << "test get graph attributes:" << std::endl;
//     std::cout << "cg size: " << boost::num_vertices(graph) << std::endl;
//     std::cout << "cg edge size: " << boost::num_edges(graph) << std::endl;
    
//     cg->add_edge(0, 1,  CEdge{DepType::Accum, 100}, graph);
//     std::cout << "cg edge size: " << boost::num_edges(graph) << std::endl;
//     auto adj_nodes = cg->get_adjacent_nodes(0, graph);
//     std::cout << "adj_nodes size: " << adj_nodes.size() << std::endl;
//     // auto node = graph.m_vertices[0];
//     // auto test = node.m_property.m_value;
//     // std::cout << "node: " << test << std::endl;
//     // auto node1 = cg->get_node_property(1, graph);
//     const auto& node1 = cg->get_node_property(1, graph);
//     std::cout << "node1: " << node1 << std::endl;
//     cg->set_node_property(1, CNode{1000, {30,30}, 2, {384,384}, {384,384}}, graph);
//     std::cout << "node1 after: " << cg->get_node_property(1, graph) << std::endl;
//     const auto& edge = cg->get_edge_property(0, 1, graph);
//     std::cout << "edge: " << edge << std::endl;
//     cg->set_edge_property(0, 1, CEdge{DepType::Prop, 200}, graph);
//     std::cout << "edge after: " << cg->get_edge_property(0, 1, graph) << std::endl;
//     auto adj_edges = cg->get_adjacent_edges(0, graph);
//     std::cout << "adj_edges size: " << adj_edges.size() << std::endl;
//     cg->remove_edge(0, 1, graph);
//     std::cout << "cg edge size: " << boost::num_edges(graph) << std::endl;
//     const auto& edge1 = cg->get_edge_property(0, 1, graph);
//     std::cout << "edge1: " << edge1 << std::endl;
//     cg->add_node(CNode{0, {3,3}, 1, {256,384}, {256,384}}, graph);
//     std::cout << "cg size: " << boost::num_vertices(graph) << std::endl;
//     cg->remove_node(0, graph);
//     std::cout << "cg size: " << boost::num_vertices(graph) << std::endl;
//     std::cout << "print graph info:" << std::endl;
//     cg->print_graph_info();
//     std::cout << "##### test basic func end #####\n" << std::endl;
//     return 0;
// }
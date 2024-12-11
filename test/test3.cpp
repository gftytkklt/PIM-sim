#include "graph.h"
#include <gtest/gtest.h>

// test T-VDFG generation
class GraphTest : public ::testing::Test {
protected:
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{224, 224},{112, 112}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{112, 112},{56, 56}},
    {2, {3,3}, {384,256}, std::vector<Depinfo>{{-1,std::make_pair(0,0)}},{56, 56},{28, 28}} 
    };
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256));
    decltype(cg->get_graph()) graph = cg->get_graph();
    TGraph tg = TGraph(cg, 3);
    // void SetUp() override {
    //     graph = cg->get_graph();
    // }
};

TEST_F(GraphTest, TESTTG){
    cg->print_graph_info();
    tg.print_graph_info();
    // std::cout << tg.get_graph().m_vertices.size() << std::endl;
}
#include "graph.h"
#include <gtest/gtest.h>

// test HCG generation
class GraphTest : public ::testing::Test {
protected:
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{8, 8},{4, 4}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{4, 4},{2, 2}},
    {2, {3,3}, {384,256}, std::vector<Depinfo>{{-1,std::make_pair(0,0)}},{2, 2},{1, 1}} 
    };
    std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256));
    decltype(cg->get_graph()) graph = cg->get_graph();
    TGraph tg = TGraph(cg, 3);
    // void SetUp() override {
    //     graph = cg->get_graph();
    // }
};

TEST_F(GraphTest, HGRAPH){ 
    auto hg = HGraph(std::make_shared<TGraph>(tg), cg);
    hg.print_graph_info();
    // hg.debug();
}
    

// TEST_F(GraphTest, NEEDTILE){
//     tg = TGraph(cg, 2);
//     // tg.print_graph_info();
//     // auto hg = HGraph(std::make_shared<TGraph>(tg), cg, std::make_pair(2, 2));
//     ASSERT_THROW(auto hg = HGraph(std::make_shared<TGraph>(tg), cg, std::make_pair(2, 2)), std::invalid_argument);
//     // tg.debug();
//     auto hg = HGraph(std::make_shared<TGraph>(tg), cg);
//     hg.print_graph_info();
// }
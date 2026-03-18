#include "graph.h"
#include "strategy/StrategyBase.h"
#include <gtest/gtest.h>

// test HCG generation
class GraphTest : public ::testing::Test {
protected:
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{8, 8},{4, 4}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{4, 4},{2, 2}},
    {2, {3,3}, {384,256}, std::vector<Depinfo>{{-1,std::make_pair(0,0)}},{2, 2},{1, 1}} 
    };
    // std::shared_ptr<CGraph> cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256));
    // decltype(cg->get_graph()) graph = cg->get_graph();
    // TGraph tg = TGraph(cg, 3);
    // // void SetUp() override {
    // //     graph = cg->get_graph();
    // // }
};

// // TEST_F(GraphTest, HGRAPH){ 
// //     auto hg = HGraph(std::make_shared<TGraph>(tg), cg);
// //     hg.print_graph_info();
// //     // hg.debug();
// // }
    

// TEST_F(GraphTest, DGSEG){
//     tg = TGraph(cg, 2);
//     // tg.print_graph_info();
//     // auto hg = HGraph(std::make_shared<TGraph>(tg), cg, std::make_pair(2, 2));
//     // ASSERT_THROW(auto hg = HGraph(std::make_shared<TGraph>(tg), cg, std::make_pair(2, 2)), std::invalid_argument);
//     // tg.debug();
//     auto hg = HGraph(std::make_shared<TGraph>(tg), cg);
//     auto dg = DGraph(std::make_shared<HGraph>(hg), std::make_shared<TGraph>(tg), cg);
//     // dg.print_graph_info();

//     // hg.print_graph_info();
// }


TEST_F(GraphTest, CGraphConstruction) {
    auto strategy = createStrategy<CGraph>(OptType::PIMAPPING);
    auto cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256), strategy);
    ASSERT_TRUE(cg != nullptr);
    ASSERT_GT(cg->get_cdep().size(), 0);
}

TEST_F(GraphTest, TGraphConstruction) {
    auto cg_strategy = createStrategy<CGraph>(OptType::PIMAPPING);
    auto cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256), cg_strategy);
    
    auto tg_strategy = createStrategy<TGraph>(OptType::PIMAPPING);
    auto tg = std::make_shared<TGraph>(cg, 2, tg_strategy);
    
    ASSERT_TRUE(tg != nullptr);
    ASSERT_GT(tg->get_tdep().size(), 0);
}

TEST_F(GraphTest, HGraphConstruction) {
    auto cg_strategy = createStrategy<CGraph>(OptType::PIMAPPING);
    auto cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256), cg_strategy);
    
    auto tg_strategy = createStrategy<TGraph>(OptType::PIMAPPING);
    auto tg = std::make_shared<TGraph>(cg, 2, tg_strategy);
    
    auto hg_strategy = createStrategy<HGraph>(OptType::PIMAPPING);
    auto hg = std::make_shared<HGraph>(tg, cg, std::make_pair(3, 3), hg_strategy);
    
    ASSERT_TRUE(hg != nullptr);
}

TEST_F(GraphTest, DGraphConstruction) {
    auto cg_strategy = createStrategy<CGraph>(OptType::PIMAPPING);
    auto cg = std::make_shared<CGraph>(kernels, std::make_pair(1152, 256), cg_strategy);
    
    auto tg_strategy = createStrategy<TGraph>(OptType::PIMAPPING);
    auto tg = std::make_shared<TGraph>(cg, 2, tg_strategy);
    
    auto hg_strategy = createStrategy<HGraph>(OptType::PIMAPPING);
    auto hg = std::make_shared<HGraph>(tg, cg, std::make_pair(3, 3), hg_strategy);
    
    auto dg_strategy = createStrategy<DGraph>(OptType::PIMAPPING);
    auto dg = std::make_shared<DGraph>(hg, tg, cg, 1, dg_strategy);
    
    ASSERT_TRUE(dg != nullptr);
    ASSERT_GT(dg->get_path_segs().size(), 0);
}

TEST_F(GraphTest, DifferentStrategies) {
    
    auto cg_mnsim = std::make_shared<CGraph>(
        kernels, std::make_pair(1152, 256), createStrategy<CGraph>(OptType::MNSIM));
    auto tg_spatem = std::make_shared<TGraph>(
        cg_mnsim, 2, createStrategy<TGraph>(OptType::SPATEM));
    
    ASSERT_TRUE(cg_mnsim != nullptr);
    ASSERT_TRUE(tg_spatem != nullptr);
}
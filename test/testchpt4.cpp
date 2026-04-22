#include "analyzer.h"
#include <gtest/gtest.h>

// test analyzer
class AnalyzerTest : public ::testing::Test {
protected:
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{8, 8},{4, 4}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{4, 4},{2, 2}},
    {2, {3,3}, {384,256}, std::vector<Depinfo>{{-1,std::make_pair(0,0)}},{2, 2},{1, 1}} 
    };
    HWInfo info = {{1152, 256}, 4, {2, 2}, 1};
    OptInfo opt = {true, true};
    void SetUp() override {
        // code here will execute just before the test ensues
        opt = {true, true};
    }
};

TEST_F(AnalyzerTest, PUMA) {
    // test analyzer
    std::cout << "PUMA" << std::endl;
    // Analyzer analyzer = Analyzer(kernels, info, opt, true);
    Analyzer analyzer = Analyzer(kernels, info, OptType::PUMA);
    analyzer.generate_analysis_result();
    analyzer.generate_comm_info();
    analyzer.print_result();
}

TEST_F(AnalyzerTest, TILE2_0_V2) {
    // test analyzer
    std::cout << "TILE2_0_V2" << std::endl;
    // Analyzer analyzer = Analyzer(kernels, info, opt, true);
    Analyzer analyzer = Analyzer(kernels, info, OptType::TILE2_0_V2);
    analyzer.generate_analysis_result();
    analyzer.generate_comm_info();
    analyzer.print_result();
}

TEST_F(AnalyzerTest, REHARVEST) {
    // test analyzer
    std::cout << "REHARVEST" << std::endl;
    // Analyzer analyzer = Analyzer(kernels, info, opt, true);
    Analyzer analyzer = Analyzer(kernels, info, OptType::REHARVEST);
    analyzer.generate_analysis_result();
    analyzer.generate_comm_info();
    analyzer.print_result();
}

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
    HWInfo info = {{1152, 256}, 2, {3, 3}, 1};
    OptInfo opt = {true, true};
    void SetUp() override {
        // code here will execute just before the test ensues
        opt = {true, true};
    }
};

TEST_F(AnalyzerTest, OptAll) {
    // test analyzer
    std::cout << "OptAll" << std::endl;
    Analyzer analyzer = Analyzer(kernels, info, opt);
    analyzer.generate_analysis_result();
    analyzer.print_result();
}

// TEST_F(AnalyzerTest, OptSchedule) {
//     // test analyzer
//     std::cout << "OptSchedule" << std::endl;
//     opt.mapping_opt = false;
//     Analyzer analyzer = Analyzer(kernels, info, opt);
//     analyzer.generate_analysis_result();
//     analyzer.print_result();
// }

// TEST_F(AnalyzerTest, OptMapping) {
//     // test analyzer
//     std::cout << "OptMapping" << std::endl;
//     opt.sched_opt = false;
//     Analyzer analyzer = Analyzer(kernels, info, opt);
//     analyzer.generate_analysis_result();
//     analyzer.print_result();
// }

// TEST_F(AnalyzerTest, OptNone) {
//     // test analyzer
//     std::cout << "OptNone" << std::endl;
//     opt = {false, false};
//     Analyzer analyzer = Analyzer(kernels, info, opt);
//     analyzer.generate_analysis_result();
//     analyzer.print_result();
// }
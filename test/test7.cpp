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
    
};

TEST_F(AnalyzerTest, AnalyzerTest) {
    // test analyzer
    Analyzer analyzer = Analyzer(kernels, info);
}
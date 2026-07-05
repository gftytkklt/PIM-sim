#include "simulator/tile2_0/tiling.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

class TilingSimulatorTest: public ::testing::Test {
protected:
    void SetUp() override {
        simulator = std::make_shared<TilingSimulator>(300000);
        simulator->Init();
        simulator->init_task(); // 模拟L1C已有部分数据，触发TS
    }
    void TearDown() override {
        simulator.reset();
    }
    std::shared_ptr<TilingSimulator> simulator;
};

TEST_F(TilingSimulatorTest, BasicSimulation) {
    simulator->run();
}
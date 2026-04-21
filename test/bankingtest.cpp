#include "simulator/tile2_0/membanking.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

class BankingSimulatorTest : public ::testing::Test {
protected:    
    void SetUp() override {
        simulator = std::make_shared<BankingSimulator>(200000);
        simulator->Init();
        simulator->init_task(0, 0, 8); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
    }

    void TearDown() override {
        simulator.reset();
    }

    std::shared_ptr<BankingSimulator> simulator;
};

TEST_F(BankingSimulatorTest, BasicSimulation) {
    simulator->run();
}
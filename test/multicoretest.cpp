#include "simulator/tile2_0/multicore.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

class MulticoreSimulatorTest : public ::testing::Test {
protected:    
    void SetUp() override {
        simulator = std::make_shared<MulticoreSimulator>(200000);
        simulator->Init();
        simulator->init_task(0, 0, 4); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
        simulator->init_task(1, 0, 4); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
        simulator->init_task(2, 0, 4); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
        simulator->init_task(3, 0, 4); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。

    }

    void TearDown() override {
        simulator.reset();
    }
    std::shared_ptr<MulticoreSimulator> simulator;
};

TEST_F(MulticoreSimulatorTest, BasicSimulation) {
    simulator->run();
    simulator->dump_completed_events("multicore_completed_events.csv");
}
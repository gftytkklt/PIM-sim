#include "simulator/tile2_0/tile2_0.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

void handle_simd_computation_done(const GenericMessage& msg) {
    int value = std::any_cast<int>(msg.body);
    std::cout << "SIMD computation done, value: " << value 
            << ", delay: " << msg.delay_cycles << " cycles" << std::endl;
}

class OPUTileSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        simulator = std::make_shared<OPUSimulator>(200000);
        simulator->Init();
        simulator->init_task(0, 4); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
    }

    void TearDown() override {
        simulator.reset();
    }

    std::shared_ptr<OPUSimulator> simulator;
};

TEST_F(OPUTileSimulatorTest, BasicSimulation) {
    simulator->run();
}
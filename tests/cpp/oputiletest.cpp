#include "runtime_paths.h"
#include "simulator/tile2_0/tile2_0.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

class OPUTileSimulatorTest : public ::testing::Test {
protected:
    void SetUp() override {
        simulator = std::make_shared<OPUSimulator>(200000);
        simulator->Init();
        simulator->init_task(0, 6); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
    }

    void TearDown() override {
        simulator.reset();
    }

    std::shared_ptr<OPUSimulator> simulator;
};

// 这个测试用来根据fmap参数输出计算延迟。
TEST_F(OPUTileSimulatorTest, BasicSimulation) {
    simulator->set_completion_condition([this]() {
        return simulator->get_module<TaskScheduler>("task_scheduler")->completed_batches() == 6;
    });
    simulator->run();
    ASSERT_EQ(simulator->get_stop_reason(), CycleAccurateSimulator::StopReason::Completed);
    ASSERT_TRUE(simulator->is_simulation_done());
    const auto& batches = simulator->get_module<TaskScheduler>("task_scheduler")->get_completed_processes();
    const auto& outputs = simulator->get_module<SIMD>("simd")->get_completed_processes();
    ASSERT_EQ(batches.size(), 6u);
    ASSERT_EQ(outputs.size(), 72u); // 6 batches * two columns * six rows.
    for (size_t batch = 0; batch < batches.size(); ++batch) {
        EXPECT_GE(batches[batch]->get_end_time(), outputs[(batch + 1) * 12 - 1]->get_finish_time());
    }
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler")->completed_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler")->admitted_batches(), 6u);

    simulator->dump_completed_events(pim::runtime_paths::run_file("completed_events.csv"));
}

#include "runtime_paths.h"
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
    simulator->set_completion_condition([this]() {
        return simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches() == 8 &&
            simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches() == 6 &&
            simulator->get_module<TaskScheduler>("task_scheduler2")->completed_batches() == 6 &&
            simulator->get_module<TaskScheduler>("task_scheduler3")->completed_batches() == 6 &&
            simulator->get_module<TaskScheduler>("task_scheduler4")->completed_batches() == 4 &&
            simulator->get_module<TaskScheduler>("task_scheduler5")->completed_batches() == 4;
    });
    simulator->run();
    ASSERT_EQ(simulator->get_stop_reason(), CycleAccurateSimulator::StopReason::Completed);
    ASSERT_TRUE(simulator->is_simulation_done());
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches(), 8u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->admitted_batches(), 8u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->admitted_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler2")->completed_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler2")->admitted_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler3")->completed_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler3")->admitted_batches(), 6u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler4")->completed_batches(), 4u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler4")->admitted_batches(), 4u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler5")->completed_batches(), 4u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler5")->admitted_batches(), 4u);

    simulator->dump_completed_events(pim::runtime_paths::run_file("multicore_completed_events.csv"));
}

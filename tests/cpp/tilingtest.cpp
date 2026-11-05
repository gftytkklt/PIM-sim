#include "runtime_paths.h"
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
    simulator->set_completion_condition([this]() {
        return simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches() == 96 &&
            simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches() == 96;
    });
    simulator->run();
    simulator->dump_completed_events(pim::runtime_paths::run_file("tiling_completed_events.csv"));
    ASSERT_EQ(simulator->get_stop_reason(), CycleAccurateSimulator::StopReason::Completed);
    ASSERT_TRUE(simulator->is_simulation_done());
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches(), 96u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->admitted_batches(), 96u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches(), 96u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->admitted_batches(), 96u);

}

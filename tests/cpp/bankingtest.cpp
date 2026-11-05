#include "runtime_paths.h"
#include "simulator/tile2_0/membanking.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include <gtest/gtest.h>
#include <iostream>
#include <tuple>
#include <vector>

/*
  测试参数：
  对XY和YX两种策略，init_task传入(0,0,16)，task_list0是{4, 8, 8, 128, true}
  对Custom策略，init_task传入(0,0,8)，task_list0是{4, 16, 4, 128, true}
*/
class BankingSimulatorTest : public ::testing::Test {
protected:    
    void SetUp() override {
        simulator = std::make_shared<BankingSimulator>(200000);
        simulator->Init();
        simulator->init_task(0, 0, 16); // 模拟L1C已有部分数据，触发TS内部的任务队列初始化。
    }

    void TearDown() override {
        simulator.reset();
    }

    std::shared_ptr<BankingSimulator> simulator;
};

TEST_F(BankingSimulatorTest, BasicSimulation) {
    simulator->set_completion_condition([this]() {
        return simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches() == 16 &&
            simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches() == 4 &&
            simulator->get_module<TaskScheduler>("task_scheduler2")->completed_batches() == 2;
    });
    simulator->run();
    ASSERT_EQ(simulator->get_stop_reason(), CycleAccurateSimulator::StopReason::Completed);
    ASSERT_TRUE(simulator->is_simulation_done());
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->completed_batches(), 16u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler0")->admitted_batches(), 16u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->completed_batches(), 4u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler1")->admitted_batches(), 4u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler2")->completed_batches(), 2u);
    EXPECT_EQ(simulator->get_module<TaskScheduler>("task_scheduler2")->admitted_batches(), 2u);

    std::string block_strategy_str;
    switch (simulator->get_block_strategy()) {
        case BankingSimulator::BlockStrategy::XY:
            block_strategy_str = "XY";
            break;
        case BankingSimulator::BlockStrategy::YX:
            block_strategy_str = "YX";
            break;
        case BankingSimulator::BlockStrategy::Custom:
            block_strategy_str = "16x4";
            break;
        default:
            block_strategy_str = "Unknown";
            break;
    }
    simulator->dump_completed_events(pim::runtime_paths::run_file("banking_completed_events_" + block_strategy_str + ".csv"));
    // simulator->dump_completed_events("banking_completed_events_YX.csv");
    // simulator->dump_completed_events("banking_completed_events_16x4.csv");
}
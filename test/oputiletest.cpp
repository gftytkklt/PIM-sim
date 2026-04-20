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
        // 创建模拟器实例
        const std::array<FmapTask, L1C_BANK> task_list = {{
            {1, 8, 8, 128, true}, // Bank 0
            {0, 0, 0, 0, 0}, // Bank 1
            {0, 0, 0, 0, 0}, // Bank 2
            {0, 0, 0, 0, 0}  // Bank 3
        }};
        simulator = std::make_shared<CycleAccurateSimulator>(200000);
        simulator->register_module<SIMD>("simd", 4);
        simulator->register_module<Crossbar>("crossbar", 3);
        simulator->register_module<TaskScheduler>("task_scheduler", 2, task_list);
        simulator->register_module<L1C>("L1_cache", 1);
        // demo，注册一个消息处理器来接收SIMD计算完成的消息，可以利用这个机制更新任务调度器的状态或者触发后续的任务。
        // simulator->register_task_handler("SIMD_computation_done", [this](const GenericMessage& msg) {
        //     // 处理SIMD计算完成的消息
        //     std::cout << "Received SIMD computation done message with value: " 
        //               << std::any_cast<int>(msg.body) << std::endl;
        // });
        // 第二种用法
        simulator->register_task_handler("SIMD_computation_done", handle_simd_computation_done);
        simulator->send_message_to_core("task_scheduler", GenericMessage("SIMD_computation_done", 441, 0)); // 发送测试消息
        simulator->send_message_to_core("task_scheduler", "xxx", 442); // 发送测试消息
    }

    void TearDown() override {
        simulator.reset();
    }

    std::shared_ptr<CycleAccurateSimulator> simulator;
};

TEST_F(OPUTileSimulatorTest, BasicSimulation) {
    // TS to L1C
    simulator->connect_modules("task_scheduler", "cache_read_trigger", "L1_cache", "cache_read_trigger");
    simulator->connect_modules("task_scheduler", "cache_read_len", "L1_cache", "cache_read_len");
    simulator->connect_modules("task_scheduler", "cache_write_trigger", "L1_cache", "cache_write_trigger");
    simulator->connect_modules("task_scheduler", "cache_write_len", "L1_cache", "cache_write_len");
    simulator->connect_modules("L1_cache", "cache_read_done", "task_scheduler", "cache_read_valid");
    simulator->connect_modules("L1_cache", "cache_write_done", "task_scheduler", "cache_write_done");
    // TS to XBAR
    simulator->connect_modules("task_scheduler", "xbar_computation_trigger", "crossbar", "computation_trigger");
    simulator->connect_modules("task_scheduler", "xbar_switching_trigger", "crossbar", "switching_trigger");
    simulator->connect_modules("crossbar", "switching_done", "task_scheduler", "xbar_switching_done");
    // TS to SIMD
    simulator->connect_modules("task_scheduler", "pooling_enabled", "simd", "SIMD_pooling_enable");
    simulator->connect_modules("simd", "SIMD_data_valid", "task_scheduler", "SIMD_computation_done");
    // XBAR to SIMD
    simulator->connect_modules("crossbar", "computation_done", "simd", "SIMD_channel_batch");
    // 运行模拟
    simulator->run();
    
}
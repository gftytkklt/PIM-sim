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
        // 创建模拟器实例
        const std::array<FmapTask, L1C_BANK> task_list = {{
            {1, 1, 4, 128, true}, // Bank 0
            {0, 0, 0, 0, 0}, // Bank 1
            {0, 0, 0, 0, 0}, // Bank 2
            {0, 0, 0, 0, 0}  // Bank 3
        }};
        simulator = std::make_shared<CycleAccurateSimulator>(2000);
        simulator->register_module<SIMD>("simd", 4);
        simulator->register_module<Crossbar>("crossbar", 3);
        simulator->register_module<TaskScheduler>("task_scheduler", 2, task_list);
        simulator->register_module<L1C>("L1_cache", 1);
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
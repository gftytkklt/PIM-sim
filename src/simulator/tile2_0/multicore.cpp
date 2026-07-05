#include "simulator/tile2_0/multicore.h"

void MulticoreSimulator::Init() {
    // core 0
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {1, 8, 8, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {1, 4, 4, 128, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd0", 4);
    register_module<Crossbar>("crossbar0", 3);
    register_module<TaskScheduler>("task_scheduler0", 2, task_list0);
    register_module<L1C>("L1_cache0", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler0", "cache_read_trigger", "L1_cache0", "cache_read_trigger");
    connect_modules("task_scheduler0", "cache_read_len", "L1_cache0", "cache_read_len");
    connect_modules("task_scheduler0", "cache_write_trigger", "L1_cache0", "cache_write_trigger");
    connect_modules("task_scheduler0", "cache_write_len", "L1_cache0", "cache_write_len");
    connect_modules("L1_cache0", "cache_read_done", "task_scheduler0", "cache_read_valid");
    connect_modules("L1_cache0", "cache_write_done", "task_scheduler0", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler0", "xbar_computation_trigger", "crossbar0", "computation_trigger");
    connect_modules("task_scheduler0", "xbar_switching_trigger", "crossbar0", "switching_trigger");
    connect_modules("crossbar0", "switching_done", "task_scheduler0", "xbar_switching_done");
    connect_modules("crossbar0", "computation_done", "task_scheduler0", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler0", "pooling_enabled", "simd0", "SIMD_pooling_enable");
    connect_modules("simd0", "SIMD_data_valid", "task_scheduler0", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar0", "computation_done", "simd0", "SIMD_channel_batch");
    // core 1
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, 8, 8, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {0, 0, 0, 0, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd1", 4);
    register_module<Crossbar>("crossbar1", 3);
    register_module<TaskScheduler>("task_scheduler1", 2, task_list1);
    register_module<L1C>("L1_cache1", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler1", "cache_read_trigger", "L1_cache1", "cache_read_trigger");
    connect_modules("task_scheduler1", "cache_read_len", "L1_cache1", "cache_read_len");
    connect_modules("task_scheduler1", "cache_write_trigger", "L1_cache1", "cache_write_trigger");
    connect_modules("task_scheduler1", "cache_write_len", "L1_cache1", "cache_write_len");
    connect_modules("L1_cache1", "cache_read_done", "task_scheduler1", "cache_read_valid");
    connect_modules("L1_cache1", "cache_write_done", "task_scheduler1", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler1", "xbar_computation_trigger", "crossbar1", "computation_trigger");
    connect_modules("task_scheduler1", "xbar_switching_trigger", "crossbar1", "switching_trigger");
    connect_modules("crossbar1", "switching_done", "task_scheduler1", "xbar_switching_done");
    connect_modules("crossbar1", "computation_done", "task_scheduler1", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler1", "pooling_enabled", "simd1", "SIMD_pooling_enable");
    connect_modules("simd1", "SIMD_data_valid", "task_scheduler1", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar1", "computation_done", "simd1", "SIMD_channel_batch");
    // core 2
    const std::array<FmapTask, L1C_BANK> task_list2 = {{
            {1, 8, 8, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {0, 0, 0, 0, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd2", 4);
    register_module<Crossbar>("crossbar2", 3);
    register_module<TaskScheduler>("task_scheduler2", 2, task_list2);
    register_module<L1C>("L1_cache2", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler2", "cache_read_trigger", "L1_cache2", "cache_read_trigger");
    connect_modules("task_scheduler2", "cache_read_len", "L1_cache2", "cache_read_len");
    connect_modules("task_scheduler2", "cache_write_trigger", "L1_cache2", "cache_write_trigger");
    connect_modules("task_scheduler2", "cache_write_len", "L1_cache2", "cache_write_len");
    connect_modules("L1_cache2", "cache_read_done", "task_scheduler2", "cache_read_valid");
    connect_modules("L1_cache2", "cache_write_done", "task_scheduler2", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler2", "xbar_computation_trigger", "crossbar2", "computation_trigger");
    connect_modules("task_scheduler2", "xbar_switching_trigger", "crossbar2", "switching_trigger");
    connect_modules("crossbar2", "switching_done", "task_scheduler2", "xbar_switching_done");
    connect_modules("crossbar2", "computation_done", "task_scheduler2", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler2", "pooling_enabled", "simd2", "SIMD_pooling_enable");
    connect_modules("simd2", "SIMD_data_valid", "task_scheduler2", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar2", "computation_done", "simd2", "SIMD_channel_batch");
    // core 3
    const std::array<FmapTask, L1C_BANK> task_list3 = {{
            {1, 8, 8, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {0, 0, 0, 0, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd3", 4);
    register_module<Crossbar>("crossbar3", 3);
    register_module<TaskScheduler>("task_scheduler3", 2, task_list3);
    register_module<L1C>("L1_cache3", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler3", "cache_read_trigger", "L1_cache3", "cache_read_trigger");
    connect_modules("task_scheduler3", "cache_read_len", "L1_cache3", "cache_read_len");
    connect_modules("task_scheduler3", "cache_write_trigger", "L1_cache3", "cache_write_trigger");
    connect_modules("task_scheduler3", "cache_write_len", "L1_cache3", "cache_write_len");
    connect_modules("L1_cache3", "cache_read_done", "task_scheduler3", "cache_read_valid");
    connect_modules("L1_cache3", "cache_write_done", "task_scheduler3", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler3", "xbar_computation_trigger", "crossbar3", "computation_trigger");
    connect_modules("task_scheduler3", "xbar_switching_trigger", "crossbar3", "switching_trigger");
    connect_modules("crossbar3", "switching_done", "task_scheduler3", "xbar_switching_done");
    connect_modules("crossbar3", "computation_done", "task_scheduler3", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler3", "pooling_enabled", "simd3", "SIMD_pooling_enable");
    connect_modules("simd3", "SIMD_data_valid", "task_scheduler3", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar3", "computation_done", "simd3", "SIMD_channel_batch");
    // core 4
    const std::array<FmapTask, L1C_BANK> task_list4 = {{
            {1, 4, 4, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {0, 0, 0, 0, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd4", 4);
    register_module<Crossbar>("crossbar4", 3);
    register_module<TaskScheduler>("task_scheduler4", 2, task_list4);
    register_module<L1C>("L1_cache4", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler4", "cache_read_trigger", "L1_cache4", "cache_read_trigger");
    connect_modules("task_scheduler4", "cache_read_len", "L1_cache4", "cache_read_len");
    connect_modules("task_scheduler4", "cache_write_trigger", "L1_cache4", "cache_write_trigger");
    connect_modules("task_scheduler4", "cache_write_len", "L1_cache4", "cache_write_len");
    connect_modules("L1_cache4", "cache_read_done", "task_scheduler4", "cache_read_valid");
    connect_modules("L1_cache4", "cache_write_done", "task_scheduler4", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler4", "xbar_computation_trigger", "crossbar4", "computation_trigger");
    connect_modules("task_scheduler4", "xbar_switching_trigger", "crossbar4", "switching_trigger");
    connect_modules("crossbar4", "switching_done", "task_scheduler4", "xbar_switching_done");
    connect_modules("crossbar4", "computation_done", "task_scheduler4", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler4", "pooling_enabled", "simd4", "SIMD_pooling_enable");
    connect_modules("simd4", "SIMD_data_valid", "task_scheduler4", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar4", "computation_done", "simd4", "SIMD_channel_batch");
    // core 5
    const std::array<FmapTask, L1C_BANK> task_list5 = {{
            {1, 4, 4, 128, true}, // Bank 0
            {1, 4, 4, 128, false}, // Bank 1
            {0, 0, 0, 0, false}, // Bank 2
            {0, 0, 0, 0, false}  // Bank 3
        }};
    register_module<SIMD>("simd5", 4);
    register_module<Crossbar>("crossbar5", 3);
    register_module<TaskScheduler>("task_scheduler5", 2, task_list5);
    register_module<L1C>("L1_cache5", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler5", "cache_read_trigger", "L1_cache5", "cache_read_trigger");
    connect_modules("task_scheduler5", "cache_read_len", "L1_cache5", "cache_read_len");
    connect_modules("task_scheduler5", "cache_write_trigger", "L1_cache5", "cache_write_trigger");
    connect_modules("task_scheduler5", "cache_write_len", "L1_cache5", "cache_write_len");
    connect_modules("L1_cache5", "cache_read_done", "task_scheduler5", "cache_read_valid");
    connect_modules("L1_cache5", "cache_write_done", "task_scheduler5", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler5", "xbar_computation_trigger", "crossbar5", "computation_trigger");
    connect_modules("task_scheduler5", "xbar_switching_trigger", "crossbar5", "switching_trigger");
    connect_modules("crossbar5", "switching_done", "task_scheduler5", "xbar_switching_done");
    connect_modules("crossbar5", "computation_done", "task_scheduler5", "xbar_computation_done");
    // TS to SIMD
    connect_modules("task_scheduler5", "pooling_enabled", "simd5", "SIMD_pooling_enable");
    connect_modules("simd5", "SIMD_data_valid", "task_scheduler5", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar5", "computation_done", "simd5", "SIMD_channel_batch");
    
    // message handler
    register_task_handler("task_batch_done",
            [this](const GenericMessage& msg) {
                this->handle_batch_task_done(msg);
            });

    // init batch num map
    // core_batch_num_map_["task_scheduler0"] = 0;
    // core_batch_num_map_["task_scheduler1"] = 0;
    // core_batch_num_map_["task_scheduler2"] = 0;
    // core_batch_num_map_["task_scheduler3"] = 0;
    // core_batch_num_map_["task_scheduler4"] = 0;
    // core_batch_num_map_["task_scheduler5"] = 0;
    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < L1C_BANK; ++j) {
            core_batch_num_map_[{"task_scheduler" + std::to_string(i), j}] = 0;
        }
    }
}

void MulticoreSimulator::handle_batch_task_done(const GenericMessage& msg) {
    // 处理任务完成的消息，可以根据需要更新模拟器状态或者触发其他事件
    auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
    int bank_id = std::get<0>(data);
    std::string core_name = std::get<1>(data);
    // 写死：core0和core1的bank0算完可以出发core4，core2和core3的bank0算完可以触发core5
    core_batch_num_map_[{core_name, bank_id}]++;
    // std::cout << "Received task batch done message for bank " << bank_id 
    //           << " with core: " << core_name << std::endl;
    std::cout << "Current batch num for " << core_name << " bank " << bank_id 
              << ": " << core_batch_num_map_[{core_name, bank_id}] << std::endl;
    if (core_name == "task_scheduler0") {
        switch (bank_id) {
            // 与core1的bank0共同触发core4和core5的bank0，core1和core2的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler0", 0}] == 4 && 
                    core_batch_num_map_[{"task_scheduler1", 0}] == 4) {
                    // core0和core1的bank0都完成了，可以触发core4
                    init_task(4, 0, 2);
                    init_task(5, 0, 2);
                    init_task(1, 1, 2);
                    init_task(2, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler0", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler1", 0}] = 0;
                }
                break;
            // 和core4和core5的bank0共同触发core4和core5的bank1
            case 1:
                if (core_batch_num_map_[{"task_scheduler0", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler4", 0}] == 2 && 
                    core_batch_num_map_[{"task_scheduler5", 0}] == 2) {
                    // core0和core1的bank1都完成了，可以触发core4
                    init_task(4, 1, 2);
                    init_task(5, 1, 2);
                    // 清空core0和core1的bank1的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler0", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler4", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler5", 0}] = 0;
                }
                break;
            default:
                break;
        }
    }
    else if (core_name == "task_scheduler1") {
        switch (bank_id) {
            // 与core0的bank0共同触发core4和core5的bank0，core1和core2的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler0", 0}] == 4 && 
                    core_batch_num_map_[{"task_scheduler1", 0}] == 4) {
                    // core0和core1的bank0都完成了，可以触发core4
                    init_task(4, 0, 2);
                    init_task(5, 0, 2);
                    init_task(1, 1, 2);
                    init_task(2, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler0", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler1", 0}] = 0;
                }
                break;
            // 和core2和core3的bank1共同触发core0的bank2
            case 1:
                if (core_batch_num_map_[{"task_scheduler1", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler2", 1}] == 2 && 
                    core_batch_num_map_[{"task_scheduler3", 1}] == 2) {
                    // core0和core1的bank1都完成了，可以触发core4
                    init_task(0, 2, 2);
                    // 清空core0和core1的bank1的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler1", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler2", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler3", 1}] = 0;
                }
                break;
            default:
                break;
        }
    }
    else if (core_name == "task_scheduler2") {
        switch (bank_id) {
            // 和core3的bank0共同触发core0的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler2", 0}] == 4 && 
                    core_batch_num_map_[{"task_scheduler3", 0}] == 4) {
                    init_task(0, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler2", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler3", 0}] = 0;
                }
                break;
            // 和core1和core3的bank1共同触发core0的bank2
            case 1:
                if (core_batch_num_map_[{"task_scheduler1", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler2", 1}] == 2 && 
                    core_batch_num_map_[{"task_scheduler3", 1}] == 2) {
                    // core0和core1的bank1都完成了，可以触发core4
                    init_task(0, 2, 2);
                    // 清空core0和core1的bank1的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler1", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler2", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler3", 1}] = 0;
                }
                break;
            default:
                break;
        }
    }
    else if (core_name == "task_scheduler3") {
        switch (bank_id) {
            // 和core2的bank0共同触发core0的bank1,core3的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler2", 0}] == 4 && 
                    core_batch_num_map_[{"task_scheduler3", 0}] == 4) {
                    init_task(0, 1, 2);
                    init_task(3, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler2", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler3", 0}] = 0;
                }
                break;
            // 和core1和core2的bank1共同触发core0的bank2
            case 1:
                if (core_batch_num_map_[{"task_scheduler1", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler2", 1}] == 2 && 
                    core_batch_num_map_[{"task_scheduler3", 1}] == 2) {
                    // core0和core1的bank1都完成了，可以触发core4
                    init_task(0, 2, 2);
                    // 清空core0和core1的bank1的batch num，准备下一轮
                    core_batch_num_map_[{"task_scheduler1", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler2", 1}] = 0;
                    core_batch_num_map_[{"task_scheduler3", 1}] = 0;
                }
                break;
            default:
                break;
        }
    }
    else if (core_name == "task_scheduler4") {
        switch (bank_id) {
            // 和core5的bank0，core0的bank1共同触发core4的bank1和core5的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler0", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler4", 0}] == 2 && 
                    core_batch_num_map_[{"task_scheduler5", 0}] == 2) {
                    init_task(4, 1, 2);
                    init_task(5, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮
                        
                    core_batch_num_map_[{"task_scheduler4", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler5", 0}] = 0;
                }
                break;
            default:
                break;
        }
    }
    else if (core_name == "task_scheduler5") {
        switch (bank_id) {
            // 和core4的bank0,core0的bank1共同触发core4的bank1和core5的bank1
            case 0:
                if (core_batch_num_map_[{"task_scheduler0", 1}] == 2 &&
                    core_batch_num_map_[{"task_scheduler4", 0}] == 2 && 
                    core_batch_num_map_[{"task_scheduler5", 0}] == 2) {
                    init_task(4, 1, 2);
                    init_task(5, 1, 2);
                    // 清空core0和core1的bank0的batch num，准备下一轮

                    core_batch_num_map_[{"task_scheduler4", 0}] = 0;
                    core_batch_num_map_[{"task_scheduler5", 0}] = 0;
                }
                break;
            default:
                break;
        }
    }
}
#include "simulator/tile2_0/multicore.h"
#include "simulator/tile2_0/core_factory.h"

void MulticoreSimulator::Init() {
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {1, 8, 8, 128, true}, {1, 4, 4, 128, false},
            {1, 4, 4, 128, false}, {0, 0, 0, 0, false}
        }};
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, 8, 8, 128, true}, {1, 4, 4, 128, false},
            {0, 0, 0, 0, false}, {0, 0, 0, 0, false}
        }};
    const std::array<FmapTask, L1C_BANK> task_list2 = {{
            {1, 4, 4, 128, true}, {1, 4, 4, 128, false},
            {0, 0, 0, 0, false}, {0, 0, 0, 0, false}
        }};

    create_core_modules(*this, "0", task_list0);
    create_core_modules(*this, "1", task_list1);
    create_core_modules(*this, "2", task_list1);
    create_core_modules(*this, "3", task_list1);
    create_core_modules(*this, "4", task_list2);
    create_core_modules(*this, "5", task_list2);

    register_task_handler<std::tuple<int, std::string>>("task_batch_done",
            [this](const std::tuple<int, std::string>& data) {
                this->handle_batch_task_done(data);
            });

    for (int i = 0; i < 6; ++i) {
        for (int j = 0; j < L1C_BANK; ++j) {
            core_batch_num_map_[{"task_scheduler" + std::to_string(i), j}] = 0;
        }
    }
}

void MulticoreSimulator::handle_batch_task_done(const std::tuple<int, std::string>& data) {
    // 处理任务完成的消息，可以根据需要更新模拟器状态或者触发其他事件
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
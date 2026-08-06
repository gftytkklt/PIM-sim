#include "simulator/tile2_0/membanking.h"
#include "simulator/tile2_0/core_factory.h"

void BankingSimulator::Init() {
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {4, 8, 8, 128, true}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, 8, 8, 128, true}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};
    const std::array<FmapTask, L1C_BANK> task_list2 = {{
            {1, 4, 4, 128, true}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};

    create_core_modules(*this, "0", task_list0);
    create_core_modules(*this, "1", task_list1);
    create_core_modules(*this, "2", task_list2);

    register_task_handler("task_batch_done",
            [this](const GenericMessage& msg) {
                this->handle_batch_task_done(msg);
            });
    core_batch_num_map_["task_scheduler0"] = 0;
    core_batch_num_map_["task_scheduler1"] = 0;
    core_batch_num_map_["task_scheduler2"] = 0;
}

void BankingSimulator::init_task(int core_id, int bank_id, int batch_num) {
    std::string core_name = "task_scheduler" + std::to_string(core_id); // 根据core_id确定对应的任务调度器核心名称
    int local_bank_id = bank_id; // 计算本地bank id，假设每个调度器管理L1C_BANK个bank
    send_message_to_core(core_name, GenericMessage("init_task", std::make_tuple(local_bank_id, batch_num), 0));
}

void BankingSimulator::handle_batch_task_done(const GenericMessage& msg) {
    // 处理任务完成的消息，可以根据需要更新模拟器状态或者触发其他事件
    auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
    // int bank_id = std::get<0>(data);
    std::string core_name = std::get<1>(data);
    // std::cout << "Received task batch done message from " << core_name 
    //           << " with bank id: " << bank_id << std::endl;
    // 写死：core0
    if (core_name == "task_scheduler0") {
        core_batch_num_map_["task_scheduler0"] += 1;
        // 在这里切换触发条件
        // 前两个条件每个分块算四个batch
        switch (block_strategy_) {
            case BlockStrategy::XY:
                if (core_batch_num_map_["task_scheduler0"] == 16) { // 16个batch完成后触发切换
                    send_message_to_core("task_scheduler1", GenericMessage("init_task", std::make_tuple(0, 4), 1)); // 触发切换，持续一个周期
                    core_batch_num_map_["task_scheduler0"] = 0; // 重置计数器
                }
                break;
            case BlockStrategy::YX:
                // 可以在这里实现不同的切换策略，例如每完成8个batch触发一次切换
                if (core_batch_num_map_["task_scheduler0"] == 8) {
                    send_message_to_core("task_scheduler1", GenericMessage("init_task", std::make_tuple(0, 2), 1)); // 触发切换，持续一个周期
                    core_batch_num_map_["task_scheduler0"] = 0; // 重置计数器
                }
                break;
            // 这里是16x4的块，每个块只有2个batch，一共就8次计算
            case BlockStrategy::Custom:
                // 可以在这里实现自定义的切换策略
                if (core_batch_num_map_["task_scheduler0"] == 2) {
                    send_message_to_core("task_scheduler1", GenericMessage("init_task", std::make_tuple(0, 1), 1)); // 触发切换，持续一个周期
                    core_batch_num_map_["task_scheduler0"] = 0; // 重置计数器
                }
                break;
            case BlockStrategy::NoTiling:
                // 不进行切换，直到所有batch完成
                if (core_batch_num_map_["task_scheduler0"] == 2) {
                    // std::cout << "All batches for task_scheduler0 completed." << std::endl;
                    send_message_to_core("task_scheduler1", GenericMessage("init_task", std::make_tuple(0, 1), 1)); // 触发切换，持续一个周期
                    core_batch_num_map_["task_scheduler0"] = 0; // 重置计数器
                }
                break;
        }

    }
    else if (core_name == "task_scheduler1") {
        core_batch_num_map_["task_scheduler1"] += 1;
        if (core_batch_num_map_["task_scheduler1"] == 2) { // 16个batch完成后触发切换
            send_message_to_core("task_scheduler2", GenericMessage("init_task", std::make_tuple(0, 1), 1)); // 触发切换，持续一个周期
            core_batch_num_map_["task_scheduler1"] = 0; // 重置计数器
        }
    }
    else if (core_name == "task_scheduler2") {
        core_batch_num_map_["task_scheduler2"] += 1;
    }
    else {
        throw std::runtime_error("Unknown core name in batch task done message");
    }
}
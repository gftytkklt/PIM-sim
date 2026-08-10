#include "simulator/tile2_0/tiling.h"
#include "simulator/tile2_0/core_factory.h"

void TilingSimulator::Init() {
    int block_size0 = dynamic_banking ? 12 : 8;
    int block_size1 = dynamic_banking ? 6 : 8;
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {1, block_size0, block_size0, 128, true},
            {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, block_size1, block_size1, 128, true},
            {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};

    create_core_modules(*this, "0", task_list0);
    create_core_modules(*this, "1", task_list1);

    register_task_handler("task_batch_done",
            [this](const GenericMessage& msg) {
                this->handle_batch_task_done(msg);
            });
    core_batch_num_map_["task_scheduler0"] = 0;
    core_batch_num_map_["task_scheduler1"] = 0;
}

void TilingSimulator::init_task() {
    // 这里可以根据需要初始化任务，例如发送消息给TaskScheduler触发任务开始。
    std::cout << "Initializing tasks in TilingSimulator..." << std::endl;
    // 特征图是48x48，对8x8分块大小，每个分块有4个batch，一共有36个分块，所以是144
    // 对12x12分块大小，每个分块有6个batch，一共有16个分块，是96
    int batch_num = dynamic_banking ? 96 : 144; // 根据是否启用动态分块策略设置batch数量
    send_message_to_core("task_scheduler0", GenericMessage("init_task", std::make_tuple(0, batch_num), 1)); // 触发core0的任务，持续一个周期
}

void TilingSimulator::handle_batch_task_done(const GenericMessage& msg) {
    auto [bank_id, core_name] = std::any_cast<std::tuple<int, std::string>>(msg.body);
    // std::cout << "Received task batch done message for bank " << bank_id 
    //           << " with core: " << core_name << std::endl;
    // 根据core_name更新对应的batch计数器
    if (core_name == "task_scheduler0") {
        core_batch_num_map_["task_scheduler0"] += 1;
        // 如果core0的batch完成数量达到2，触发core1的任务开始
        // 这里采用YX策略吧。
        // 如果是12x12，每个分块6个batch，2个batch直接触发切换。
        // 如果是8x8，每个分块4个batch，2个分块就是8个batch，触发切换。
        int batch_threshold = dynamic_banking ? 2 : 8;
        if (core_batch_num_map_["task_scheduler0"] == batch_threshold) {
            send_message_to_core("task_scheduler1", GenericMessage("init_task", std::make_tuple(0, 2), 1)); // 触发切换，持续一个周期
            core_batch_num_map_["task_scheduler0"] = 0; // 重置计数器
        }
    }

}
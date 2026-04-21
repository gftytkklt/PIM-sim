#include "simulator/tile2_0/tiling.h"

void TilingSimulator::Init() {
    // core 0
    int block_size0 = dynamic_banking ? 12 : 8;
    const std::array<FmapTask, L1C_BANK> task_list0 = {{
            {1, block_size0, block_size0, 128, true}, // Bank 0
            {0, 0, 0, 0, 0}, // Bank 1
            {0, 0, 0, 0, 0}, // Bank 2
            {0, 0, 0, 0, 0}  // Bank 3
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
    // TS to SIMD
    connect_modules("task_scheduler0", "pooling_enabled", "simd0", "SIMD_pooling_enable");
    connect_modules("simd0", "SIMD_data_valid", "task_scheduler0", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar0", "computation_done", "simd0", "SIMD_channel_batch");
    // core 1
    int block_size1 = dynamic_banking ? 6 : 8;
    const std::array<FmapTask, L1C_BANK> task_list1 = {{
            {1, block_size1, block_size1, 128, true}, // Bank 0
            {0, 0, 0, 0, 0}, // Bank 1
            {0, 0, 0, 0, 0}, // Bank 2
            {0, 0, 0, 0, 0}  // Bank 3
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
    // TS to SIMD
    connect_modules("task_scheduler1", "pooling_enabled", "simd1", "SIMD_pooling_enable");
    connect_modules("simd1", "SIMD_data_valid", "task_scheduler1", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar1", "computation_done", "simd1", "SIMD_channel_batch");
    // message handler for task completion
    register_task_handler("task_batch_done",
            [this](const GenericMessage& msg) {
                this->handle_batch_task_done(msg);
            });
    // 初始化batch计数器
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
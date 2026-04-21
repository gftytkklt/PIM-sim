#include "simulator/tile2_0/tile2_0.h"

void OPUSimulator::Init() {
    // 注册模块
    const std::array<FmapTask, L1C_BANK> task_list = {{
            {1, 8, 8, 128, true}, // Bank 0
            {0, 0, 0, 0, 0}, // Bank 1
            {0, 0, 0, 0, 0}, // Bank 2
            {0, 0, 0, 0, 0}  // Bank 3
        }};
    register_module<SIMD>("simd", 4);
    register_module<Crossbar>("crossbar", 3);
    register_module<TaskScheduler>("task_scheduler", 2, task_list);
    register_module<L1C>("L1_cache", 1);
    // 模块连接关系
    // TS to L1C
    connect_modules("task_scheduler", "cache_read_trigger", "L1_cache", "cache_read_trigger");
    connect_modules("task_scheduler", "cache_read_len", "L1_cache", "cache_read_len");
    connect_modules("task_scheduler", "cache_write_trigger", "L1_cache", "cache_write_trigger");
    connect_modules("task_scheduler", "cache_write_len", "L1_cache", "cache_write_len");
    connect_modules("L1_cache", "cache_read_done", "task_scheduler", "cache_read_valid");
    connect_modules("L1_cache", "cache_write_done", "task_scheduler", "cache_write_done");
    // TS to XBAR
    connect_modules("task_scheduler", "xbar_computation_trigger", "crossbar", "computation_trigger");
    connect_modules("task_scheduler", "xbar_switching_trigger", "crossbar", "switching_trigger");
    connect_modules("crossbar", "switching_done", "task_scheduler", "xbar_switching_done");
    // TS to SIMD
    connect_modules("task_scheduler", "pooling_enabled", "simd", "SIMD_pooling_enable");
    connect_modules("simd", "SIMD_data_valid", "task_scheduler", "SIMD_computation_done");
    // XBAR to SIMD
    connect_modules("crossbar", "computation_done", "simd", "SIMD_channel_batch");
    // 注册消息处理器
    register_task_handler("SIMD_computation_done", 
            [this](const GenericMessage& msg) {
                this->handle_simd_computation_done(msg);
            });
}
// 收到SIMD_done，增加完成计算的点数信息。
void OPUSimulator::handle_simd_computation_done(const GenericMessage& msg) {
    // 处理SIMD计算完成的消息
    std::cout << "Received SIMD computation done message with value: " 
              << std::any_cast<int>(msg.body) << std::endl;
    // 可以在这里更新任务调度器的状态或者触发后续的任务。
}

// 任务队列初始化，模拟计算开始时L1C已有部分数据。
void OPUSimulator::init_task(int bank_id, int batch_num) {
    send_message_to_core("task_scheduler", GenericMessage("init_task", std::make_tuple(bank_id, batch_num), 0));
}
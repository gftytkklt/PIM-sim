#include "simulator/tile2_0/tile2_0.h"

void OPUSimulator::Init() {
    const std::array<FmapTask, L1C_BANK> task_list = {{
            {1, 6, 12, 128, true},
            {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}, {0, 0, 0, 0, 0}
        }};
    register_module<SIMD>("simd", 4);
    register_module<Crossbar>("crossbar", 3);
    register_module<TaskScheduler>("task_scheduler", 2, task_list);
    register_module<L1C>("L1_cache", 1);
    connect_modules("task_scheduler", "cache_read_trigger", "L1_cache", "cache_read_trigger");
    connect_modules("task_scheduler", "cache_read_len", "L1_cache", "cache_read_len");
    connect_modules("task_scheduler", "cache_write_trigger", "L1_cache", "cache_write_trigger");
    connect_modules("task_scheduler", "cache_write_len", "L1_cache", "cache_write_len");
    connect_modules("L1_cache", "cache_read_done", "task_scheduler", "cache_read_valid");
    connect_modules("L1_cache", "cache_write_done", "task_scheduler", "cache_write_done");
    connect_modules("task_scheduler", "xbar_computation_trigger", "crossbar", "computation_trigger");
    connect_modules("task_scheduler", "xbar_switching_trigger", "crossbar", "switching_trigger");
    connect_modules("crossbar", "switching_done", "task_scheduler", "xbar_switching_done");
    connect_modules("crossbar", "computation_done", "task_scheduler", "xbar_computation_done");
    connect_modules("task_scheduler", "pooling_enabled", "simd", "SIMD_pooling_enable");
    connect_modules("simd", "SIMD_data_valid", "task_scheduler", "SIMD_computation_done");
    connect_modules("crossbar", "computation_done", "simd", "SIMD_channel_batch");

    register_task_handler("task_batch_done",
            [this](const GenericMessage& msg) {
                this->handle_batch_task_done(msg);
            });
}

// 任务队列初始化，模拟计算开始时L1C已有部分数据。
void OPUSimulator::init_task(int bank_id, int batch_num) {
    send_message_to_core("task_scheduler", GenericMessage("init_task", std::make_tuple(bank_id, batch_num), 0));
}

void OPUSimulator::handle_batch_task_done(const GenericMessage& msg) {
    auto [bank_id, core_name] = std::get<std::tuple<int, std::string>>(msg.body);
    std::cout << "Received task batch done message for bank " << bank_id 
              << " with core: " << core_name << std::endl;
    // 可以在这里更新任务调度器的状态或者触发后续的任务。
}
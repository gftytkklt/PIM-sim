#include "simulator/tile2_0/SIMD.h"

SIMD::SIMD(const std::string& id) : ModuleBase(id) {
    // Crossbar接口
    add_signal(Signal("SIMD_channel_batch", Signal::Direction::INPUT)); // int, input batch num
    add_signal(Signal("SIMD_compute_ready", Signal::Direction::OUTPUT, std::make_any<bool>(true))); // bool, output compute ready signal
    // TS接口
    add_signal(Signal("SIMD_pooling_enable", Signal::Direction::INPUT)); // bool, input pooling enable signal
    add_signal(Signal("SIMD_data_valid", Signal::Direction::OUTPUT, std::make_any<bool>(false))); // bool, output data valid
    set_signal_value("SIMD_compute_ready", true, 0); // 初始化时SIMD处于ready状态
}

bool SIMD::check_computation_trigger() {
    auto channel_batch_val = get_signal_value("SIMD_channel_batch");
    // auto channel_batch = channel_batch_val.has_value() ? std::any_cast<int>(channel_batch_val) : 0;
    // std::cout << "channel batch = " << channel_batch << std::endl;
    return channel_batch_val.has_value() && std::any_cast<int>(channel_batch_val) > 0;
}

bool SIMD::check_computation_exec() {
    auto SIMD_ready = get_signal_value("SIMD_compute_ready");
    if (SIMD_ready.has_value() && std::any_cast<bool>(SIMD_ready)) {
        // throw std::runtime_error("SIMD is already computing but received another trigger"); // 如果正在计算但又被触发，说明实现有问题
        // submit_signal_value("SIMD_processing", true, 1);
        submit_signal_value("SIMD_compute_ready", false, 1); // 计算开始后重置ready信号
        auto channel_batch_val = get_signal_value("SIMD_channel_batch");
        auto stage_num = (std::any_cast<int>(channel_batch_val) + channel_num_ - 1) / channel_num_; // 计算需要的阶段数
        auto pooling_enable_val = get_signal_value("SIMD_pooling_enable");
        auto pooling_enable = pooling_enable_val.has_value() && std::any_cast<bool>(pooling_enable_val);
        pooling_latency_ = pooling_enable ? SIMD_POOLING_LATENCY : 0;
        auto stage_level = pooling_enable ? 3 : 2; // 如果启用pooling，阶段数增加一个
        if (enable_pipeline_) {
            auto pipeline_latency = std::max({quant_latency_, activate_latency_, pooling_latency_});
            compute_latency_ = pipeline_latency * (stage_level + stage_num - 1);
        }
        else {
            compute_latency_ = stage_num * (quant_latency_ + activate_latency_ + pooling_latency_); // 不启用流水线时，计算总延迟为每个阶段的延迟乘以阶段数
        }
        // std::cout << "computation latency set to " << compute_latency_ << " cycles for channel batch " << std::any_cast<int>(channel_batch_val) 
        //           << " with pooling " << pooling_enable << std::endl;
        submit_signal_value("SIMD_data_valid", true, compute_latency_); // 计算完成后数据有效
        return true;
    }
    return false;
}

bool SIMD::check_computation_finish() {
    auto data_valid = get_signal_value("SIMD_data_valid");
    return data_valid.has_value() && std::any_cast<bool>(data_valid);
}

bool SIMD::check_computation_end() {
    // submit_signal_value("SIMD_processing", false, 1); // 计算完成后重置processing信号
    submit_signal_value("SIMD_compute_ready", true, 1); // 计算完成后设置ready信号
    submit_signal_value("SIMD_data_valid", false, 1); // 重置数据有效信号
    return true;
}

#ifndef SIGNALS_H
#define SIGNALS_H

#include <string>
#include <stdexcept>

// 类型化信号 ID：替换字符串信号名，编译期捕获拼写错误
enum class SignalID {
    // Crossbar
    computation_trigger,
    switching_trigger,
    computation_done,
    switching_done,
    computation_process,
    switching_process,
    // L1C
    cache_read_trigger,
    cache_read_len,
    cache_read_done,
    cache_read_valid,
    cache_read_process,
    cache_write_trigger,
    cache_write_len,
    cache_write_done,
    // TaskScheduler → Crossbar/SIMD 桥接
    xbar_computation_trigger,
    xbar_switching_trigger,
    xbar_computation_done,
    xbar_switching_done,
    pooling_enabled,
    // TaskScheduler 内部
    batch_wr_bank,
    batch_wr_done,
    current_task_id,
    // SIMD
    SIMD_channel_batch,
    SIMD_compute_ready,
    SIMD_data_valid,
    SIMD_pooling_enable,
    SIMD_computation_done,
    // 测试辅助
    test_int_signal,
    test_bool_signal,
    test_empty_signal,
    // 哨兵
    COUNT
};

// SignalID → 名称（用于日志/调试）
inline const char* signal_name(SignalID id) {
    switch (id) {
        case SignalID::computation_trigger: return "computation_trigger";
        case SignalID::switching_trigger: return "switching_trigger";
        case SignalID::computation_done: return "computation_done";
        case SignalID::switching_done: return "switching_done";
        case SignalID::computation_process: return "computation_process";
        case SignalID::switching_process: return "switching_process";
        case SignalID::cache_read_trigger: return "cache_read_trigger";
        case SignalID::cache_read_len: return "cache_read_len";
        case SignalID::cache_read_done: return "cache_read_done";
        case SignalID::cache_read_valid: return "cache_read_valid";
        case SignalID::cache_read_process: return "cache_read_process";
        case SignalID::cache_write_trigger: return "cache_write_trigger";
        case SignalID::cache_write_len: return "cache_write_len";
        case SignalID::cache_write_done: return "cache_write_done";
        case SignalID::xbar_computation_trigger: return "xbar_computation_trigger";
        case SignalID::xbar_switching_trigger: return "xbar_switching_trigger";
        case SignalID::xbar_computation_done: return "xbar_computation_done";
        case SignalID::xbar_switching_done: return "xbar_switching_done";
        case SignalID::pooling_enabled: return "pooling_enabled";
        case SignalID::batch_wr_bank: return "batch_wr_bank";
        case SignalID::batch_wr_done: return "batch_wr_done";
        case SignalID::current_task_id: return "current_task_id";
        case SignalID::SIMD_channel_batch: return "SIMD_channel_batch";
        case SignalID::SIMD_compute_ready: return "SIMD_compute_ready";
        case SignalID::SIMD_data_valid: return "SIMD_data_valid";
        case SignalID::SIMD_pooling_enable: return "SIMD_pooling_enable";
        case SignalID::SIMD_computation_done: return "SIMD_computation_done";
        case SignalID::test_int_signal: return "test_int_signal";
        case SignalID::test_bool_signal: return "test_bool_signal";
        case SignalID::test_empty_signal: return "test_empty_signal";
        default: return "unknown_signal";
    }
}

// 名称 → SignalID（用于配置/调试），未找到抛异常
inline SignalID signal_id_from_string(const std::string& name) {
    for (int i = 0; i < static_cast<int>(SignalID::COUNT); ++i) {
        if (signal_name(static_cast<SignalID>(i)) == name) {
            return static_cast<SignalID>(i);
        }
    }
    throw std::invalid_argument("Unknown signal name: " + name);
}

#endif
#include "simulator/tile2_0/L1C.h"

// 模拟Tile2.0的两读一写端口设计。假设每次写任务均匀分配到2个读端口，而写是同时写两列。

L1C::L1C(const std::string& id) : ModuleBase(id) {
    for (int i = 0; i < L1C_SRAM_NUM; i++) {
        srams_[i] = {L1C_SRAM_LINE_BYTES * 8, L1C_SRAM_DEPTH};
    }
    add_signal(Signal(SignalID::cache_read_trigger, Signal::Direction::INPUT)); // int, bank id
    add_signal(Signal(SignalID::cache_read_len, Signal::Direction::INPUT)); // int, valid_lines
    add_signal(Signal(SignalID::cache_write_trigger, Signal::Direction::INPUT)); // int, bank id
    add_signal(Signal(SignalID::cache_write_len, Signal::Direction::INPUT)); // int, valid_lines
    add_signal(Signal(SignalID::cache_read_done, Signal::Direction::OUTPUT, false)); // bool
    add_signal(Signal(SignalID::cache_write_done, Signal::Direction::OUTPUT, false)); // bool
}

bool L1C::check_cache_read_trigger() {
    auto trigger_val = get_signal_value(SignalID::cache_read_trigger);
    auto len_val = get_signal_value(SignalID::cache_read_len);
    return trigger_val.has_value() && len_val.has_value() && std::any_cast<int>(trigger_val) >= 0 
           && std::any_cast<int>(trigger_val) < L1C_BANK && std::any_cast<int>(len_val) > 0;
}

bool L1C::check_cache_read_exec() {
    auto trigger_val = get_signal_value(SignalID::cache_read_trigger);
    auto len_val = get_signal_value(SignalID::cache_read_len);
    if (trigger_val.has_value() && len_val.has_value()) {
        int bank_id = std::any_cast<int>(trigger_val);
        int valid_lines = std::any_cast<int>(len_val);
        // 模拟读操作：更新性能统计，提交读进程信号
        total_accesses_[bank_id] += valid_lines;
        raise_sram_rd_process(valid_lines); // 提交读进程信号
        // submit_signal_value("cache_read_process", true, 1); // 提交读进程信号
        // submit_signal_value("cache_read_done", true, valid_lines); // 提交读完成信号，携带访问的行数信息
        return true;
    }
    throw std::runtime_error("Invalid signal value for cache_read_trigger or cache_read_len");
}
// 这里的逻辑相当于，在检测到done信号以后，跳转至finish，并在一个周期以后重置。
bool L1C::check_cache_read_finish() {
    auto process_val = get_signal_value(SignalID::cache_read_done);
    return process_val.has_value() && std::any_cast<bool>(process_val);
}

bool L1C::check_cache_read_end() {
    // submit_signal_value("cache_read_process", {}, 1); // 重置读进程信号
    // submit_signal_value("cache_read_done", {}, 1); // 重置读完成信号
    invalidate_sram_rd_process(); // 重置读进程和读完成信号
    return true;
}

bool L1C::check_cache_write_trigger() {
    auto trigger_val = get_signal_value(SignalID::cache_write_trigger);
    auto len_val = get_signal_value(SignalID::cache_write_len);
    return trigger_val.has_value() && len_val.has_value() && std::any_cast<int>(trigger_val) >= 0 
           && std::any_cast<int>(trigger_val) < L1C_BANK && std::any_cast<int>(len_val) > 0;
}

bool L1C::check_cache_write_exec() {
    auto trigger_val = get_signal_value(SignalID::cache_write_trigger);
    auto len_val = get_signal_value(SignalID::cache_write_len);
    if (trigger_val.has_value() && len_val.has_value()) {
        int bank_id = std::any_cast<int>(trigger_val);
        int valid_lines = std::any_cast<int>(len_val);
        // 模拟写操作：更新性能统计，提交写进程信号
        total_accesses_[bank_id] += valid_lines;
        raise_sram_wresp(valid_lines); // 提交写完成信号
        // submit_signal_value("cache_write_done", true, valid_lines); // 提交写完成信号，携带访问的行数信息
        return true;
    }
    throw std::runtime_error("Invalid signal value for cache_write_trigger or cache_write_len");
}

bool L1C::check_cache_write_finish() {
    auto process_val = get_signal_value(SignalID::cache_write_done);
    return process_val.has_value() && std::any_cast<bool>(process_val);
}

bool L1C::check_cache_write_end() {
    invalidate_sram_wresp(); // 重置写完成信号
    // submit_signal_value("cache_write_done", {}, 1); // 重置写完成信号
    return true;
}
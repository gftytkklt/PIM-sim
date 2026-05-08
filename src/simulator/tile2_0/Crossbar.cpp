#include "simulator/tile2_0/Crossbar.h"

Crossbar::Crossbar(const std::string& id) : ModuleBase(id) {
    add_signal(Signal("computation_trigger", Signal::Direction::INPUT)); // bool
    add_signal(Signal("switching_trigger", Signal::Direction::INPUT)); // int
    add_signal(Signal("computation_done", Signal::Direction::OUTPUT)); // int, output channel num
    add_signal(Signal("switching_done", Signal::Direction::OUTPUT, false)); // bool
    add_signal(Signal("computation_process", Signal::Direction::INTERNAL, false)); // bool
    add_signal(Signal("switching_process", Signal::Direction::INTERNAL, false)); // bool
    // add_signal(Signal("Pooling_enabled", Signal::Direction::OUTPUT, false)); // bool
}

Crossbar::Crossbar(const std::string& id, const std::vector<ComputeTask>& tasks) : ModuleBase(id) {
    for (const auto& task : tasks) {
        add_xbar_task(task);
    }
    add_signal(Signal("computation_trigger", Signal::Direction::INPUT)); // bool
    add_signal(Signal("switching_trigger", Signal::Direction::INPUT)); // int
    add_signal(Signal("computation_done", Signal::Direction::OUTPUT)); // int, output channel num
    add_signal(Signal("switching_done", Signal::Direction::OUTPUT, false)); // bool
    add_signal(Signal("computation_process", Signal::Direction::INTERNAL, false)); // bool
    add_signal(Signal("switching_process", Signal::Direction::INTERNAL, false)); // bool
    // add_signal(Signal("Pooling_enabled", Signal::Direction::OUTPUT, false)); // bool
}

bool Crossbar::check_computation_trigger() {
    auto trigger_val = get_signal_value("computation_trigger");
    return trigger_val.has_value() && std::any_cast<bool>(trigger_val);
}
// computation priority higher than switching
bool Crossbar::check_computation_exec() {
    auto switching_val = get_signal_value("switching_process");
    if (switching_val.has_value() && std::any_cast<bool>(switching_val)) {
        return false; // 如果正在切换，计算不能执行
    }
    submit_signal_value("computation_process", true, 1); // 提交当前计算任务索引
    submit_signal_value("computation_done", XBAR_BL, compute_latency_); // 提交计算完成信号，携带当前任务的bl_num信息
    increment_current_task_counter();
    return true; // 简化：只要触发了就执行
}

bool Crossbar::check_computation_finish() {
    auto computation_done = get_signal_value("computation_done");
    return computation_done.has_value();
}

bool Crossbar::check_computation_end() {
    submit_signal_value("computation_process", {}, 1); // 重置计算进程信号
    submit_signal_value("computation_done", {}, 1); // 重置计算完成信号
    return true;
}

bool Crossbar::check_switching_trigger() {
    auto trigger_val = get_signal_value("switching_trigger");
    // return trigger_val.has_value() && std::any_cast<bool>(trigger_val);
    return trigger_val.has_value() && std::any_cast<int>(trigger_val) >= 0 && std::any_cast<int>(trigger_val) < xbar_num_;
}

bool Crossbar::check_switching_exec() {
    auto computation_val = get_signal_value("computation_process");
    if (computation_val.has_value() && std::any_cast<bool>(computation_val)) {
        return false; // 如果正在计算，切换不能执行
    }
    auto computation_trigger_val = get_signal_value("computation_trigger");
    if (computation_trigger_val.has_value() && std::any_cast<bool>(computation_trigger_val)) {
        return false; // 如果计算触发了，切换不能执行
    }
    // get switching index from switch_val
    auto switching_val = get_signal_value("switching_trigger");
    set_current_task_index(std::any_cast<int>(switching_val));
    submit_signal_value("switching_process", true, 1); // 提交切换正在进行的信号
    submit_signal_value("switching_done", true, switch_latency_); // 重置切换完成信号
    return true; // 简化：只要触发了就执行
}

bool Crossbar::check_switching_finish() {
    auto switching_done = get_signal_value("switching_done");
    return switching_done.has_value() && std::any_cast<bool>(switching_done);
}

bool Crossbar::check_switching_end() {
    submit_signal_value("switching_process", {}, 1); // 重置切换进程信号
    submit_signal_value("switching_done", {}, 1); // 重置切换完成信号
    return true;
}

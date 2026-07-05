#ifndef CROSSBAR_H
#define CROSSBAR_H

#include "config.h"

struct ComputeTask{
    int wl_num;
    int bl_num;
    int task_num;
    bool pooling;
};

struct LogicalXbar{
    ComputeTask task;
    int finish_task_counter;
};

class Crossbar : public ModuleBase<Crossbar> {
public:
    Crossbar(const std::string& id);
    // add and init signal
    Crossbar(const std::string& id, const std::vector<ComputeTask>& tasks);
    void register_processes() override final {
        // 注册交叉开关的进程
        register_process("xbar_computation",
            [this]() { return check_computation_trigger(); },
            [this]() { return check_computation_exec(); },
            [this]() { return check_computation_finish(); },
            [this]() { return check_computation_end(); },
            XBAR_COMPUTE_LATENCY
        );
        register_process("xbar_switching",
            [this]() { return check_switching_trigger(); },
            [this]() { return check_switching_exec(); },
            [this]() { return check_switching_finish(); },
            [this]() { return check_switching_end(); },
            XBAR_SWITCH_LATENCY
        );
    }
    void register_message_handlers() override final {
        // 交叉开关目前没有需要处理的消息，可以留空或者添加一些调试消息的处理函数。
    }
    
private:
    bool check_computation_trigger();
    bool check_computation_exec();
    bool check_computation_finish();
    bool check_computation_end();

    bool check_switching_trigger();
    bool check_switching_exec();
    bool check_switching_finish();
    bool check_switching_end();

    // bool check_task_completion() const;

    int get_currrent_task_index() const {
        return cur_task_index_;
    }

    void set_current_task_index(int index) {
        if (index >= 0 && index < xbar_num_) {
            cur_task_index_ = index;
        }
        else {
            throw std::out_of_range("Invalid task index for crossbar: " + std::to_string(index));
        }
    }

    const auto& get_xbar_tasks() const {
        return xbar_tasks_;
    }

    const auto& get_current_xbar_task() const {
        if (cur_task_index_ >= 0 && cur_task_index_ < xbar_tasks_.size()) {
            return xbar_tasks_[cur_task_index_];
        }
        throw std::out_of_range("Current task index is out of range: " + std::to_string(cur_task_index_));
    }

    void add_xbar_task(const ComputeTask& task) {
        xbar_tasks_.push_back({task, 0});
    }

    void clear_xbar_tasks() {
        xbar_tasks_.clear();
        cur_task_index_ = 0;
    }

    void increment_current_task_counter() {
        if (cur_task_index_ < xbar_tasks_.size()) {
            xbar_tasks_[cur_task_index_].finish_task_counter++;
        }
    }

    const int compute_latency_ = XBAR_COMPUTE_LATENCY;
    const int switch_latency_ = XBAR_SWITCH_LATENCY;
    const int xbar_num_ = XBAR_NUM;
    std::vector<LogicalXbar> xbar_tasks_;
    int cur_task_index_ = 0;
};


#endif
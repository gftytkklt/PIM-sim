#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "config.h"

struct TaskCounter {
    // 任务总数
    int batch_num;
    int pt_num;
    // 当前任务的计数
    int pt_cnt; // 考虑奇数col的情况下，最后一个batch可能只有一列的数据，但目前先不处理
    int batch_cnt;
    // 当计数完成返回true，表明完成了该批次两列计算
    bool step() {
        // std::cout << "batch_num=" << batch_num << ", batch_cnt=" << batch_cnt << ", pt_cnt=" << pt_cnt << std::endl;
        pt_cnt++;
        if (pt_cnt == pt_num) {
            pt_cnt = 0;
            batch_cnt++;
            if (batch_cnt == batch_num) {
                batch_cnt = 0;
                // std::cout << "Task batch completed!" << std::endl;
            }
            return true; // 完成了一个batch的计算
        }
        return false; // 任务未完成
    }
};

enum class TaskStatus {
    IDLE,
    READ_DATA,
    COMPUTE,
};

// 全局
struct FmapTask {
    int block_num; // 分块总数
    int row;
    int col;
    int channel_num; // 读给PE是输入通道数，激励写L1C是输出通道数
    bool pooling;
};

struct BatchInfo {
    int batch_lines; // 每批次占用的SRAM行数
    int valid_batch_num;
    int max_batch_capacity;
};

class TaskScheduler : public ModuleBase<TaskScheduler> {
public:
    TaskScheduler(const std::string& id, const std::array<FmapTask, L1C_BANK>& task_list);
    void register_processes() override final {
        register_process("Read_Task",
            [this]() { return check_rtask_trigger(); },
            [this]() { return check_rtask_exec(); },
            [this]() { return check_rtask_finish(); },
            [this]() { return check_rtask_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        // register_process("Partial_Read_Task",
        //     [this]() { return check_ptrd_trigger(); },
        //     [this]() { return check_ptrd_exec(); },
        //     [this]() { return check_ptrd_finish(); },
        //     [this]() { return check_ptrd_end(); },
        //     1 // latency will be determined by the number of lines accessed
        // );
        register_process("Write_Task",
            [this]() { return check_wtask_trigger(); },
            [this]() { return check_wtask_exec(); },
            [this]() { return check_wtask_finish(); },
            [this]() { return check_wtask_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        // register_process("Partial_Write_Task",
        //     [this]() { return check_ptwr_trigger(); },
        //     [this]() { return check_ptwr_exec(); },
        //     [this]() { return check_ptwr_finish(); },
        //     [this]() { return check_ptwr_end(); },
        //     1 // latency will be determined by the number of lines accessed
        // );
        // register_process("Switch_Task",
        //     [this]() { return check_switch_task_trigger(); },
        //     [this]() { return check_switch_task_exec(); },
        //     [this]() { return check_switch_task_finish(); },
        //     [this]() { return check_switch_task_end(); },
        //     1 // latency will be determined by the number of lines accessed
        // );
    }

    // 留给多核判断反压的接口

private:
    // 两列写回L1C的任务触发
    bool check_wtask_trigger();
    bool check_wtask_exec();
    bool check_wtask_finish();
    bool check_wtask_end();

    // 写回L1C单次计算的任务触发，目前对批量数据进行建模
    // 对单次写入的检测，可以参考rtask的状态机实现。
    // bool check_ptwr_trigger(){return false;}
    // bool check_ptwr_exec(){return false;}
    // bool check_ptwr_finish(){return false;}
    // bool check_ptwr_end(){return false;}

    // 两列计算的整体任务触发
    bool check_rtask_trigger();
    bool check_rtask_exec();
    bool check_rtask_finish();
    bool check_rtask_end();

    // 读L1C单次计算的任务触发
    // 这些函数被rtask使用状态机实现了。
    // bool check_ptrd_trigger(){return false;}
    // bool check_ptrd_exec(){return false;}
    // bool check_ptrd_finish(){return false;}
    // bool check_ptrd_end(){return false;}

    // bool check_switch_task_trigger();
    // bool check_switch_task_exec();
    // bool check_switch_task_finish();
    // bool check_switch_task_end();

    // 简化写入的计算任务初始化建模
    // 第一次是两个batch，后面都是1个
    void init_pending_tasks(int bank_id, int batch_num) {
        for (int i = 0; i < batch_num; ++i) {
            pending_tasks_.push(bank_id);
        }
        // std::cout << "task num =" << pending_tasks_.size() << std::endl;
    }

    // 根据当前进程任务计数判断是否满足taskqueue调度条件，返回批次
    void update_pending_tasks(int bank_id = -1) {
        auto bank_task_counter = task_counters_[bank_id];
        int batch_required = (bank_task_counter.batch_cnt == 0) ? 2 : 1;
        if(batch_data_info[bank_id].valid_batch_num >= batch_required) {
            pending_tasks_.push(bank_id);
        }
    }

    void raise_sram_wr_req(int bank_id, int lines) {
        submit_signal_value("cache_write_trigger", bank_id, 1); // 触发写任务
        submit_signal_value("cache_write_len", lines, 1); // 写任务长度
    }

    void invalidate_sram_wr_req() {
        submit_signal_value("cache_write_trigger", {}, 1); // 重置写任务触发信号
        submit_signal_value("cache_write_len", {}, 1); // 重置写任务长度
    }

    void raise_sram_rd_req(int bank_id, int lines) {
        submit_signal_value("cache_read_trigger", bank_id, 1); // 触发读任务
        submit_signal_value("cache_read_len", lines, 1); // 读任务长度
    }

    void invalidate_sram_rd_req() {
        submit_signal_value("cache_read_trigger", {}, 1); // 重置读任务触发信号
        submit_signal_value("cache_read_len", {}, 1); // 重置读任务长度
    }

    void raise_xbar_computation_trigger() {
        submit_signal_value("xbar_computation_trigger", true, 1); // 触发计算
        submit_signal_value("pooling_enabled", task_list_[current_task_id_].pooling, 1); // 设置pooling使能信号
    }

    void invalidate_xbar_computation_trigger() {
        submit_signal_value("xbar_computation_trigger", {}, 1); // 重置计算触发信号
        submit_signal_value("pooling_enabled", {}, 1); // 重置pooling使能信号
    }

    void raise_switch_task_trigger() {
        submit_signal_value("xbar_switching_trigger", current_task_id_, 1); // 触发任务切换
        submit_signal_value("switching_process", true, 1); // 设置切换中标志
    }

    void invalidate_switch_task_trigger() {
        submit_signal_value("xbar_switching_trigger", {}, 1); // 重置任务切换触发信号
        submit_signal_value("switching_process", {}, 1); // 重置切换中标志
    }

    // 模块内部消息示例：例如SRAM的写入可以通过这个接口来update，也可以触发wr_trigger。
    virtual void handle_message(const GenericMessage& msg) override final{
        // 处理来自SIMD的计算完成消息，更新当前任务状态
        if (msg.task_id == "SIMD_computation_done") {
            auto data = std::any_cast<int>(msg.body);
            std::cout << "TaskScheduler received SIMD computation done message with value: " << data << std::endl;
        }
        else {
            throw std::runtime_error("TaskScheduler received unknown message with task_id: " + msg.task_id);
        }
    }

    // 每个xbar维护自己当前的任务计数器
    std::array<TaskCounter, L1C_BANK> task_counters_; // 任务计数器，记录每个bank当前执行的任务状态
    std::array<FmapTask, L1C_BANK> task_list_; // 整体任务信息
    std::array<BatchInfo, L1C_BANK> batch_data_info; // 当前L1C有效数据批数
    std::array<bool, L1C_BANK> task_finish_flags_; // 各bank推理任务是否完成
    bool core_task_finish_flag_ = false; // 核心计算任务完成标志
    // 通过ID和当前任务计数器的状态，判断task执行的读操作参数
    std::queue<int> pending_tasks_; // 存储待调度的任务，元素可以是任务ID或者其他标识信息
    int current_task_id_ = -1; // 当前正在执行的任务ID
    int last_executed_task_id_ = -1; // 上一个执行的任务ID
    TaskStatus current_task_status_ = TaskStatus::IDLE; // 当前任务状态
};

#endif
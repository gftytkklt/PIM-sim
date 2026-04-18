#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "config.h"

struct TaskCounter {
    int pt_cnt;
    int batch_cnt;
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
        register_process("Partial_Read_Task",
            [this]() { return check_ptrd_trigger(); },
            [this]() { return check_ptrd_exec(); },
            [this]() { return check_ptrd_finish(); },
            [this]() { return check_ptrd_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        register_process("Write_Task",
            [this]() { return check_wtask_trigger(); },
            [this]() { return check_wtask_exec(); },
            [this]() { return check_wtask_finish(); },
            [this]() { return check_wtask_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        register_process("Partial_Write_Task",
            [this]() { return check_ptwr_trigger(); },
            [this]() { return check_ptwr_exec(); },
            [this]() { return check_ptwr_finish(); },
            [this]() { return check_ptwr_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        register_process("Switch_Task",
            [this]() { return check_switch_task_trigger(); },
            [this]() { return check_switch_task_exec(); },
            [this]() { return check_switch_task_finish(); },
            [this]() { return check_switch_task_end(); },
            1 // latency will be determined by the number of lines accessed
        );
    }

    // 留给多核判断反压的接口

private:
    // 两列写回L1C的任务触发
    bool check_wtask_trigger();
    bool check_wtask_exec();
    bool check_wtask_finish();
    bool check_wtask_end();

    // 写回L1C单次计算的任务触发
    bool check_ptwr_trigger(){return true;}
    bool check_ptwr_exec(){return true;}
    bool check_ptwr_finish(){return true;}
    bool check_ptwr_end(){return true;}

    // 两列计算的整体任务触发
    bool check_rtask_trigger();
    bool check_rtask_exec();
    bool check_rtask_finish();
    bool check_rtask_end();

    // 读L1C单次计算的任务触发
    bool check_ptrd_trigger();
    bool check_ptrd_exec();
    bool check_ptrd_finish();
    bool check_ptrd_end();

    bool check_switch_task_trigger();
    bool check_switch_task_exec();
    bool check_switch_task_finish();
    bool check_switch_task_end();

    // 根据当前进程任务计数判断是否满足taskqueue调度条件，返回批次
    void update_pending_tasks(int bank_id = -1) {
        auto bank_task_counter = task_counters_[bank_id];
        int batch_required = (bank_task_counter.batch_cnt == 0) ? 2 : 1;
        if(batch_data_info[bank_id].valid_batch_num >= batch_required) {
            pending_tasks_.push(bank_id);
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
};

#endif
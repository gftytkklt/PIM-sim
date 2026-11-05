#ifndef TASK_SCHEDULER_H
#define TASK_SCHEDULER_H

#include "config.h"
#include <limits>
#include <stdexcept>

// 这里的粒度与rtask对齐，完成一批计算会发送一次完成信号，因此不在这里建模多个分块的计算。
struct TaskCounter {
    // 任务总数
    int batch_num = 0;
    int pt_num = 0;
    // 当前任务的计数
    int pt_cnt = 0;
    int batch_cnt = 0;
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
    WAIT_OUTPUT,
};

// 这里指输出特征图大小
// 通道实际上是由输入通道数决定的
// 因为输入最多支持128通道，跟特征图到底有几个通道无关
struct FmapTask {
    int block_num; // 分块总数
    int row;
    int col;
    int channel_num; // 输入激励的大小
    bool pooling;
};

struct BatchInfo {
    int batch_lines; // 每批次占用的SRAM行数
    int valid_batch_num;
    int max_batch_capacity;
};

class TaskScheduler : public ModuleBase {
public:
    TaskScheduler(const std::string& id, const std::array<FmapTask, L1C_BANK>& task_list);
    uint64_t completed_batches() const { return completed_batches_; }
    uint64_t admitted_batches() const { return admitted_batches_; }
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
    void register_message_handlers() override final {
        // 类型化注册：handler 直接接收 tuple<int,int>（bank_id, batch_num），
        // 框架自动登记消息类型并在分发时校验
        register_message_handler<std::tuple<int, int>>("init_task",
            [this](const std::tuple<int, int>& data) {
                int bank_id = std::get<0>(data);
                int batch_num = std::get<1>(data);
                this->init_pending_tasks(bank_id, batch_num);
            });
    }

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

    // 根据写入的batch数量初始化SRAM可用数据容量，初始化待调度任务队列
    void init_pending_tasks(int bank_id, int batch_num) {
        if (bank_id < 0 || bank_id >= L1C_BANK || batch_num <= 0 ||
            task_list_[bank_id].block_num <= 0) {
            throw std::invalid_argument("init_task requires an active bank and positive batch count");
        }
        // This legacy interface admits a stream of batches, not preloaded SRAM contents.
        // Preserve already admitted work when another transaction arrives.
        if (batch_num > std::numeric_limits<int>::max() - batch_data_info[bank_id].valid_batch_num)
            throw std::overflow_error("Too many pending batches");
        batch_data_info[bank_id].valid_batch_num += batch_num;
        admitted_batches_ += batch_num;
        for (int i = 0; i < batch_num; ++i) {
            pending_tasks_.push(bank_id);
        }
        // std::cout << "task num =" << pending_tasks_.size() << std::endl;
    }

    // 根据当前进程任务计数判断是否满足taskqueue调度条件，返回批次
    void update_pending_tasks(int bank_id) {
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

    // 每个xbar维护自己当前的任务计数器
    std::array<TaskCounter, L1C_BANK> task_counters_{}; // 任务计数器，记录每个bank当前执行的任务状态
    std::array<FmapTask, L1C_BANK> task_list_; // 整体任务信息
    std::array<BatchInfo, L1C_BANK> batch_data_info{}; // 当前L1C有效数据批数
    std::array<bool, L1C_BANK> task_finish_flags_{}; // 各bank推理任务是否完成
    uint64_t admitted_batches_ = 0;
    uint64_t completed_batches_ = 0;
    bool core_task_finish_flag_ = false; // 核心计算任务完成标志
    // 通过ID和当前任务计数器的状态，判断task执行的读操作参数
    std::queue<int> pending_tasks_; // 存储待调度的任务，元素可以是任务ID或者其他标识信息
    int current_task_id_ = -1; // 当前正在执行的任务ID
    int last_executed_task_id_ = -1; // 上一个执行的任务ID
    bool switch_pending_ = false;
    std::optional<uint64_t> last_read_step_cycle_;
    TaskStatus current_task_status_ = TaskStatus::IDLE; // 当前任务状态
};

#endif

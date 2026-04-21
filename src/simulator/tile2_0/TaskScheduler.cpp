#include "simulator/tile2_0/TaskScheduler.h"

TaskScheduler::TaskScheduler(const std::string& id, const std::array<FmapTask, L1C_BANK>& task_list) : ModuleBase(id), task_list_(task_list) {
    // 写两列的任务触发请求，该请求受多核反压机制控制，有效时一定允许执行。
    add_signal(Signal("batch_wr_bank", Signal::Direction::INPUT)); // int, batch写入的bank id
    // 这个信号被wtask的cache_write_done替代了。
    // add_signal(Signal("batch_wr_done", Signal::Direction::OUTPUT, false)); // bool, batch写入完成信号
    // L1C接口
    add_signal(Signal("cache_read_trigger", Signal::Direction::OUTPUT)); // int, bank id
    add_signal(Signal("cache_read_len", Signal::Direction::OUTPUT)); // int, valid_lines
    add_signal(Signal("cache_write_trigger", Signal::Direction::OUTPUT)); // int, bank id
    add_signal(Signal("cache_write_len", Signal::Direction::OUTPUT)); // int sram lines to write
    add_signal(Signal("cache_read_valid", Signal::Direction::INPUT)); // bool, 读有效信号
    add_signal(Signal("cache_write_done", Signal::Direction::INPUT)); // bool, 写响应信号
    // xbar接口，当前通过TS等待XBAR的计算完成再发送下一次来实现控制依赖，不对ready建模
    add_signal(Signal("xbar_computation_trigger", Signal::Direction::OUTPUT)); // bool
    add_signal(Signal("xbar_switching_trigger", Signal::Direction::OUTPUT)); // int, target xbar id
    add_signal(Signal("xbar_switching_done", Signal::Direction::INPUT, false)); // bool
    // SIMD接口，当前通过TS等待SIMD的计算完成再发送下一次来实现控制依赖，不对ready建模
    add_signal(Signal("pooling_enabled", Signal::Direction::OUTPUT, false)); // bool
    add_signal(Signal("SIMD_computation_done", Signal::Direction::INPUT)); // int, output channel num
    // 内部信号
    add_signal(Signal("current_task_id", Signal::Direction::INTERNAL)); // int, 当前执行的任务ID
    add_signal(Signal("switching_process", Signal::Direction::INTERNAL)); // bool，是否正在切换
    add_signal(Signal("computation_process", Signal::Direction::INTERNAL)); // bool，是否正在计算

    // 初始化任务计数
    // 这个任务计数没有考虑容量问题，
    for (int i = 0; i < L1C_BANK; i++) {
        // 初始化各bank完成标志，如果没有任务，默认完成
        task_finish_flags_[i] = (task_list_[i].block_num == 0); // 如果任务块数为0，表示没有任务，默认完成
        if (task_list_[i].block_num > 0) {
            std::cout << "Initialized TaskScheduler for bank " << i 
                      << ": block_num=" << task_list_[i].block_num 
                      << ", row=" << task_list_[i].row 
                      << ", col=" << task_list_[i].col 
                      << ", channel_num=" << task_list_[i].channel_num 
                      << ", pooling=" << task_list_[i].pooling 
                      << std::endl;
        }
        else {
            continue;
        }
        // task_counters_[i] = {0, 0}; // 读写任务
        // task_counters_[i] = {0, 0, 0, 0}; // 读写任务的pt和batch计数
        // 根据fmap_task信息初始化batch_data_info，两列的总点数除以16
        int batch_pts = 2 * task_list_[i].row; // 每批次的容量(Bytes)
        task_counters_[i].pt_num = batch_pts;
        int batch_num = (task_list_[i].col + 1) / 2; // 每个bank的批次数，向上取整
        task_counters_[i].batch_num = batch_num;
        int batch_bytes = batch_pts * task_list_[i].channel_num; // 每批次的容量(Bytes)
        // 一个batch占多少行SRAM，向上取整
        int batch_lines = (batch_bytes + L1C_SRAM_LINE_BYTES - 1) / L1C_SRAM_LINE_BYTES;
        batch_data_info[i].batch_lines = batch_lines;
        // batch容量=SRAM深度除以每个batch占的行数，向下取整，乘以每个bank3个SRAM
        batch_data_info[i].max_batch_capacity = L1C_SRAM_DEPTH / batch_lines * 3;
        // batch容量至少为3，否则无法支持两列交替读写，抛出异常
        // 目前不对写指针的机制进行建模，由于写是逐SRAMbatch顺序写，而读是顺序读出写入的数据
        // 因此只要计数足够，进行相应加减就行了
        // 3*3卷积，batch读写各自释放/占用一个batch容量，交替使用，至少需要3个batch容量才能保证不冲突
        if (batch_data_info[i].max_batch_capacity < 3) {
            throw std::runtime_error("Invalid task configuration for bank " + std::to_string(i) + ": batch capacity is zero");
        }
    }
}

// 重置有效请求的条件由外部模块实现
bool TaskScheduler::check_wtask_trigger() {
    auto batch_wr_bank_val = get_signal_value("batch_wr_bank");
    if (batch_wr_bank_val.has_value()) {
        int bank_id = std::any_cast<int>(batch_wr_bank_val);
        // 只有当对应bank的写任务未完成且有数据要写时，才触发写任务
        // 当前触发有效时，一定可以执行，如果不满足条件，说明实现有问题
        if (bank_id >= 0 && bank_id < L1C_BANK) {
            return !task_finish_flags_[bank_id] 
            && batch_data_info[bank_id].valid_batch_num < batch_data_info[bank_id].max_batch_capacity;
        }
        else {
            throw std::runtime_error("Invalid bank ID in batch_wr_bank signal: " + std::to_string(bank_id));
        }
    }
    return false;
}

// 一次仅触发一个batch的写任务。
// 由于写任务在架构中是原子化的批处理操作，因此只考虑整体写入延迟，暂时不实现单个点的写入。
bool TaskScheduler::check_wtask_exec() {
    auto batch_wr_bank_val = get_signal_value("batch_wr_bank");
    int bank_id = std::any_cast<int>(batch_wr_bank_val);
    auto batch_lines_val = batch_data_info[bank_id].batch_lines;
    // 提交写任务执行，写任务的长度由当前有效数据量决定
    raise_sram_wr_req(bank_id, batch_lines_val); // 触发写任务
    // submit_signal_value("cache_write_trigger", bank_id, 1); // 触发写任务
    // submit_signal_value("cache_write_len", batch_lines_val, 1); // 写任务长度
    return true;
}

// 可以在这里添加多核随机延迟，以简化建模实现
bool TaskScheduler::check_wtask_finish() {
    // 在这里触发多核反压的更新
    auto write_done_val = get_signal_value("cache_write_done");
    if(write_done_val.has_value() && std::any_cast<bool>(write_done_val)) {
        auto batch_wr_bank_val = get_signal_value("batch_wr_bank");
        int bank_id = std::any_cast<int>(batch_wr_bank_val);
        // 写任务完成后，更新对应bank的任务状态和有效数据量
        batch_data_info[bank_id].valid_batch_num += 1; // 写入一个batch的数据
        update_pending_tasks(bank_id); // 更新待调度任务
        return true;
    }
    return false;
}

bool TaskScheduler::check_wtask_end() {
    invalidate_sram_wr_req(); // 重置写请求信号
    // submit_signal_value("cache_write_trigger", {}, 1); // 重置写任务触发信号
    // submit_signal_value("cache_write_len", {}, 1); // 重置写任务长度
    return true;
}

bool TaskScheduler::check_rtask_trigger() {
    // 触发条件：当前bank的读任务未完成且有数据可读
    // 目前没有对切换任务进行建模。
    return !pending_tasks_.empty();
}
// 这里读任务只是开启一批读数据，但具体的读操作还是要分批读取，在MVM kernel计算完成以后才允许读下一批数据。
// 在这里判断是否需要触发切换
bool TaskScheduler::check_rtask_exec() {
    current_task_id_ = pending_tasks_.front();
    if (current_task_id_ != last_executed_task_id_) {
        // 触发切换任务
        raise_switch_task_trigger();
        last_executed_task_id_ = current_task_id_;
        return false; // 切换任务优先级高于读任务，先执行切换任务
    }
    // 如果当前处在切换状态中，则等待切换完成后再执行读任务
    auto switching_process_val = get_signal_value("switching_process");
    if (switching_process_val.has_value() && std::any_cast<bool>(switching_process_val)) {
        auto switch_done_val = get_signal_value("xbar_switching_done");
        if (switch_done_val.has_value() && std::any_cast<bool>(switch_done_val)) {
            invalidate_switch_task_trigger(); // 切换完成，重置切换触发信号
            return true; // 切换完成，可以执行读任务
        }
        return false; // 等待切换完成
    }
    // 提交读任务执行，读任务的长度由当前有效数据量决定
    return true;
}
// 握手
bool TaskScheduler::check_rtask_finish() {
    switch (current_task_status_) {
        // 没有正在进行的读任务，触发读任务。
        case TaskStatus::IDLE: {
            int rd_pts = task_counters_[current_task_id_].pt_cnt == 0 ? 9 : 3;
            int pt_channel = task_list_[current_task_id_].channel_num; // 每个batch的通道数
            int pt_lines = (rd_pts * pt_channel + L1C_SRAM_LINE_BYTES - 1) / L1C_SRAM_LINE_BYTES;
            raise_sram_rd_req(current_task_id_, pt_lines); // 触发读任务
            current_task_status_ = TaskStatus::READ_DATA;
            return false; // 还未完成
        }
        // SRAM在检测到done信号的周期完成，并移出活跃事件队列当中，下一个周期done信号重置
        // 在TS逻辑里，在检测到done信号的周期进入计算，并且在下一个周期重置读请求
        // 因此下一个周期检测trigger条件的时候不会被触发，符合握手语义
        case TaskStatus::READ_DATA: {
            auto read_valid_val = get_signal_value("cache_read_valid");
            if (read_valid_val.has_value() && std::any_cast<bool>(read_valid_val)) {
                // 读数据准备好了，触发计算
                // submit_signal_value("xbar_computation_trigger", true, 1); // 触发计算
                raise_xbar_computation_trigger();
                invalidate_sram_rd_req(); // 读请求完成，重置读请求信号
                current_task_status_ = TaskStatus::COMPUTE;
            }
            return false; // 还未完成
        }
        // 但是这里有所区别，由于crossbar的计算和这里的等待是并行的，且不存在握手信号
        // 并且这里等待的是和SIMD的握手，因此信号到来会和xbar的计算完成不同步，会产生问题
        // 最保险的建模方法是握手以后就拉低请求，对无阻塞的计算触发信号应当立刻拉低
        case TaskStatus::COMPUTE: {
            invalidate_xbar_computation_trigger(); // 重置计算触发信号
            auto compute_done_val = get_signal_value("SIMD_computation_done");
            if (compute_done_val.has_value() && std::any_cast<bool>(compute_done_val)) {
                // 计算完成，任务完成
                
                current_task_status_ = TaskStatus::IDLE;
                bool task_finished = task_counters_[current_task_id_].step(); // 更新任务计数器，并判断是否完成
                task_finish_flags_[current_task_id_] = task_finished; // 更新任务完成标志
                // if (task_finished) {
                //     std::cout << "Task " << current_task_id_ << " completed!" << std::endl;
                // }
                return task_finished; // 返回任务是否完成
            }
            return false; // 还未完成
        }
        default:
            throw std::runtime_error("Invalid task status");
    }
}
// 释放资源
bool TaskScheduler::check_rtask_end() {
    // 在这里触发多核反压的更新，粒度以batch为单位就可以了。
    submit_message("task_batch_done", std::make_tuple(current_task_id_, id_), 1); // 任务完成后发送消息通知外部模块，携带当前bank id和有效batch数量
    batch_data_info[current_task_id_].valid_batch_num -= 1; // 读出一个batch的数据
    pending_tasks_.pop(); // 移除已完成的任务
    return true;
}
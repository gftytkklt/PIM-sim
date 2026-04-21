#ifndef TILE2_0MODULES_H
#define TILE2_0MODULES_H

// 建模原则：
// 1. 每个模块都继承自ModuleBase，利用其提供的信号管理和进程管理功能。
// 2. 每个模块在构造函数中定义自己的输入输出信号，并在register_processes方法中注册自己的进程。
// 3. 进程的触发条件、执行逻辑、完成条件和结束条件由模块自己实现，利用ModuleBase提供的信号接口进行通信。
// 4. 模块之间通过连接信号进行通信，连接关系由Simulator管理，模块只需要关注自己的信号和进程逻辑。
// 5. 每个模块可以维护自己的性能统计数据，并通过get_module_specific_stats接口提供给Simulator汇总。
// 6. 模块内部可以使用私有成员变量来维护状态，例如TaskScheduler的任务计数器和批次信息，SIMD的计算状态和延迟等。
// 7. 发起进程请求在exec函数中进行，完成条件在finish函数中进行检查，结束条件在end函数中进行处理，符合握手语义。
// 8. 使用raise_process和invalidate_process接口来管理进程状态，避免直接操作信号值，保持模块内部逻辑清晰。
// 9. 如果一个进程发起模块A的执行，但通过模块B的信号判断完成条件，A驱动信号的发起和撤销应以A的握手为准（例如TS发起xbar计算，但和SIMD握手）

#include "TaskScheduler.h"
#include "Crossbar.h"
#include "SIMD.h"
#include "L1C.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"

// // 维护逻辑是，total_batch_num维护一个分块的总批次数，issued_num是每个分块内已经发起的读请求数
// // valid_batch_num只维护当前有多少有效数据，该数据根据读写情况进行更新
// // 这里假设写入的数据一定是下一次计算需要的
// // 另外，这里只维护有效数据信息，不维护全局任务信息。
// struct BankStatus {
//     int valid_batch_num; // 当前有多少组有效数据
//     int batch_capacity; // L1C中每个bank的批次容量，单位是batch
//     int issued_read_batch_num; // 已经发起读请求但还未完成的批次数
//     // 每次只会写一个batch
//     bool can_issue_write(int required_batch_num) const {
//         return valid_batch_num + required_batch_num <= batch_capacity;
//     }
//     // 第一批计算读两个batch，后续都是一个
//     bool can_issue_read(int required_batch_num) const {
//         return valid_batch_num >= required_batch_num;
//     }
//     void write_batch(int batch_num) {
//         valid_batch_num += batch_num;
//     }
//     void read_batch(int batch_num) {
//         issued_read_batch_num += batch_num;
//         valid_batch_num -= batch_num;
//     }
// };
// // bank整体task情况。
// // 当前需要手动建模层融合的计数机制
// // 即前级发射多少次，才生成一批后级有效数据。
// // 简化处理：两种情况，pooling的时候只有前后级统一blk大小和两倍两种情况。
// // 前者要算4个block才能生成对应batch的数据
// // 直接在满足条件的时候往pending_tasks里丢任务就行了
// struct BankTask {
//     int block_num;
//     int block_row;
//     int block_col;
//     bool pooling;
// };

// using BankID = std::pair<std::string, int>; // <core_name, bank_id>
// struct BankIDHash {
//     std::size_t operator()(const BankID& bank_id) const {
//         return std::hash<std::string>{}(bank_id.first) ^ (std::hash<int>{}(bank_id.second) << 1);
//     }
// };
// using BankStatusMap = std::unordered_map<BankID, BankStatus, BankIDHash>;
// using TaskPair = std::unordered_map<BankID, BankID, BankIDHash>; // <读任务bank, 写任务bank>

class OPUSimulator : public CycleAccurateSimulator {
public:
    // ctor，两段初始化都要做。
    OPUSimulator(uint64_t max_cycles = 100000) : CycleAccurateSimulator(max_cycles){}
    void Init();
    // 模块到模拟器的消息处理函数
    void handle_simd_computation_done(const GenericMessage& msg);
    void handle_batch_task_done(const GenericMessage& msg);
    // 模拟器到模块的消息分发函数
    // 任务队列初始化，模拟计算开始时L1C已有部分数据。
    void init_task(int bank_id, int batch_num);
// private:
//     BankStatusMap bank_status_map_;
//     TaskPair task_pair_;
};

#endif
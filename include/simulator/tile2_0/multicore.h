#ifndef MULTICORE_H
#define MULTICORE_H

// AlexNet 8x8分块模拟，模块信息进行硬编码。

#include "TaskScheduler.h"
#include "Crossbar.h"
#include "SIMD.h"
#include "L1C.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"
#include "simulator/TaskDependency.h"


using CoreBank = std::pair<std::string, int>; // pair<core_name, bank_id>
// 需要自定义hash函数来支持unordered_map以CoreBank为键
struct CoreBankHash {
    std::size_t operator()(const CoreBank& cb) const {
        // 组合字符串和整数的哈希值
        auto h1 = std::hash<std::string>{}(cb.first);
        auto h2 = std::hash<int>{}(cb.second);
        
        // 使用位运算组合哈希值
        return h1 ^ (h2 << 1);
    }
};
class MulticoreSimulator : public CycleAccurateSimulator {
public:    
    // ctor，两段初始化都要做。
    MulticoreSimulator(uint64_t max_cycles = 100000) : CycleAccurateSimulator(max_cycles){}
    void Init();
    void init_task(int core_id, int bank_id, int batch_num) {
        std::string core_name = "task_scheduler" + std::to_string(core_id); // 根据core_id确定对应的任务调度器核心名称
        int local_bank_id = bank_id; // 计算本地bank id，假设每个调度器管理L1C_BANK个bank
        send_message_to_core(core_name, GenericMessage("init_task", std::make_tuple(local_bank_id, batch_num), 0));
    }
    void handle_batch_task_done(const std::tuple<int, std::string>& data);
    // 构造一个生产-消费屏障表项（生产者/条件/消费者均来自该模拟器的 init_task 与计数）
    // producers: {core_name, bank_id, threshold} 每个生产者携带各自计数阈值
    // targets:   {core_id, bank_id, batch_num} 消费者事务列表
    TaskDependencyEntryPtr make_dependency(
        const std::string& name,
        std::vector<std::tuple<std::string, int, int>> producers,
        std::vector<std::tuple<int, int, int>> targets);
private:
    bool ideal_= false; // 是否理想化模拟，理想化模拟遵循第四章的流水线，和tile2.0进行比较。
};

#endif
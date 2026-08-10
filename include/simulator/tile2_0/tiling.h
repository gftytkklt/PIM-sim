#ifndef TILING_H
#define TILING_H

#include "TaskScheduler.h"
#include "Crossbar.h"
#include "SIMD.h"
#include "L1C.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"

class TilingSimulator : public CycleAccurateSimulator {
public:    // ctor，两段初始化都要做。
    TilingSimulator(uint64_t max_cycles = 100000) : CycleAccurateSimulator(max_cycles){}
    void Init();
    // 模块到模拟器的消息处理函数
    // 直接写死就行了
    void init_task();
    void handle_batch_task_done(const std::tuple<int, std::string>& data);
private:
    std::unordered_map<std::string, int> core_batch_num_map_; // 记录每个核心当前处理的batch数量
    bool dynamic_banking = true; // 是否启用动态分块策略
};

#endif
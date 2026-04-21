#ifndef MEMBANKING_H
#define MEMBANKING_H

// 这个用来跑16x16-8x8-4x4的模拟，模块信息进行硬编码。

#include "TaskScheduler.h"
#include "Crossbar.h"
#include "SIMD.h"
#include "L1C.h"
#include "simulator/ModuleBase.h"
#include "simulator/Simulator.h"

class BankingSimulator : public CycleAccurateSimulator {
public:
    enum class BlockStrategy {
        XY,
        YX,
        Custom,
        NoTiling,
    };
    // ctor，两段初始化都要做。
    BankingSimulator(uint64_t max_cycles = 100000) : CycleAccurateSimulator(max_cycles){}
    void Init();
    // 模块到模拟器的消息处理函数，在此例子中，应该是写死的0，0，16
    void init_task(int core_id, int bank_id, int batch_num);
    void handle_batch_task_done(const GenericMessage& msg);
private:
    std::unordered_map<std::string, int> core_batch_num_map_; // 记录每个核心当前处理的batch数量
    BlockStrategy block_strategy_ = BlockStrategy::Custom; // 默认的分块策略
};

#endif
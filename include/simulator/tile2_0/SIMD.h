#ifndef SIMD_H
#define SIMD_H

#include "config.h"

// quant -> activate -> pooling
class SIMD : public ModuleBase<SIMD> {
public:
    SIMD(const std::string& id);
    void register_processes() override final{
        register_process("SIMD_computation",
            [this]() { return check_computation_trigger(); },
            [this]() { return check_computation_exec(); },
            [this]() { return check_computation_finish(); },
            [this]() { return check_computation_end(); },
            compute_latency_
        );
    }
    void register_message_handlers() override final {
        // SIMD目前没有需要处理的消息，可以留空或者添加一些调试消息的处理函数。
    }
private:
    bool check_computation_trigger();
    bool check_computation_exec();
    bool check_computation_finish();
    bool check_computation_end();

    bool enable_pipeline_ = true;
    int compute_latency_;
    int channel_num_ = SIMD_NUM;
    const int quant_latency_ = SIMD_QUANT_LATENCY;
    const int activate_latency_ = SIMD_ACTIVATE_LATENCY;
    int pooling_latency_ = SIMD_POOLING_LATENCY;
};

#endif
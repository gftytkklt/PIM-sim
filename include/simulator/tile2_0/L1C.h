#ifndef L1C_H
#define L1C_H

#include "simulator/tile2_0/config.h"

struct SRAM {
    int linewidth; // bit
    int depth;     // number of lines
};

class L1C : public ModuleBase {
public:
    L1C(const std::string& id);
    void register_processes() override final {
        register_process("Cache_Read",
            [this]() { return check_cache_read_trigger(); },
            [this]() { return check_cache_read_exec(); },
            [this]() { return check_cache_read_finish(); },
            [this]() { return check_cache_read_end(); },
            1 // latency will be determined by the number of lines accessed
        );
        register_process("Cache_Write",
            [this]() { return check_cache_write_trigger(); },
            [this]() { return check_cache_write_exec(); },
            [this]() { return check_cache_write_finish(); },
            [this]() { return check_cache_write_end(); },
            1 // latency will be determined by the number of lines accessed
        );
    }
    void register_message_handlers() override final {
        // L1C目前没有需要处理的消息，可以留空或者添加一些调试消息的处理函数。
    }
private:
    bool check_cache_read_trigger();
    bool check_cache_read_exec();
    bool check_cache_read_finish();
    bool check_cache_read_end();

    bool check_cache_write_trigger();
    bool check_cache_write_exec();
    bool check_cache_write_finish();
    bool check_cache_write_end();

    void raise_sram_rd_process(int valid_lines) {
        // submit_signal_value("cache_read_process", true, 1); // 提交读进程信号
        submit_signal_value("cache_read_done", true, valid_lines); // 提交读完成信号，携带访问的行数信息
    }

    void invalidate_sram_rd_process() {
        // submit_signal_value("cache_read_process", {}, 1); // 重置读进程信号
        submit_signal_value("cache_read_done", {}, 1); // 重置读完成信号
    }

    void raise_sram_wresp(int valid_lines) {
        submit_signal_value("cache_write_done", true, valid_lines); // 提交写完成信号，携带写入的行数信息
    }

    void invalidate_sram_wresp() {
        submit_signal_value("cache_write_done", {}, 1); // 重置写完成信号
    }

    // SRAM组织形式
    std::array<SRAM, L1C_SRAM_NUM> srams_;

    // 性能统计
    std::array<uint64_t, L1C_BANK> total_accesses_ = {};
    
    SRAM sram_;
};

#endif // L1C_H
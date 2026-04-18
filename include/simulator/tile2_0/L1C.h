#ifndef L1C_H
#define L1C_H

#include "simulator/tile2_0/config.h"

struct SRAM {
    int linewidth; // bit
    int depth;     // number of lines
};

class L1C : public ModuleBase<L1C> {
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
private:
    bool check_cache_read_trigger();
    bool check_cache_read_exec();
    bool check_cache_read_finish();
    bool check_cache_read_end();

    bool check_cache_write_trigger();
    bool check_cache_write_exec();
    bool check_cache_write_finish();
    bool check_cache_write_end();

    // SRAM组织形式
    std::array<SRAM, L1C_SRAM_NUM> srams_;

    // 性能统计
    std::array<uint64_t, L1C_BANK> total_accesses_ = {};
    
    SRAM sram_;
};

#endif // L1C_H
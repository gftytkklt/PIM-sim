#ifndef MEMORYUNIT_H
#define MEMORYUNIT_H

#include "ModuleBase.h"
#include <iostream>
#include <unordered_map>

/**
 * 存储器模块示例
 * 对应文档中的存储系统建模
 */
class MemoryUnit : public ModuleBase<MemoryUnit> {
private:
    uint64_t read_latency_;
    uint64_t write_latency_;
    uint64_t read_count_{0};
    uint64_t write_count_{0};
    std::unordered_map<uint64_t, std::any> storage_;
    
public:
    MemoryUnit(const std::string& id, uint64_t read_latency = 2, uint64_t write_latency = 1);
    ~MemoryUnit() override = default;
    
    std::vector<std::shared_ptr<Event>> check_triggers(uint64_t current_cycle) override;
    
    uint64_t get_latency_for_event(const std::shared_ptr<Event>& event) override;
    
    void update_output_signals(const std::shared_ptr<Event>& event, uint64_t current_cycle) override;
    
    std::unordered_map<std::string, uint64_t> get_module_specific_stats() const override;
};

#endif // MEMORYUNIT_H
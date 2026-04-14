#ifndef PROCESSINGUNIT_H
#define PROCESSINGUNIT_H

#include "ModuleBase.h"
#include <iostream>

/**
 * 计算单元模块示例
 * 对应文档中的存算核心体系结构建模
 */
class ProcessingUnit : public ModuleBase<ProcessingUnit> {
private:
    uint64_t compute_latency_;
    uint64_t mac_operations_{0};
    
public:
    ProcessingUnit(const std::string& id, uint64_t compute_latency = 1);
    ~ProcessingUnit() override = default;
    
    // 模块特定的触发检查
    std::vector<std::shared_ptr<Event>> check_triggers(uint64_t current_cycle) override;
    
    uint64_t get_latency_for_event(const std::shared_ptr<Event>& event) override;
    
    void update_output_signals(const std::shared_ptr<Event>& event, uint64_t current_cycle) override;
    
    std::unordered_map<std::string, uint64_t> get_module_specific_stats() const override;
};

#endif // PROCESSINGUNIT_H
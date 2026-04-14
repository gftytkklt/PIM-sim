#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ISimulatable.h"
#include "ModuleBase.h"
#include "Event.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <iostream>

/**
 * 周期精确模拟器
 * 对应算法5.3的完整实现
 */
class CycleAccurateSimulator {
private:
    EventQueue event_queue_;
    std::vector<std::shared_ptr<ISimulatable>> modules_;
    std::vector<std::shared_ptr<ISimulatable>> combinational_modules_; // 组合逻辑模块
    std::unordered_map<std::string, std::shared_ptr<ISimulatable>> module_map_;
    uint64_t current_cycle_{0};
    uint64_t max_cycles_{1000};
    bool simulation_done_{false};
    
    // 性能统计
    struct SimulationStats {
        uint64_t total_cycles{0};
        uint64_t total_events{0};
        uint64_t modules_processed{0};
        std::unordered_map<std::string, std::unordered_map<std::string, uint64_t>> module_stats;
    } stats_;
    
    // 私有方法
    void initialize_simulation();
    void simulate_cycle();
    void process_combinational_logic();
    void process_events_at_cycle(uint64_t cycle);
    void check_simulation_complete();
    void print_statistics() const;
    
public:
    CycleAccurateSimulator(uint64_t max_cycles = 1000);
    ~CycleAccurateSimulator() = default;
    
    // 禁止拷贝
    CycleAccurateSimulator(const CycleAccurateSimulator&) = delete;
    CycleAccurateSimulator& operator=(const CycleAccurateSimulator&) = delete;
    
    // 注册模块
    template<typename ModuleType>
    std::shared_ptr<ModuleType> register_module(const std::string& id, int topological_depth);
    
    // 连接模块
    void connect_modules(const std::string& src_id, const std::string& src_signal,
                        const std::string& dst_id, const std::string& dst_signal);
    
    // 运行模拟
    void run();
    
    // 获取模块（带类型检查）
    template<typename ModuleType>
    std::shared_ptr<ModuleType> get_module(const std::string& id);
    
    // 获取所有模块（类型擦除版本）
    const std::vector<std::shared_ptr<ISimulatable>>& get_all_modules() const;
    
    // 获取当前周期
    uint64_t get_current_cycle() const { return current_cycle_; }
    
    // 检查模拟是否完成
    bool is_simulation_done() const { return simulation_done_; }
};

// 模板方法实现
template<typename ModuleType>
std::shared_ptr<ModuleType> CycleAccurateSimulator::register_module(const std::string& id, int topological_depth) {
    auto module = std::make_shared<ModuleType>(id);
    module->set_topological_depth(topological_depth);
    
    modules_.push_back(module);
    module_map_[id] = module;
    
    // 按拓扑深度排序
    std::sort(modules_.begin(), modules_.end(),
        [](const std::shared_ptr<ISimulatable>& a, 
           const std::shared_ptr<ISimulatable>& b) {
            return a->get_topological_depth() > b->get_topological_depth(); // 降序
        });
    
    return module;
}

template<typename ModuleType>
std::shared_ptr<ModuleType> CycleAccurateSimulator::get_module(const std::string& id) {
    auto it = module_map_.find(id);
    if (it != module_map_.end() && 
        it->second->get_module_type() == typeid(ModuleType)) {
        return std::static_pointer_cast<ModuleType>(it->second);
    }
    return nullptr;
}

#endif // SIMULATOR_H
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ISimulatable.h"
#include "ModuleBase.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <iostream>
#include <queue>

/**
 * 周期精确模拟器
 * 对应算法5.3的完整实现
 */
// struct SignalUpdateEvent {
//     uint64_t cycle;  // 生效周期
//     std::weak_ptr<ISimulatable> source_module;
//     std::string source_signal;
//     std::weak_ptr<ISimulatable> target_module;
//     std::string target_signal;
//     std::any value;
    
//     // 为优先队列定义比较函数
//     bool operator>(const SignalUpdateEvent& other) const {
//         return cycle > other.cycle;  // 最小堆，周期小的优先
//     }
// };

struct SignalUpdateEvent {
    uint64_t cycle;                      // 生效周期
    std::weak_ptr<ISimulatable> module;  // 源模块
    std::string signal_name;             // 信号名
    std::any value;                      // 信号值
    
    // 比较函数，用于优先队列
    bool operator>(const SignalUpdateEvent& other) const {
        return cycle > other.cycle;  // 最小堆
    }
};

class CycleAccurateSimulator {
private:
    // 优先队列，最小堆，按周期排序
    using EventQueue = std::priority_queue<
        SignalUpdateEvent, 
        std::vector<SignalUpdateEvent>,
        std::greater<SignalUpdateEvent>
    >;
    
    EventQueue signal_event_queue_;
    
    // 连接管理映射：源模块信号 -> 目标模块信号列表
    struct ConnectionInfo {
        std::weak_ptr<ISimulatable> target_module;
        std::string target_signal;
    };
    using ConnectionKey = std::pair<std::weak_ptr<ISimulatable>, std::string>;
    
    struct ConnectionKeyHash {
        std::size_t operator()(const ConnectionKey& key) const {
            auto module_ptr = key.first.lock();
            if (!module_ptr) return 0;
            return std::hash<std::string>{}(module_ptr->get_id()) ^ 
                   (std::hash<std::string>{}(key.second) << 1);
        }
    };
    
    struct ConnectionKeyEqual {
        bool operator()(const ConnectionKey& a, const ConnectionKey& b) const {
            auto a_module = a.first.lock();
            auto b_module = b.first.lock();
            if (!a_module || !b_module) return false;
            return a_module->get_id() == b_module->get_id() && 
                   a.second == b.second;
        }
    };
    
    std::unordered_map<
        ConnectionKey, 
        std::vector<ConnectionInfo>,
        ConnectionKeyHash,
        ConnectionKeyEqual
    > connections_map_;
    
    // 私有方法
    void process_signal_events(uint64_t current_cycle);
    void propagate_signal_to_targets(std::shared_ptr<ISimulatable> source_module,
                                    const std::string& source_signal,
                                    const std::any& value,
                                    uint64_t valid_cycle);

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
    void check_simulation_complete();
    void print_statistics() const;
    
public:
    // 新增：提交信号更新事件的公共接口
    void schedule_signal_update(const SignalUpdateEvent& event) {
        signal_event_queue_.push(event);
    }
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

#endif // SIMULATOR_H
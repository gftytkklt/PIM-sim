#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ISimulatable.h"
#include "ModuleBase.h"
#include "MessageBase.h"
#include "SimulatorEvent.h"
#include "ISimulator.h"
#include "TaskDependency.h"
#include <cstdint>
#include <memory>
#include <vector>
#include <unordered_map>
#include <string>
#include <algorithm>
#include <iostream>
#include <queue>

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

// 消息事件
struct MessageEvent {
    uint64_t trigger_cycle;
    GenericMessage message;
    
    bool operator>(const MessageEvent& other) const {
        return trigger_cycle > other.trigger_cycle;
    }
};

/**
 * 周期精确模拟器实现
 * 继承 ISimulator 抽象接口，实现虚方法/虚钩子。
 * 模板方法（register_module/get_module/register_task_handler）
 * 在接口层定义，委托到本类的虚钩子。
 */
class CycleAccurateSimulator : public ISimulator {
private:
    // 优先队列，最小堆，按周期排序
    using EventQueue = std::priority_queue<
        SignalUpdateEvent, 
        std::vector<SignalUpdateEvent>,
        std::greater<SignalUpdateEvent>
    >;
    
    EventQueue signal_event_queue_;
    
    // 连接管理映射：使用 ModuleBase.h 中定义的全局 ConnectionKey/ConnectionInfo 类型
    using SimConnectionInfo = ConnectionInfo;
    using SimConnectionKey = ConnectionKey;
    using SimConnectionKeyHash = ConnectionKeyHash;
    using SimConnectionKeyEqual = ConnectionKeyEqual;
    
    std::unordered_map<
        SimConnectionKey, 
        std::vector<SimConnectionInfo>,
        SimConnectionKeyHash,
        SimConnectionKeyEqual
    > connections_map_;

    // 消息队列
    std::priority_queue<
        MessageEvent,
        std::vector<MessageEvent>,
        std::greater<MessageEvent>
    > message_queue_;
    
    // 任务ID到处理函数的映射
    std::unordered_map<std::string, TaskHandler> task_handlers_;
    // 任务消息体类型登记：task_id → 消息体类型
    std::unordered_map<std::string, std::type_index> task_types_;
    // 多核事务：活跃任务表（生产-消费屏障）
    TaskDependencyTable dependency_table_;
    
    void dispatch_simulator_event(const SimulatorEvent& event);
    void propagate_signal_to_targets(std::shared_ptr<ISimulatable> source_module,
                                    const std::string& source_signal,
                                    const std::any& value,
                                    uint64_t valid_cycle);

    // 信号注册表：模块ID -> (信号名 -> (方向, 值类型))
    struct SignalMeta {
        Signal::Direction direction;
        std::type_index value_type{typeid(void)};
    };
    std::unordered_map<std::string, std::unordered_map<std::string, SignalMeta>> signal_registry_;

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
    
    void initialize_simulation();
    void process_combinational_logic();
    void check_simulation_complete();
    void print_statistics() const;

protected:
    // 引擎钩子（接口声明为纯虚，此处实现；测试可覆盖）
    void simulate_cycle() override;
    void process_message_events(uint64_t current_cycle) override;
    void process_signal_events(uint64_t current_cycle) override;

    // 虚钩子实现
    void register_module_impl(std::shared_ptr<ISimulatable> module,
                              const std::string& id, int topological_depth) override;
    std::shared_ptr<ISimulatable> get_module_impl(const std::string& id) override;
    void register_task_handler_impl(const std::string& task_id, TaskHandler handler) override;
    void register_task_handler_impl(const std::string& task_id, TaskHandler handler,
                                    std::type_index msg_type) override;

public:
    CycleAccurateSimulator(uint64_t max_cycles = 1000);
    ~CycleAccurateSimulator() override = default;
    
    // 禁止拷贝
    CycleAccurateSimulator(const CycleAccurateSimulator&) = delete;
    CycleAccurateSimulator& operator=(const CycleAccurateSimulator&) = delete;

    // 连接模块
    void connect_modules(const std::string& src_id, const std::string& src_signal,
                        const std::string& dst_id, const std::string& dst_signal) override;
    
    // 运行模拟
    void run() override;
    
    // 获取所有模块（类型擦除版本）
    const std::vector<std::shared_ptr<ISimulatable>>& get_all_modules() const override;

    // 提交信号更新事件的公共接口
    void schedule_signal_update(const SignalUpdateEvent& event) override {
        signal_event_queue_.push(event);
    }

    // 发送消息到核心（直发，不经消息队列）
    void send_message_to_core(const std::string& core_id, const GenericMessage& msg) override;
    
    // 获取当前周期
    uint64_t get_current_cycle() const override { return current_cycle_; }
    
    // 检查模拟是否完成
    bool is_simulation_done() const override { return simulation_done_; }

    // dump完成的事件到文件
    void dump_completed_events(const std::string& filename) const override;

    // ========== 多核事务：任务依赖表 ==========

    // 注册生产-消费屏障表项
    void register_task_dependency(TaskDependencyEntryPtr entry) {
        dependency_table_.register_entry(std::move(entry));
    }

    // 某个生产者的任务完成消息到达：驱动所有相关表项状态更新与触发判断
    // 返回触发了消费者事务的表项数
    int on_task_done(const GenericMessage& msg) {
        return dependency_table_.on_task_done(msg);
    }

    // 获取活跃任务表（供测试/配置驱动）
    TaskDependencyTable& get_dependency_table() { return dependency_table_; }
};

#endif // SIMULATOR_H
#ifndef SIMULATOR_H
#define SIMULATOR_H

#include "ISimulatable.h"
#include "ModuleBase.h"
#include "MessageBase.h"
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
    std::string source_core_id;
    GenericMessage message;
    
    bool operator>(const MessageEvent& other) const {
        return trigger_cycle > other.trigger_cycle;
    }
};

// 任务处理器类型
using TaskHandler = std::function<void(const GenericMessage&)>;

class CycleAccurateSimulator : public std::enable_shared_from_this<CycleAccurateSimulator> {
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

    // 消息队列
    std::priority_queue<
        MessageEvent,
        std::vector<MessageEvent>,
        std::greater<MessageEvent>
    > message_queue_;
    
    // 任务ID到处理函数的映射
    std::unordered_map<std::string, TaskHandler> task_handlers_;
    
    // 模块消息处理器映射
    // std::unordered_map<std::string, std::shared_ptr<ModuleBase<void>>> core_handlers_;

    // 私有方法
    void process_message_events(uint64_t current_cycle);
    
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
    CycleAccurateSimulator(uint64_t max_cycles = 1000);
    ~CycleAccurateSimulator() = default;
    
    // 禁止拷贝
    CycleAccurateSimulator(const CycleAccurateSimulator&) = delete;
    CycleAccurateSimulator& operator=(const CycleAccurateSimulator&) = delete;

    // 注册模块
    template<typename ModuleType, typename... Args>
    std::shared_ptr<ModuleType> register_module(const std::string& id, int topological_depth, Args... args);
    
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

    // 注册任务处理器
    template<typename Func>
    void register_task_handler(const std::string& task_id, Func&& handler) {
        task_handlers_[task_id] = std::forward<Func>(handler);
    }

    // 提交信号更新事件的公共接口
    void schedule_signal_update(const SignalUpdateEvent& event) {
        signal_event_queue_.push(event);
    }

    // 发送消息到核心
    void send_message_to_core(const std::string& core_id, const GenericMessage& msg) {
        // auto it = core_handlers_.find(core_id);
        auto it = module_map_.find(core_id);
        if (it != module_map_.end()) {
            it->second->handle_message(msg);
            // 转换为ModuleBase<void>指针并调用handle_message
            // auto module_handler = std::dynamic_pointer_cast<ModuleBase<void>>(it->second);
            // if (module_handler) {
            //     module_handler->handle_message(msg);
            // } else {
            //     throw std::runtime_error("Module found but failed to cast for core: " + core_id);
            //     // std::cerr << "Warning: Core handler found but failed to cast for core: " << core_id << std::endl;
            // }
            // it->second->handle_message(msg);
        } else {
            throw std::runtime_error("No module found for core: " + core_id);
            // std::cerr << "Warning: No handler registered for core: " << core_id << std::endl;
        }
    }

    // 便捷版本：发送消息到核心
    template<typename T>
    void send_message_to_core(const std::string& core_id, 
                             const std::string& task_id,
                             T&& data) {
        send_message_to_core(core_id, GenericMessage(task_id, std::forward<T>(data)));
    }
    
    // 获取当前周期
    uint64_t get_current_cycle() const { return current_cycle_; }
    
    // 检查模拟是否完成
    bool is_simulation_done() const { return simulation_done_; }
};

template<typename ModuleType, typename... Args>
std::shared_ptr<ModuleType> CycleAccurateSimulator::register_module(
    const std::string& id, int topological_depth, Args... args) {
    
    auto module = std::make_shared<ModuleType>(id, std::forward<Args>(args)...);
    module->set_topological_depth(topological_depth);
    
    // 设置信号更新回调
    auto weak_this = std::weak_ptr<CycleAccurateSimulator>(
        std::static_pointer_cast<CycleAccurateSimulator>(shared_from_this())
    );
    
    module->set_schedule_callback([weak_this, module](
        uint64_t valid_cycle, // latency after current cycle
        std::weak_ptr<ISimulatable> source_module,
        const std::string& signal_name,
        const std::any& value) {
        
        if (auto sim = weak_this.lock()) {
            // 将信号更新事件加入队列
            SignalUpdateEvent event;
            event.cycle = valid_cycle + sim->get_current_cycle();
            event.module = source_module;
            event.signal_name = signal_name;
            event.value = value;
            
            sim->signal_event_queue_.push(event);
        }
    });

    //这里设计的语义跟上面一样，都是提交消息事件队列。
    module->set_message_submit_callback([weak_this, module](const GenericMessage& msg) {
        if (auto sim = weak_this.lock()) {
            MessageEvent event;
            event.trigger_cycle = sim->get_current_cycle() + msg.delay_cycles;
            event.source_core_id = module->get_id();
            event.message = msg;
            
            sim->message_queue_.push(event);
        }
    });

    module->register_processes();

    module->register_message_handlers();
    
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
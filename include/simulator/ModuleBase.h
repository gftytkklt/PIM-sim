#ifndef MODULEBASE_H
#define MODULEBASE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <memory>
#include "ISimulatable.h"
#include "Event.h"
#include "Process.h"

// 前向声明
class EventQueue;
struct Event;

/**
 * 信号定义
 */
struct Signal {
    std::string name;
    enum class Direction { INPUT, OUTPUT, INTERNAL, BIDIRECTIONAL } direction;
    bool valid{false};
    uint64_t valid_cycle{0};
    std::any value;
    
    Signal(const std::string& n, Signal::Direction d, const std::any& v = {})
        : name(n), direction(d), value(v) {}
    
    Signal() = default;
};

/**
 * 模块基类模板
 * 实现ISimulatable接口，同时保持具体模块类型的类型安全
 */
template <typename DerivedModule>
class ModuleBase : public ISimulatable, 
                   public std::enable_shared_from_this<DerivedModule> {
protected:
    std::string id_;
    int topological_depth_{0};
    bool available_{true};
    
    // 信号管理
    std::unordered_map<std::string, Signal> signals_;
    
    // 连接管理
    struct Connection {
        std::string local_signal;
        std::weak_ptr<ISimulatable> target_module;
        std::string target_signal;
    };
    std::vector<Connection> connections_;

    //进程管理
    std::unique_ptr<ProcessManager> process_manager_;
    // 信号到进程类型的映射（用于快速检查哪些进程受信号影响）
    std::unordered_map<std::string, std::vector<std::string>> signal_to_processes_;
    
    // 性能统计
    mutable std::unordered_map<std::string, uint64_t> performance_stats_;
    
public:
    ModuleBase(const std::string& id) : id_(id) {
        // 初始化默认性能统计
        performance_stats_["total_cycles"] = 0;
        performance_stats_["events_processed"] = 0;
        performance_stats_["busy_cycles"] = 0;
        process_manager_ = std::make_unique<ProcessManager>();
    }
    
    virtual ~ModuleBase() = default;
    
    // ========== ISimulatable接口实现 ==========
    
    const std::string& get_id() const override { 
        return id_; 
    }
    
    std::type_index get_module_type() const override { 
        return typeid(DerivedModule); 
    }
    
    int get_topological_depth() const override { 
        return topological_depth_; 
    }
    
    void set_topological_depth(int depth) override { 
        topological_depth_ = depth; 
    }
    
    bool is_available() const override { 
        return available_; 
    }
    
    void set_available(bool available) override { 
        available_ = available; 
    }
    
    bool has_signal(const std::string& name) const override {
        return signals_.find(name) != signals_.end();
    }
    
    std::any get_signal_value(const std::string& name) const override {
        auto it = signals_.find(name);
        if (it != signals_.end() && it->second.valid) {
            return it->second.value;
        }
        return {};
    }
    
    void set_signal_value(const std::string& name, 
                         const std::any& value, 
                         uint64_t valid_cycle) override {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.value = value;
            it->second.valid = true;
            it->second.valid_cycle = valid_cycle;
            
            // 记录信号更新事件
            performance_stats_["signal_updates"]++;
        }
    }
    
    void connect_to(const std::string& local_signal,
                   std::shared_ptr<ISimulatable> target_module,
                   const std::string& target_signal) override {
        connections_.push_back({local_signal, target_module, target_signal});
    }
    
    bool evaluate(uint64_t current_cycle, EventQueue& event_queue) override;
    
    void on_event_finished(std::shared_ptr<Event> event, 
                          uint64_t current_cycle,
                          EventQueue& event_queue) override;
    
    void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const override {
        stats = performance_stats_;
        
        // 添加模块特定统计
        auto derived = static_cast<const DerivedModule*>(this);
        auto module_stats = derived->get_module_specific_stats();
        stats.insert(module_stats.begin(), module_stats.end());
    }

    // 进程管理接口
    bool register_process(const std::string& name,
                         std::function<bool()> trigger_cond,
                         std::function<bool()> exec_cond,
                         std::function<bool()> finish_cond,
                         std::function<bool()> end_cond,
                         uint64_t latency = 1) {
        return process_manager_->register_process_type(name, trigger_cond, exec_cond, 
                                                      finish_cond, end_cond, latency);
    }

    // 绑定信号到进程（用于优化性能）
    void bind_signal_to_process(const std::string& signal_name, 
                               const std::string& process_name) {
        signal_to_processes_[signal_name].push_back(process_name);
    }
    
    // 获取受信号影响的进程列表
    const std::vector<std::string>& get_processes_by_signal(const std::string& signal_name) const {
        static const std::vector<std::string> empty_list;
        auto it = signal_to_processes_.find(signal_name);
        if (it != signal_to_processes_.end()) {
            return it->second;
        }
        return empty_list;
    }

    
    
    // 获取进程性能统计
    std::unordered_map<std::string, uint64_t> get_process_stats() const {
        return process_manager_->get_performance_stats();
    }
    
    
    
protected:
    // 信号传播
    void propagate_signals(uint64_t current_cycle, EventQueue& event_queue);
    
    // 添加信号
    void add_signal(const Signal& signal) {
        signals_[signal.name] = signal;
    }
    
    // 获取信号
    Signal& get_signal(const std::string& name) {
        return signals_.at(name);
    }
    
    const Signal& get_signal(const std::string& name) const {
        return signals_.at(name);
    }

    // 派生类可访问的进程管理器
    ProcessManager* get_process_manager() { return process_manager_.get(); }
    const ProcessManager* get_process_manager() const { return process_manager_.get(); }
    // 检查并触发新事件
    virtual void check_and_trigger_events(uint64_t current_cycle) {
        // 获取所有注册的进程类型
        // 这里需要派生类实现具体的进程检查逻辑
        // 可以在派生类中重写此方法，或者通过注册的回调函数来实现
    }
    
    // 更新事件状态
    virtual void update_event_states(uint64_t current_cycle) {
        process_manager_->update_event_states(current_cycle);
        process_manager_->cleanup_ended_events();
    }
    // 在check_triggers中集成进程管理
    std::vector<std::shared_ptr<Event>> check_triggers(uint64_t current_cycle) override {
        std::vector<std::shared_ptr<Event>> events;
        
        // 步骤1: 检查并触发新事件
        check_and_trigger_events(current_cycle);
        
        // 步骤2: 更新现有事件状态
        update_event_states(current_cycle);
        
        // 步骤3: 根据事件状态生成输出信号
        // 这部分由派生类实现
        
        return events;
    }
    
    // 具体模块需要实现的接口
    // virtual std::vector<std::shared_ptr<Event>> check_triggers(uint64_t current_cycle) = 0;
    // virtual uint64_t get_latency_for_event(const std::shared_ptr<Event>& event) = 0;
    // virtual void update_output_signals(const std::shared_ptr<Event>& event, uint64_t current_cycle) = 0;
    virtual std::unordered_map<std::string, uint64_t> get_module_specific_stats() const = 0;
};

// 模板类的方法实现放在同一个头文件中
// 还是通过有效信号的检查来触发事件比较好，因为事件执行不一定影响后续事件的准备
// 例如计算的流水重叠，可以在计算的过程中继续访存，或者是计算的流水化。
// 但是有效事件的释放还是要在事件finish的时刻进行判断，这样前级模块才能读到对应的值。
template <typename DerivedModule>
bool ModuleBase<DerivedModule>::evaluate(uint64_t current_cycle, EventQueue& event_queue) {
    // 更新模块状态
    performance_stats_["total_cycles"]++;
    
    // 检查模块是否可用
    if (!available_) {
        return false;
    }
    
    // 调用具体模块的触发检查
    auto derived = static_cast<DerivedModule*>(this);
    auto events = derived->check_triggers(current_cycle);
    
    // 这里实际上有两重事件：一个是模块返回的相关事件入队，还有一个是所谓的事件完成传值事件入队
    // 那这里其实是没问题的，completion event就是在当前触发，finish cycle传值
    // 但是end cycle真正释放事件的机制还是没有完成。
    for (auto& event : events) {
        if (event->state == Event::State::PENDING) {
            // 设置事件属性
            event->state = Event::State::EXECUTING;
            event->exec_cycle = current_cycle;
            event->source_module = this->shared_from_this();
            
            // 计算完成时间
            uint64_t latency = derived->get_latency_for_event(event);
            event->finish_cycle = current_cycle + latency;
            event->end_cycle = event->finish_cycle; // 简化处理
            
            // 插入事件队列
            event_queue.push(event);
            
            // 更新模块状态
            available_ = false;
            performance_stats_["events_processed"]++;
            
            auto completion_event = std::make_shared<Event>();
            completion_event->trigger_cycle = current_cycle;
            completion_event->exec_cycle = event->finish_cycle;
            completion_event->finish_cycle = event->finish_cycle;
            completion_event->end_cycle = event->finish_cycle;
            completion_event->state = Event::State::PENDING;
            completion_event->type = Event::Type::MODULE_EVALUATE;
            completion_event->source_module = this->shared_from_this();
            completion_event->action = [this, event, current_cycle, &event_queue]() {
                this->on_event_finished(event, current_cycle, event_queue);
            };
            event_queue.push(completion_event);
        }
    }
    
    return !events.empty();
}


// 实际上这个函数应该只进行传值，并且应该在cur_cycle+latency传递。
// 信号的无效化可以在这里操作，利用当前event的源-目的模块指针实现
// 理论上，finish time应该完成值的传递，end time完成值的接收和状态更新
// 可以参考这个函数实现on event end，注册对应的行为，从而实现模块状态的释放。
template <typename DerivedModule>
void ModuleBase<DerivedModule>::on_event_finished(std::shared_ptr<Event> event, 
                                                 uint64_t current_cycle,
                                                 EventQueue& event_queue) {
    // 更新模块状态
    available_ = true;
    performance_stats_["busy_cycles"] += (event->finish_cycle - event->exec_cycle);
    
    // 调用具体模块的输出更新
    auto derived = static_cast<DerivedModule*>(this);
    derived->update_output_signals(event, current_cycle);
    
    // 传播信号到连接模块
    propagate_signals(current_cycle, event_queue);
}


// 事件类型的检查可以通过检查latency来确定，如果为0就强制定义为组合逻辑事件。
template <typename DerivedModule>
void ModuleBase<DerivedModule>::propagate_signals(uint64_t current_cycle, EventQueue& event_queue) {
    for (const auto& conn : connections_) {
        auto target = conn.target_module.lock();
        if (target && has_signal(conn.local_signal)) {
            auto& signal = signals_.at(conn.local_signal);
            if (signal.valid && signal.valid_cycle <= current_cycle) {
                // 创建信号传播事件
                auto sig_event = std::make_shared<Event>();
                sig_event->trigger_cycle = current_cycle;
                sig_event->exec_cycle = current_cycle; // 立即传播
                sig_event->finish_cycle = current_cycle;
                sig_event->end_cycle = current_cycle;
                sig_event->state = Event::State::PENDING;
                sig_event->type = Event::Type::SIGNAL_PROPAGATION;
                sig_event->source_module = this->shared_from_this();
                sig_event->target_module = target;
                sig_event->signal_name = conn.target_signal;
                sig_event->signal_value = signal.value;
                sig_event->action = [target, signal_name = conn.target_signal, 
                                    value = signal.value, valid_cycle = signal.valid_cycle]() {
                    if (auto t = target.lock()) {
                        t->set_signal_value(signal_name, value, valid_cycle);
                    }
                };
                event_queue.push(sig_event);
            }
        }
    }
}

#endif // MODULEBASE_H
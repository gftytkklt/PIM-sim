#ifndef MODULEBASE_H
#define MODULEBASE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <memory>
#include <optional>
#include <iostream>
#include "ISimulatable.h"
#include "Process.h"
#include "MessageBase.h"
#include "signals.h"

/**
 * 信号定义
 */
struct Signal {
    SignalID name;
    enum class Direction { INPUT, OUTPUT, INTERNAL} direction;
    bool valid{false};
    uint64_t valid_cycle{0};
    std::any value;

    template<typename T>
    Signal(SignalID n, Signal::Direction d, T&& v)
        : name(n), direction(d), value(std::forward<T>(v)) {}

    Signal(SignalID n, Signal::Direction d)
        : name(n), direction(d) {}
    
    Signal() = default;
};

// 连接管理映射：源模块信号 -> 目标模块信号列表
struct ConnectionInfo {
    std::weak_ptr<ISimulatable> target_module;
    SignalID target_signal;
};
using ConnectionKey = std::pair<std::weak_ptr<ISimulatable>, SignalID>;

struct ConnectionKeyHash {
    std::size_t operator()(const ConnectionKey& key) const {
        auto module_ptr = key.first.lock();
        if (!module_ptr) return 0;
        return std::hash<std::string>{}(module_ptr->get_id()) ^ 
                (std::hash<int>{}(static_cast<int>(key.second)) << 1);
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

/**
 * 模块基类
 * 实现ISimulatable接口
 */
class ModuleBase : public ISimulatable, 
                   public std::enable_shared_from_this<ISimulatable> {
protected:
    std::string id_;
    int topological_depth_{0};
    
    // 信号管理
    std::unordered_map<SignalID, Signal> signals_;
    
    // 连接管理
    struct Connection {
        SignalID local_signal;
        std::weak_ptr<ISimulatable> target_module;
        SignalID target_signal;
    };
    std::vector<Connection> connections_;

    //进程管理
    std::unique_ptr<ProcessManager> process_manager_;
    // 信号到进程类型的映射（用于快速检查哪些进程受信号影响）
    std::unordered_map<SignalID, std::vector<std::string>> signal_to_processes_;

    // 本周期待调度事件缓冲（由 evaluate() 返回给模拟器）
    std::vector<SimulatorEvent> pending_events_;

    // 用于提交消息的函数指针
    using MessageHandlerFunc = std::function<void(const GenericMessage&)>;
    std::unordered_map<std::string, MessageHandlerFunc> message_handlers_;
    
    // 性能统计
    mutable std::unordered_map<std::string, uint64_t> performance_stats_;
    
public:
    ModuleBase(const std::string& id) : id_(id) {
        process_manager_ = std::make_unique<ProcessManager>();
    }

    // 新增：注册消息处理函数
    void register_message_handler(const std::string& task_id, 
                                 MessageHandlerFunc handler) {
        message_handlers_[task_id] = std::move(handler);
    }
    
    // 便捷版本：注册消息处理函数（完美转发）
    // 允许传递除了msg以外的其它参数，这些参数会被绑定到处理函数中，在消息到达时一起调用。
    template<typename Func, typename... Args>
    void register_message_handler(const std::string& task_id, 
                                 Func&& func, Args&&... args) {
        // 使用lambda包装函数和参数
        message_handlers_[task_id] = 
            [func = std::forward<Func>(func), 
             args = std::make_tuple(std::forward<Args>(args)...)]
            (const GenericMessage& msg) mutable {
            
            // 调用函数，传入消息和绑定的参数
            std::apply([&func, &msg](auto&&... bound_args) {
                func(msg, std::forward<decltype(bound_args)>(bound_args)...);
            }, std::move(args));
        };
    }

    virtual void handle_message(const GenericMessage& msg) {
        std::cout << std::endl;
        auto it = message_handlers_.find(msg.task_id);
        if (it != message_handlers_.end()) {
            // 找到处理函数，执行它
            it->second(msg);
        } else {
            // 没有找到处理函数，调用默认处理器
            throw std::runtime_error(
                "Module " + id_ + ": No handler registered for message with task_id: " + 
                msg.task_id
            );
        }
    }
    
    virtual ~ModuleBase() = default;
    
    // ========== ISimulatable接口实现 ==========
    
    const std::string& get_id() const override { 
        return id_; 
    }
    
    std::type_index get_module_type() const override { 
        return typeid(*this); 
    }
    
    int get_topological_depth() const override { 
        return topological_depth_; 
    }
    
    void set_topological_depth(int depth) override { 
        topological_depth_ = depth; 
    }
    
    bool has_signal(SignalID name) const override {
        return signals_.find(name) != signals_.end();
    }
    
    std::any get_signal_value(SignalID name) const override {
        auto it = signals_.find(name);
        if (it != signals_.end() && it->second.valid) {
            return it->second.value;
        }
        return {};
    }

    template<typename T>
    std::optional<T> get_signal_as(SignalID name) const {
        auto it = signals_.find(name);
        if (it == signals_.end() || !it->second.valid) return std::nullopt;
        const auto* ptr = std::any_cast<T>(&it->second.value);
        return ptr ? std::optional<T>(*ptr) : std::nullopt;
    }

    void clear_signal(SignalID name) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.valid = false;
            it->second.value.reset();
        }
    }

    // 这是将setter函数延迟到对应周期的接口
    // 当模块产生输出的时候，不直接修改信号的值，通过该接口提交一个信号更新事件。
    void submit_signal_value(SignalID name, 
                         const std::any& value, 
                         uint64_t valid_cycle) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            performance_stats_["signal_updates"]++;
            // 将信号更新事件累积到缓冲，由 evaluate() 返回给模拟器
            pending_events_.push_back(
                SimulatorEvent::make_signal(valid_cycle, shared_from_this(), name, value));
        }
        else {
            throw std::runtime_error(std::string("Attempting to submit value for non-existent signal: ") + signal_name(name));
        }
    }

    // 提交GenericMessage
    // task_id在simulator中注册，对应处理函数
    // delay cycle跟body是一样的，传值的东西
    void submit_message(const GenericMessage& msg) {
        // 将消息事件累积到缓冲，由 evaluate() 返回给模拟器
        pending_events_.push_back(
            SimulatorEvent::make_message(msg.delay_cycles, shared_from_this(), msg));
    }

    void submit_message(const std::string& task_id, const std::any& body, uint64_t delay_cycles = 0) {
        submit_message(GenericMessage(task_id, body, delay_cycles));
    }

    void invalidate_signal(SignalID name) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.valid = false;
            it->second.value.reset();
        }
    }
    
    // setter函数
    void set_signal_value(SignalID name, 
                         const std::any& value, 
                         uint64_t valid_cycle) override {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.value = value;
            it->second.valid = true;
            it->second.valid_cycle = valid_cycle;
        }
    }
    // 目前没有用到这个函数，但为了实现嵌套module的连接，需要保留这个接口
    // 当前的实现是在simulator里例化flatten的module，然后定义它们的connection
    void connect_to(SignalID local_signal,
                   std::shared_ptr<ISimulatable> target_module,
                   SignalID target_signal) override {
        connections_.push_back({local_signal, target_module, target_signal});
    }
    
    // 这里直接调用
    std::vector<SimulatorEvent> evaluate(uint64_t current_cycle) override {
        performance_stats_["total_evaluations"]++;
        process_manager_->drive_state_transitions(current_cycle);
        // 返回并清空本周期待调度事件
        auto events = std::move(pending_events_);
        pending_events_.clear();
        return events;
    }
    
    const std::vector<ProcessEventPtr>& get_active_processes() const override {
        static const std::vector<ProcessEventPtr> empty;
        if (process_manager_) {
            return process_manager_->get_active_events();
        }
        return empty;
    }

    const std::vector<ProcessEventPtr>& get_completed_processes() const override {
        static const std::vector<ProcessEventPtr> empty;
        if (process_manager_) {
            return process_manager_->get_completed_events();
        }
        return empty;
    }
    
    void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const override {
        stats = performance_stats_;
        auto module_stats = get_process_stats();
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
    void bind_signal_to_process(SignalID signal_name, 
                               const std::string& process_name) {
        signal_to_processes_[signal_name].push_back(process_name);
    }

    // 获取受信号影响的进程列表
    // 可以基于此函数构建一个反向索引，快速找到受某个信号影响的进程列表，在信号更新时直接检查这些进程的触发条件，而不是每次都遍历所有进程。
    const std::vector<std::string>& get_processes_by_signal(SignalID signal_name) const {
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
    
    // 添加信号
    void add_signal(const Signal& signal) {
        signals_[signal.name] = signal;
    }
    
    // 获取信号
    Signal& get_signal(SignalID name) {
        return signals_.at(name);
    }
    
    const Signal& get_signal(SignalID name) const {
        return signals_.at(name);
    }

    // 派生类可访问的进程管理器
    ProcessManager& get_process_manager() { return *process_manager_; }
    const ProcessManager& get_process_manager() const { return *process_manager_; }
    // 具体模块需要实现的接口
    virtual void register_processes() = 0; // 由派生类实现，注册自己的进程类型和条件函数
    virtual void register_message_handlers() = 0; // 由派生类实现，注册自己的消息处理函数
    virtual std::unordered_map<std::string, uint64_t> get_module_specific_stats() const {
         return {};
    }
};

#endif // MODULEBASE_H
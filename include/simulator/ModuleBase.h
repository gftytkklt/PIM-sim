#ifndef MODULEBASE_H
#define MODULEBASE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <memory>
#include <iostream>
#include "ISimulatable.h"
#include "Process.h"
#include "MessageBase.h"

/**
 * 信号定义
 */
struct Signal {
    std::string name;
    enum class Direction { INPUT, OUTPUT, INTERNAL} direction;
    bool valid{false};
    uint64_t valid_cycle{0};
    std::any value;

    template<typename T>
    Signal(const std::string& n, Signal::Direction d, T&& v)
        : name(n), direction(d), value(std::forward<T>(v)) {
            // std::cout << "Initialized signal '" << name << "' with value of type " 
            //           << value.type().name() << std::endl;
        }

    Signal(const std::string& n, Signal::Direction d)
        : name(n), direction(d) {}
    
    Signal() = default;
};

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

    // 添加：用于提交信号更新事件的函数指针
    std::function<void(uint64_t, std::weak_ptr<ISimulatable>, 
                      std::string, std::any)> schedule_signal_update_callback_;
    
    // 性能统计
    mutable std::unordered_map<std::string, uint64_t> performance_stats_;

    // 消息接口
    using MessageCallback = std::function<void(
        uint64_t delay_cycles,
        MessageHeader header,
        std::any payload
    )>;

    MessageCallback message_callback_;
    
public:
    ModuleBase(const std::string& id) : id_(id) {
        // 初始化默认性能统计
        // performance_stats_["total_cycles"] = 0; // 相当于total_evaluations
        // performance_stats_["events_processed"] = 0; // 相当于total_complete_events
        // performance_stats_["busy_cycles"] = 0; 这个放到进程计数器里做。
        // 私有模块要在这里注册进程类型。也就是派生类必须要调用register_process来注册自己的进程类型
        process_manager_ = std::make_unique<ProcessManager>();
    }

    // 设置信号更新回调
    void set_schedule_callback(std::function<void(uint64_t, std::weak_ptr<ISimulatable>, 
                                                std::string, std::any)> callback) {
        schedule_signal_update_callback_ = callback;
    }

    // 设置信息发送回调
    void set_message_callback(MessageCallback callback) {
        message_callback_ = callback;
    }

    virtual void handle_message(const GenericMessage& msg) {
        // 默认实现：打印消息内容
        std::cout << "Module '" << id_ << "' received message of type: " 
                //   << static_cast<int>(msg.header().type) 
                //   << " with body type: " << msg.body_().type().name() 
                  << std::endl;
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

    void clear_signal(const std::string& name) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.valid = false;
            it->second.value.reset();
            // keep previous valid_cycle info
            // it->second.valid_cycle = 0;
        }
    }

    // 这是将setter函数延迟到对应周期的接口
    // 当模块产生输出的时候，不直接修改信号的值，通过该接口提交一个信号更新事件。
    void submit_signal_value(const std::string& name, 
                         const std::any& value, 
                         uint64_t valid_cycle) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            // it->second.value = value;
            // it->second.valid = true;
            // it->second.valid_cycle = valid_cycle; // 经多少周期以后信号生效。
            
            performance_stats_["signal_updates"]++;
            
            // 提交信号更新事件
            if (schedule_signal_update_callback_) {
                schedule_signal_update_callback_(
                    valid_cycle, 
                    std::weak_ptr<ISimulatable>(this->shared_from_this()),
                    name, 
                    value
                );
            }
        }
        else {
            throw std::runtime_error("Attempting to submit value for non-existent signal: " + name);
        }
        // std::cout << "Module " << id_ << " submitted signal update: " 
        //           << name << " = " << value.type().name() 
        //           << " (valid after cycle " << valid_cycle << ")" << std::endl;
    }

    // 当前可以把delay cycle视为0，也就是传输以后立刻更新消息表项
    // 如果需要模拟传输延迟，可以在simulator里维护key为valid cycle的队列来检查
    // 消息队列可以不主动触发行为，只用来更新状态。
    // 在当前用例里，消息队列的来源是SIMD计算完成，每计算完成一次或四次，就更新消息表项
    // 目前消息表项可以直接触发写事件，
    template<typename... Args>
    void send_message(MessageHeader header, Args&&... args) {
        if (message_callback_) {
            // 这里直接调用回调函数，消息的发送和处理都是在当前周期进行的。
            auto body = MessageBody<std::decay_t<Args>...>(
                std::forward<Args>(args)...
            );
            // 调用回调函数
            schedule_message_callback_(
                header.delay_cycles,
                std::move(header),
                std::make_any<MessageBody<std::decay_t<Args>...>>(std::move(body))
            );
            } else {
                throw std::runtime_error("Message callback not set for module: " + id_);
            }
    }

    void invalidate_signal(const std::string& name) {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.valid = false;
            it->second.value.reset();
            // keep previous valid_cycle info
            // it->second.valid_cycle = 0;
        }
    }
    
    // setter函数
    void set_signal_value(const std::string& name, 
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
    void connect_to(const std::string& local_signal,
                   std::shared_ptr<ISimulatable> target_module,
                   const std::string& target_signal) override {
        connections_.push_back({local_signal, target_module, target_signal});
    }
    
    // 这里直接调用
    virtual void evaluate(uint64_t current_cycle) override {
        performance_stats_["total_evaluations"]++;
        process_manager_->drive_state_transitions(current_cycle);
    }
    
    const std::vector<ProcessEventPtr>& get_active_processes() const override {
        static const std::vector<ProcessEventPtr> empty;
        if (process_manager_) {
            return process_manager_->get_active_events();
        }
        return empty;
    }
    
    void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const override {
        stats = performance_stats_;
        
        // 添加模块特定统计
        auto derived = static_cast<const DerivedModule*>(this);
        // auto module_stats = derived->get_module_specific_stats();
        auto module_stats = derived->get_process_stats();
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
    // 可以基于此函数构建一个反向索引，快速找到受某个信号影响的进程列表，在信号更新时直接检查这些进程的触发条件，而不是每次都遍历所有进程。
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
    // 具体模块需要实现的接口
    virtual void register_processes() = 0; // 由派生类实现，注册自己的进程类型和条件函数
    virtual std::unordered_map<std::string, uint64_t> get_module_specific_stats() const {
         return {};
    }
};

#endif // MODULEBASE_H
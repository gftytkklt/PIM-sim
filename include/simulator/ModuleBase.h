#ifndef MODULEBASE_H
#define MODULEBASE_H

#include <string>
#include <unordered_map>
#include <vector>
#include <any>
#include <memory>
#include "ISimulatable.h"
#include "Process.h"

/**
 * 信号定义
 */
struct Signal {
    std::string name;
    enum class Direction { INPUT, OUTPUT, INTERNAL} direction;
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
    
public:
    ModuleBase(const std::string& id) : id_(id) {
        // 初始化默认性能统计
        performance_stats_["total_cycles"] = 0;
        performance_stats_["events_processed"] = 0;
        performance_stats_["busy_cycles"] = 0;
        // 私有模块要在这里注册进程类型。也就是派生类必须要调用register_process来注册自己的进程类型
        process_manager_ = std::make_unique<ProcessManager>();
    }

    // 设置信号更新回调
    void set_schedule_callback(std::function<void(uint64_t, std::weak_ptr<ISimulatable>, 
                                                std::string, std::any)> callback) {
        schedule_signal_update_callback_ = callback;
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

    // 修改set_signal_value实现
    void set_signal_value(const std::string& name, 
                         const std::any& value, 
                         uint64_t valid_cycle) override {
        auto it = signals_.find(name);
        if (it != signals_.end()) {
            it->second.value = value;
            it->second.valid = true;
            it->second.valid_cycle = valid_cycle;
            
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
    }
    
    // void set_signal_value(const std::string& name, 
    //                      const std::any& value, 
    //                      uint64_t valid_cycle) override {
    //     auto it = signals_.find(name);
    //     if (it != signals_.end()) {
    //         it->second.value = value;
    //         it->second.valid = true;
    //         it->second.valid_cycle = valid_cycle;
            
    //         // 记录信号更新事件
    //         performance_stats_["signal_updates"]++;

    //         // 提交信号更新事件到模拟器队列
    //         schedule_signal_update(name, value, valid_cycle);
    //     }
    // }

    // void schedule_signal_update(const std::string& signal_name,
    //                            const std::any& value,
    //                            uint64_t valid_cycle) override {
    //     if (auto simulator = simulator_.lock()) {
    //         // 获取这个信号的所有连接
    //         auto connections = get_output_connections(signal_name);
            
    //         for (const auto& [target_id, target_signal] : connections) {
    //             // 创建信号更新事件
    //             SignalUpdateEvent event;
    //             event.cycle = valid_cycle;
    //             event.source_module = this->shared_from_this();
    //             event.source_signal = signal_name;
                
    //             // 这里需要从模拟器获取目标模块
    //             // 实际实现中，ModuleBase可能需要知道如何获取目标模块
    //             // 为了简化，我们可以在set_signal_value时就创建所有目标事件
    //         }
    //     }
    // }
    
    void connect_to(const std::string& local_signal,
                   std::shared_ptr<ISimulatable> target_module,
                   const std::string& target_signal) override {
        connections_.push_back({local_signal, target_module, target_signal});
    }
    
    // 这里直接调用
    void evaluate(uint64_t current_cycle) override {
        stats_["total_evaluations"]++;
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

    // std::vector<std::pair<std::string, std::string>> 
    // get_output_connections(const std::string& signal_name) const override {
    //     std::vector<std::pair<std::string, std::string>> result;
        
    //     for (const auto& conn : connections_) {
    //         if (conn.local_signal == signal_name) {
    //             if (auto target = conn.target_module.lock()) {
    //                 result.emplace_back(target->get_id(), conn.target_signal);
    //             }
    //         }
    //     }
    //     return result;
    // }
    
    
    
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
    virtual std::unordered_map<std::string, uint64_t> get_module_specific_stats() const = 0;
};

#endif // MODULEBASE_H
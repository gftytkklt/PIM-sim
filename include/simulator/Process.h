#ifndef PROCESS_H
#define PROCESS_H

#include <cstdint>
#include <string>
#include <functional>
#include <vector>
#include <unordered_map>
#include <memory>

/**
 * 进程事件实例
 * 封装单个事件的生命周期和时间戳
 */
class ProcessEvent {
public:
    enum class State {
        IDLE,           // 空闲，未触发
        TRIGGERED,      // 已触发，等待执行
        EXECUTING,      // 执行中
        FINISHED,       // 执行完成
        ENDED           // 事件结束
    };
    
    // 事件ID，用于唯一标识事件实例
    struct EventID {
        std::string process_type;  // 进程类型
        uint64_t instance_id;      // 实例ID
        bool operator==(const EventID& other) const {
            return process_type == other.process_type && instance_id == other.instance_id;
        }
    };
    
    struct EventIDHash {
        std::size_t operator()(const EventID& id) const {
            return std::hash<std::string>{}(id.process_type) ^ (std::hash<uint64_t>{}(id.instance_id) << 1);
        }
    };
    
    ProcessEvent(const std::string& process_type, uint64_t instance_id);
    ~ProcessEvent() = default;
    
    // 基本信息
    const EventID& get_id() const { return id_; }
    State get_state() const { return state_; }
    const std::string& get_process_type() const { return id_.process_type; }
    uint64_t get_instance_id() const { return id_.instance_id; }
    
    // 时间戳访问
    uint64_t get_trigger_time() const { return trigger_time_; }
    uint64_t get_exec_time() const { return exec_time_; }
    uint64_t get_finish_time() const { return finish_time_; }
    uint64_t get_end_time() const { return end_time_; }
    
    // 状态检查和转换
    bool is_idle() const { return state_ == State::IDLE; }
    bool is_triggered() const { return state_ == State::TRIGGERED; }
    bool is_executing() const { return state_ == State::EXECUTING; }
    bool is_finished() const { return state_ == State::FINISHED; }
    bool is_ended() const { return state_ == State::ENDED; }
    
    // 状态转换（由模块外部根据条件调用）
    void set_triggered(uint64_t current_cycle);
    void set_executing(uint64_t current_cycle);
    void set_finished(uint64_t current_cycle);
    void set_ended(uint64_t current_cycle);
    
    // 重置事件
    void reset();
    
    // 性能统计
    std::unordered_map<std::string, uint64_t> get_timing_stats() const;
    
private:
    EventID id_;
    State state_{State::IDLE};
    uint64_t trigger_time_{0};
    uint64_t exec_time_{0};
    uint64_t finish_time_{0};
    uint64_t end_time_{0};
};

/**
 * 进程类型定义
 * 描述一种功能的事件进程
 */
class ProcessType {
public:
    using TriggerCondition = std::function<bool()>;  // 触发条件检查函数
    using ExecCondition = std::function<bool()>;     // 执行条件检查函数
    using FinishCondition = std::function<bool()>;   // 完成条件检查函数
    using EndCondition = std::function<bool()>;      // 结束条件检查函数
    
    ProcessType(const std::string& name, 
                TriggerCondition trigger_cond,
                ExecCondition exec_cond,
                FinishCondition finish_cond,
                EndCondition end_cond,
                uint64_t latency = 1);
    
    const std::string& get_name() const { return name_; }
    uint64_t get_latency() const { return latency_; }
    
    // 条件检查接口
    bool check_trigger() const;
    bool check_exec() const;
    bool check_finish() const;
    bool check_end() const;
    
    // 更新条件函数（允许运行时修改条件）
    void update_conditions(TriggerCondition trigger_cond,
                          ExecCondition exec_cond,
                          FinishCondition finish_cond,
                          EndCondition end_cond);
    
private:
    std::string name_;
    TriggerCondition trigger_condition_;
    ExecCondition exec_condition_;
    FinishCondition finish_condition_;
    EndCondition end_condition_;
    uint64_t latency_;  // 执行到完成的延迟周期数
};

using ProcessEventPtr = std::shared_ptr<ProcessEvent>;
using ProcessTypePtr = std::shared_ptr<ProcessType>;

/**
 * 进程管理器
 * 管理模块的所有事件进程
 */
class ProcessManager {
public:
    ProcessManager() = default;
    ~ProcessManager() = default;
    
    // 注册进程类型
    bool register_process_type(const std::string& name,
                              ProcessType::TriggerCondition trigger_cond,
                              ProcessType::ExecCondition exec_cond,
                              ProcessType::FinishCondition finish_cond,
                              ProcessType::EndCondition end_cond,
                              uint64_t latency = 1);
    
    // 获取进程类型
    ProcessTypePtr get_process_type(const std::string& name) const;
    
    // 创建事件实例
    ProcessEventPtr create_active_event(const std::string& process_type, uint64_t current_cycle);
    
    // 获取活跃事件
    const std::vector<ProcessEventPtr>& get_active_events() const { return active_events_; }
    std::vector<ProcessEventPtr> get_events_by_type(const std::string& process_type) const;
    
    // 检查是否已存在某类事件
    bool has_active_event_of_type(const std::string& process_type) const;

    void drive_state_transitions(uint64_t current_cycle);
    
    // 移除已完成事件
    void cleanup_ended_events();
    
    // 获取性能统计
    std::unordered_map<std::string, uint64_t> get_performance_stats() const;
    
    // 重置所有事件
    void reset_all_events();
    
private:
    // 进程类型注册表
    std::unordered_map<std::string, ProcessTypePtr> process_types_;
    
    // 活跃事件列表
    std::vector<ProcessEventPtr> active_events_;
    
    // 已完成事件历史（用于统计）
    std::vector<ProcessEventPtr> completed_events_;
    
    // 事件实例ID计数器
    uint64_t next_instance_id_{0};
    
    // 查找事件
    ProcessEventPtr find_event(const ProcessEvent::EventID& id) const;
};

#endif // PROCESS_H
#include "Process.h"
#include <iostream>

ProcessEvent::ProcessEvent(const std::string& process_type, uint64_t instance_id) {
    id_.process_type = process_type;
    id_.instance_id = instance_id;
    state_ = State::IDLE;
}

void ProcessEvent::set_triggered(uint64_t current_cycle) {
    if (state_ == State::IDLE) {
        state_ = State::TRIGGERED;
        trigger_time_ = current_cycle;
    }
}

void ProcessEvent::set_executing(uint64_t current_cycle) {
    if (state_ == State::TRIGGERED) {
        state_ = State::EXECUTING;
        exec_time_ = current_cycle;
    }
}

void ProcessEvent::set_finished(uint64_t current_cycle) {
    if (state_ == State::EXECUTING) {
        state_ = State::FINISHED;
        finish_time_ = current_cycle;
    }
}

void ProcessEvent::set_ended(uint64_t current_cycle) {
    if (state_ == State::FINISHED) {
        state_ = State::ENDED;
        end_time_ = current_cycle;
    }
}

void ProcessEvent::reset() {
    state_ = State::IDLE;
    trigger_time_ = 0;
    exec_time_ = 0;
    finish_time_ = 0;
    end_time_ = 0;
}

std::unordered_map<std::string, uint64_t> ProcessEvent::get_timing_stats() const {
    std::unordered_map<std::string, uint64_t> stats;
    
    if (state_ == State::ENDED) {
        stats["trigger_time"] = trigger_time_;
        stats["exec_time"] = exec_time_;
        stats["finish_time"] = finish_time_;
        stats["end_time"] = end_time_;
        
        // 计算延迟
        if (exec_time_ > trigger_time_) {
            stats["trigger_to_exec_latency"] = exec_time_ - trigger_time_;
        } else {
            stats["trigger_to_exec_latency"] = 0;
        }
        
        if (finish_time_ > exec_time_) {
            stats["exec_to_finish_latency"] = finish_time_ - exec_time_;
        } else {
            stats["exec_to_finish_latency"] = 0;
        }
        
        if (end_time_ > finish_time_) {
            stats["finish_to_end_latency"] = end_time_ - finish_time_;
        } else {
            stats["finish_to_end_latency"] = 0;
        }
        
        if (end_time_ > trigger_time_) {
            stats["total_latency"] = end_time_ - trigger_time_;
        } else {
            stats["total_latency"] = 0;
        }
    }
    
    return stats;
}

ProcessType::ProcessType(const std::string& name, 
                         TriggerCondition trigger_cond,
                         ExecCondition exec_cond,
                         FinishCondition finish_cond,
                         EndCondition end_cond,
                         uint64_t latency)
    : name_(name)
    , trigger_condition_(trigger_cond)
    , exec_condition_(exec_cond)
    , finish_condition_(finish_cond)
    , end_condition_(end_cond)
    , latency_(latency) {
}


bool ProcessType::check_trigger() const {
    if (trigger_condition_) {
        return trigger_condition_();
    }
    return false;
}

bool ProcessType::check_exec() const {
    if (exec_condition_) {
        return exec_condition_();
    }
    return false;
}

bool ProcessType::check_finish() const {
    if (finish_condition_) {
        return finish_condition_();
    }
    return false;
}

bool ProcessType::check_end() const {
    if (end_condition_) {
        return end_condition_();
    }
    return false;
}

void ProcessType::update_conditions(TriggerCondition trigger_cond,
                                   ExecCondition exec_cond,
                                   FinishCondition finish_cond,
                                   EndCondition end_cond) {
    trigger_condition_ = trigger_cond;
    exec_condition_ = exec_cond;
    finish_condition_ = finish_cond;
    end_condition_ = end_cond;
}

bool ProcessManager::register_process_type(const std::string& name,
                                          ProcessType::TriggerCondition trigger_cond,
                                          ProcessType::ExecCondition exec_cond,
                                          ProcessType::FinishCondition finish_cond,
                                          ProcessType::EndCondition end_cond,
                                          uint64_t latency) {
    if (process_types_.find(name) != process_types_.end()) {
        std::cerr << "Warning: Process type '" << name << "' already registered." << std::endl;
        return false;
    }
    
    auto process_type = std::make_shared<ProcessType>(name, trigger_cond, exec_cond, 
                                                     finish_cond, end_cond, latency);
    process_types_[name] = process_type;
    
    return true;
}

ProcessTypePtr ProcessManager::get_process_type(const std::string& name) const {
    auto it = process_types_.find(name);
    if (it != process_types_.end()) {
        return it->second;
    }
    return nullptr;
}

ProcessEventPtr ProcessManager::create_event_instance(const std::string& process_type) {
    if (process_types_.find(process_type) == process_types_.end()) {
        std::cerr << "Error: Process type '" << process_type << "' not registered." << std::endl;
        return nullptr;
    }
    
    uint64_t instance_id = next_instance_id_++;
    auto event = std::make_shared<ProcessEvent>(process_type, instance_id);
    active_events_.push_back(event);
    
    return event;
}

ProcessEventPtr ProcessManager::find_event(const ProcessEvent::EventID& id) const {
    for (const auto& event : active_events_) {
        if (event->get_id() == id) {
            return event;
        }
    }
    return nullptr;
}

std::vector<ProcessEventPtr> ProcessManager::get_events_by_type(const std::string& process_type) const {
    std::vector<ProcessEventPtr> result;
    
    for (const auto& event : active_events_) {
        if (event->get_process_type() == process_type) {
            result.push_back(event);
        }
    }
    
    return result;
}

bool ProcessManager::has_active_event_of_type(const std::string& process_type) const {
    for (const auto& event : active_events_) {
        if (event->get_process_type() == process_type && 
            event->get_state() != ProcessEvent::State::ENDED &&
            event->get_state() != ProcessEvent::State::IDLE) {
            return true;
        }
    }
    return false;
}

void ProcessManager::update_event_states(uint64_t current_cycle) {
    // 这个函数主要更新事件的状态，但具体的状态转换逻辑
    // 应该由使用ProcessManager的模块在check_triggers中实现
    // 这里只提供基础的时间检查
    
    for (auto& event : active_events_) {
        auto process_type = get_process_type(event->get_process_type());
        if (!process_type) continue;
        
        // 对于执行中的事件，可以检查是否应该完成
        if (event->is_executing()) {
            uint64_t exec_time = event->get_exec_time();
            uint64_t latency = process_type->get_latency();
            
            // 如果经过了足够的延迟，标记为可完成
            // 注意：实际的finish状态转换应该在模块的check_finish条件中判断
            if (current_cycle >= exec_time + latency) {
                // 这里不直接调用event->set_finished，因为完成条件
                // 可能还需要其他信号条件满足
            }
        }
    }
}

void ProcessManager::cleanup_ended_events() {
    // 将已结束的事件移到完成列表
    auto it = std::remove_if(active_events_.begin(), active_events_.end(),
        [this](const ProcessEventPtr& event) {
            if (event->is_ended()) {
                completed_events_.push_back(event);
                return true;
            }
            return false;
        });
    
    active_events_.erase(it, active_events_.end());
}

std::unordered_map<std::string, uint64_t> ProcessManager::get_performance_stats() const {
    std::unordered_map<std::string, uint64_t> stats;
    
    // 统计已结束的事件
    stats["total_completed_events"] = completed_events_.size();
    
    if (!completed_events_.empty()) {
        uint64_t total_latency = 0;
        uint64_t min_latency = UINT64_MAX;
        uint64_t max_latency = 0;
        
        for (const auto& event : completed_events_) {
            auto event_stats = event->get_timing_stats();
            if (event_stats.find("total_latency") != event_stats.end()) {
                uint64_t latency = event_stats.at("total_latency");
                total_latency += latency;
                min_latency = std::min(min_latency, latency);
                max_latency = std::max(max_latency, latency);
            }
        }
        
        stats["avg_latency"] = total_latency / completed_events_.size();
        stats["min_latency"] = min_latency;
        stats["max_latency"] = max_latency;
    }
    
    // 统计活跃事件
    stats["active_events_count"] = active_events_.size();
    
    // 按状态统计活跃事件
    std::unordered_map<ProcessEvent::State, uint64_t> state_counts;
    for (const auto& event : active_events_) {
        state_counts[event->get_state()]++;
    }
    
    stats["active_idle"] = state_counts[ProcessEvent::State::IDLE];
    stats["active_triggered"] = state_counts[ProcessEvent::State::TRIGGERED];
    stats["active_executing"] = state_counts[ProcessEvent::State::EXECUTING];
    stats["active_finished"] = state_counts[ProcessEvent::State::FINISHED];
    
    return stats;
}

void ProcessManager::reset_all_events() {
    for (auto& event : active_events_) {
        event->reset();
    }
    active_events_.clear();
    completed_events_.clear();
    next_instance_id_ = 0;
}
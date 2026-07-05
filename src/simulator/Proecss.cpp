#include "simulator/Process.h"
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

// 利用这些函数驱动状态转换的同时，用于更新模块输出参数和值传递，反正这些函数都是模块自带的，可以查看内部变量。
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


// 为了保证事件只执行一次，应该将功能函数放在check_exec里，如果检测到合法的握手信号，就执行对应的功能函数，并返回true
// check_finish检查是否返回了执行完成的握手信号，当前使用相关信号的值判断是否完成
// 后续优化可以考虑根据延迟触发finish事件，而不是一直等待
// check_end根据握手判断是否真正结束。
bool ProcessManager::register_process_type(const std::string& name,
                                          ProcessType::TriggerCondition trigger_cond,
                                          ProcessType::ExecCondition exec_cond,
                                          ProcessType::FinishCondition finish_cond,
                                          ProcessType::EndCondition end_cond,
                                          uint64_t latency) {
    if (process_types_.find(name) != process_types_.end()) {
        throw std::runtime_error("Process type '" + name + "' already registered.");
        // std::cerr << "Warning: Process type '" << name << "' already registered." << std::endl;
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

ProcessEventPtr ProcessManager::create_active_event(const std::string& process_type, uint64_t current_cycle) {
    if (process_types_.find(process_type) == process_types_.end()) {
        std::cerr << "Error: Process type '" << process_type << "' not registered." << std::endl;
        return nullptr;
    }
    
    uint64_t instance_id = next_instance_id_++;
    auto event = std::make_shared<ProcessEvent>(process_type, instance_id);
    event->set_triggered(current_cycle);
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

void ProcessManager::drive_state_transitions(uint64_t current_cycle) {
    // 首先判断是否有新的活跃事件触发
    for (const auto& [name, process_type] : process_types_) {
        // std::cout << "Checking trigger condition for process type: " << name << std::endl;
        if (!has_active_event_of_type(name) && process_type->check_trigger()) {
            // 如果触发条件满足，创建一个新的事件实例
            // std::cout << "Triggering new event of type: " << name << " at cycle " << current_cycle << std::endl;
            create_active_event(name, current_cycle);
        }
    }
    for (auto& event : active_events_) {
        auto process_type = get_process_type(event->get_process_type());
        if (!process_type) {
            // 事件对应的类型未注册，可能是错误，跳过
            throw std::runtime_error("Event has unregistered process type: " + event->get_process_type());
        }

        // 根据事件的当前状态，检查相应的条件并进行转换
        // 使用while循环，允许一个周期内满足条件时连续转换多个状态（如从IDLE直接到EXECUTING）
        bool state_changed = true;
        while (state_changed) {
            state_changed = false;
            switch (event->get_state()) {
                // 所有活跃事件都是TRIGGERED或更后续状态。
                case ProcessEvent::State::TRIGGERED:
                    if (process_type->check_exec()) {
                        // std::cout << "Event " << event->get_id().process_type << ":" << event->get_id().instance_id 
                        //           << " transitioning to EXECUTING at cycle " << current_cycle << std::endl;
                        event->set_executing(current_cycle);
                        state_changed = true;
                    }
                    break;
                    
                case ProcessEvent::State::EXECUTING:
                    // 选择1: 基于条件函数
                    if (process_type->check_finish()) {
                        // std::cout << "Event " << event->get_id().process_type << ":" << event->get_id().instance_id 
                        //           << " transitioning to FINISHED at cycle " << current_cycle << std::endl;
                        event->set_finished(current_cycle);
                        state_changed = true;
                    }
                    // 选择2: 或基于固定延迟 (保留现有逻辑)
                    // uint64_t exec_time = event->get_exec_time();
                    // uint64_t latency = process_type->get_latency();
                    // if (current_cycle >= exec_time + latency) {
                    //     event->set_finished(current_cycle);
                    //     state_changed = true;
                    // }
                    break;
                    
                case ProcessEvent::State::FINISHED:
                    if (process_type->check_end()) {
                        // std::cout << "Event " << event->get_id().process_type << ":" << event->get_id().instance_id 
                        //           << " transitioning to ENDED at cycle " << current_cycle << std::endl;
                        event->set_ended(current_cycle);
                        state_changed = true;
                    }
                    break;
                    
                case ProcessEvent::State::ENDED:
                    // ENDED 是终态，不再转换
                default:
                    break;
            }
        }
    }
    
    // 可选：在状态驱动的最后，清理已结束的事件
    cleanup_ended_events();
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
    // std::cout << "Total completed events: " << stats["total_completed_events"] << std::endl;
    
    if (!completed_events_.empty()) {
        uint64_t total_latency = 0;
        uint64_t min_latency = UINT64_MAX;
        uint64_t max_latency = 0;
        uint64_t busy_time = 0;
        
        for (const auto& event : completed_events_) {
            auto event_id = event->get_id();
            // std::cout << "process name: " << event_id.process_type << std::endl;
            auto event_stats = event->get_timing_stats();
            if (event_stats.find("total_latency") != event_stats.end()) {
                uint64_t latency = event_stats.at("total_latency");
                uint64_t exec_time = event_stats.at("exec_to_finish_latency");
                total_latency += latency;
                min_latency = std::min(min_latency, latency);
                max_latency = std::max(max_latency, latency);
                busy_time += exec_time;
            }
            // std::cout << "Event " << event_id.process_type << ":" << event_id.instance_id 
            //           << " - Trigger: " << event_stats["trigger_time"] 
            //           << ", End: " << event_stats["end_time"] 
            //           << std::endl;
        }
        stats["total_latency"] = total_latency;
        stats["avg_latency"] = total_latency / completed_events_.size();
        stats["busy_time"] = busy_time;
        stats["min_latency"] = min_latency;
        stats["max_latency"] = max_latency;
    }
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
#include "simulator/Simulator.h"
#include <fstream>
#include <iostream>

CycleAccurateSimulator::CycleAccurateSimulator(uint64_t max_cycles) 
    : max_cycles_(max_cycles) {}

    // 处理消息事件的实现
void CycleAccurateSimulator::process_message_events(uint64_t current_cycle) {
    while (!message_queue_.empty() && 
            message_queue_.top().trigger_cycle <= current_cycle) {
        
        auto event = message_queue_.top();
        message_queue_.pop();
        
        if (event.trigger_cycle < current_cycle) {
            throw std::runtime_error("Message event in the past detected!");
        }
        
        // 根据task_id查找处理函数
        auto handler_it = task_handlers_.find(event.message.task_id);
        if (handler_it != task_handlers_.end()) {
            // 直接调用注册的处理函数
            handler_it->second(event.message);
        } else {
            // std::cerr << "Warning: No handler registered for task: " 
            //             << event.message.task_id << std::endl;
            throw std::runtime_error("No handler registered for task: " + event.message.task_id);
        }
        
        stats_.total_events++;
    }
}

void CycleAccurateSimulator::process_signal_events(uint64_t current_cycle) {
    // 处理当前周期到期的所有信号事件
    while (!signal_event_queue_.empty() && 
           signal_event_queue_.top().cycle <= current_cycle) {
        auto event = signal_event_queue_.top();
        signal_event_queue_.pop();
        
        // 如果事件还未到生效周期，放回队列
        if (event.cycle < current_cycle) {
            // 理论上不应该发生，但安全处理
            throw std::runtime_error("Signal event in the past detected!");
        }
        
        // 传播信号到所有连接的目标
        if (auto source_module = event.module.lock()) {
            propagate_signal_to_targets(
                source_module,
                event.signal_name,
                event.value,
                current_cycle
            );
        }
    }
}

void CycleAccurateSimulator::propagate_signal_to_targets(
    std::shared_ptr<ISimulatable> source_module,
    const std::string& source_signal,
    const std::any& value,
    uint64_t valid_cycle) {
    // 更新源模块信号值
    source_module->set_signal_value(source_signal, value, valid_cycle);
    
    // 查找该信号的所有连接
    ConnectionKey key = {source_module, source_signal};
    auto it = connections_map_.find(key);
    if (it == connections_map_.end()) {
        // throw std::runtime_error("No connections found for signal: " + source_signal);
        // std::cout << "No connections found for signal: " << source_signal 
        //           << " from module: " << source_module->get_id() << std::endl;
        return;  // 没有连接
    }
    
    // 更新所有目标模块的信号
    for (const auto& conn_info : it->second) {
        if (auto target = conn_info.target_module.lock()) {
            target->set_signal_value(
                conn_info.target_signal,
                value,
                valid_cycle
            );
            // std::cout << "Propagated signal '" << source_signal 
            //           << "' from module '" << source_module->get_id() 
            //           << "' to module '" << target->get_id() 
            //           << "' as '" << conn_info.target_signal 
            //           << "' with value type: " << value.type().name() 
            //           << " (valid at cycle " << valid_cycle << ")" 
            //           << std::endl;
        }
    }
}

void CycleAccurateSimulator::run() {
    std::cout << "=== Starting Cycle-Accurate Simulation ===" << std::endl;
    std::cout << "Total modules: " << modules_.size() << std::endl;
    std::cout << "Max cycles: " << max_cycles_ << std::endl;
    
    // 初始化事件
    initialize_simulation();
    
    // 主模拟循环
    while (current_cycle_ < max_cycles_ && !simulation_done_) {
        // std::cout << "\n--- Cycle " << current_cycle_ << " ---" << std::endl;
        simulate_cycle();
        current_cycle_++;
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Final cycle: " << current_cycle_ << std::endl;
    // std::cout << "Total events processed: " << stats_.total_events << std::endl;
    
    print_statistics();
}

void CycleAccurateSimulator::connect_modules(const std::string& src_id, 
                                            const std::string& src_signal,
                                            const std::string& dst_id, 
                                            const std::string& dst_signal) {
    auto src_it = module_map_.find(src_id);
    auto dst_it = module_map_.find(dst_id);
    
    if (src_it != module_map_.end() && dst_it != module_map_.end()) {
        // 1. 在模块层面建立连接
        src_it->second->connect_to(src_signal, dst_it->second, dst_signal);
        
        // 2. 在模拟器层面记录连接关系
        ConnectionKey key = {src_it->second, src_signal};
        connections_map_[key].push_back({
            dst_it->second, dst_signal
        });
    } else {
        throw std::runtime_error("Failed to connect modules. Source or target not found: " + src_id + " -> " + dst_id);
    }
}

const std::vector<std::shared_ptr<ISimulatable>>& CycleAccurateSimulator::get_all_modules() const {
    return modules_;
}

void CycleAccurateSimulator::initialize_simulation() {
    // 创建初始激励事件
    std::cerr << "Initializing simulation with initial events..." << std::endl;
    // 可以在这里执行每个模块的初始化事件。
}

void CycleAccurateSimulator::simulate_cycle() {
    // 1. 处理到期的消息事件
    process_message_events(current_cycle_);

    // 2. 处理到期的信号事件
    process_signal_events(current_cycle_);

    // 处理组合逻辑模块（如果有的话）
    process_combinational_logic();
    
    // 评估所有模块的活跃事件
    stats_.modules_processed = 0;
    for (auto& module : modules_) {
        module->evaluate(current_cycle_);
        stats_.modules_processed++;
    }
    
    // 步骤4: 更新统计
    stats_.total_cycles = current_cycle_;
    
    // 检查结束条件
    check_simulation_complete();
}

void CycleAccurateSimulator::process_combinational_logic() {
    // 处理延迟为0的组合逻辑模块
    for (auto& module : combinational_modules_) {
        module->evaluate(current_cycle_);
    }
}

void CycleAccurateSimulator::check_simulation_complete() {
    // 简单结束条件：事件队列为空，且无信号传输事件
    if (!signal_event_queue_.empty()) {
        return; // 还有事件待处理，继续模拟
    }
    for (const auto& module : modules_) {
        if (!module->get_active_processes().empty()) {
            return; // 还有活跃事件，继续模拟
        }
    }
    std::cout << "No active events remaining. Ending simulation at cycle " << current_cycle_ << "." << std::endl;
    simulation_done_ = true;
}

void CycleAccurateSimulator::print_statistics() const {
    std::cout << "\n=== Performance Statistics ===" << std::endl;
    
    for (const auto& module : modules_) {
        std::unordered_map<std::string, uint64_t> module_stats;
        module->get_performance_stats(module_stats);
        
        std::cout << "\nModule: " << module->get_id() 
                  << " (Type: " << module->get_module_type().name() << ")" 
                  << std::endl;
        
        for (const auto& [key, value] : module_stats) {
            std::cout << "  " << key << ": " << value << std::endl;
        }
    }
    
    std::cout << "\n=== Simulation Summary ===" << std::endl;
    std::cout << "Total simulation cycles: " << stats_.total_cycles << std::endl;
    std::cout << "Total events processed: " << stats_.total_events << std::endl;
    std::cout << "Average events per cycle: " 
              << (stats_.total_cycles > 0 ? 
                  static_cast<double>(stats_.total_events) / stats_.total_cycles : 0.0) 
              << std::endl;
}

void CycleAccurateSimulator::dump_completed_events(const std::string& filename) const {
    std::ofstream ofs(filename);
    if (!ofs.is_open()) {
        std::cerr << "Failed to open file for dumping completed events: " << filename << std::endl;
        return;
    }
    
    for (const auto& module : modules_) {
        const auto& completed_events = module->get_completed_processes();
        for (const auto& event : completed_events) {
            if (event->is_ended()) {
                ofs << "Module: " << module->get_id() 
                    << ", Process Type: " << event->get_process_type() 
                    << ", Timing Stats: ";
                
                auto stats = event->get_timing_stats();
                // 只需要trigger_time和end_time
                if (stats.find("trigger_time") != stats.end() && stats.find("end_time") != stats.end()) {
                    uint64_t trigger_time = stats.at("trigger_time");
                    uint64_t end_time = stats.at("end_time");
                    uint64_t latency = end_time - trigger_time;
                    ofs << "trigger_time=" << trigger_time 
                        << ", end_time=" << end_time;
                }
                ofs << std::endl;
            }
        }
    }
    
    ofs.close();
    std::cout << "Completed events dumped to file: " << filename << std::endl;
}
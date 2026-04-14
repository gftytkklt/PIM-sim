#include "Simulator.h"
#include "simulator/modules/ProcessingUnit.h"
#include "simulator/modules/MemoryUnit.h"
#include <iostream>

CycleAccurateSimulator::CycleAccurateSimulator(uint64_t max_cycles) 
    : max_cycles_(max_cycles) {}

void CycleAccurateSimulator::run() {
    std::cout << "=== Starting Cycle-Accurate Simulation ===" << std::endl;
    std::cout << "Total modules: " << modules_.size() << std::endl;
    std::cout << "Max cycles: " << max_cycles_ << std::endl;
    
    // 初始化事件
    initialize_simulation();
    
    // 主模拟循环
    while (current_cycle_ < max_cycles_ && !simulation_done_) {
        simulate_cycle();
        current_cycle_++;
    }
    
    std::cout << "\n=== Simulation Complete ===" << std::endl;
    std::cout << "Final cycle: " << current_cycle_ << std::endl;
    std::cout << "Total events processed: " << stats_.total_events << std::endl;
    
    print_statistics();
}

void CycleAccurateSimulator::connect_modules(const std::string& src_id, 
                                            const std::string& src_signal,
                                            const std::string& dst_id, 
                                            const std::string& dst_signal) {
    auto src_it = module_map_.find(src_id);
    auto dst_it = module_map_.find(dst_id);
    
    if (src_it != module_map_.end() && dst_it != module_map_.end()) {
        src_it->second->connect_to(src_signal, dst_it->second, dst_signal);
    } else {
        std::cerr << "Warning: Failed to connect modules. Source or target not found." << std::endl;
    }
}

const std::vector<std::shared_ptr<ISimulatable>>& CycleAccurateSimulator::get_all_modules() const {
    return modules_;
}

void CycleAccurateSimulator::initialize_simulation() {
    // 创建初始激励事件
    auto init_event = std::make_shared<Event>();
    init_event->trigger_cycle = 0;
    init_event->exec_cycle = 0;
    init_event->finish_cycle = 0;
    init_event->end_cycle = 0;
    init_event->state = Event::State::EXECUTING;
    init_event->type = Event::Type::MODULE_EVALUATE;
    init_event->action = [this]() {
        std::cout << "[0] Simulation initialized" << std::endl;
        
        // 设置初始输入信号
        for (auto& module : modules_) {
            if (module->get_id() == "input_buffer") {
                module->set_signal_value("data_out", std::any(100), 0);
                module->set_signal_value("valid", std::any(true), 0);
            }
        }
    };
    
    event_queue_.push(init_event);
}

void CycleAccurateSimulator::simulate_cycle() {
    // 步骤1: 处理组合逻辑事件（最高优先级）
    process_combinational_logic();
    
    // 步骤2: 按拓扑深度降序评估所有模块
    stats_.modules_processed = 0;
    for (auto& module : modules_) {
        if (module->evaluate(current_cycle_, event_queue_)) {
            stats_.modules_processed++;
        }
    }
    
    // 步骤3: 处理当前周期所有到期事件
    process_events_at_cycle(current_cycle_);
    
    // 步骤4: 更新统计
    stats_.total_cycles = current_cycle_;
    stats_.total_events = event_queue_.get_total_events_processed();
    
    // 检查结束条件
    check_simulation_complete();
}

void CycleAccurateSimulator::process_combinational_logic() {
    // 处理延迟为0的组合逻辑模块
    for (auto& module : combinational_modules_) {
        module->evaluate(current_cycle_, event_queue_);
    }
}

void CycleAccurateSimulator::process_events_at_cycle(uint64_t cycle) {
    std::vector<EventPtr> events_to_process;
    
    // 收集所有在此时刻需要执行的事件
    while (!event_queue_.empty()) {
        auto next_event = event_queue_.peek();
        if (next_event && next_event->exec_cycle == cycle) {
            events_to_process.push_back(event_queue_.pop());
        } else {
            break;
        }
    }
    
    // 执行事件
    for (auto& event : events_to_process) {
        if (event->state == Event::State::PENDING) {
            event->state = Event::State::EXECUTING;
            
            // 执行事件动作
            if (event->action) {
                event->action();
            }
            
            // 更新事件状态
            if (event->type == Event::Type::MODULE_EVALUATE) {
                // 模块评估事件会在完成时由模块自己处理
            } else {
                event->state = Event::State::ENDED;
            }
            
            // 记录事件处理
            stats_.total_events++;
        }
    }
}

void CycleAccurateSimulator::check_simulation_complete() {
    // 简单结束条件：事件队列为空
    if (event_queue_.empty()) {
        bool all_modules_idle = true;
        for (auto& module : modules_) {
            if (!module->is_available()) {
                all_modules_idle = false;
                break;
            }
        }
        
        if (all_modules_idle) {
            simulation_done_ = true;
            std::cout << "[" << current_cycle_ << "] All modules idle, simulation complete." << std::endl;
        }
    }
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
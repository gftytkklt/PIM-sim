#include "simulator/modules/ProcessingUnit.h"

ProcessingUnit::ProcessingUnit(const std::string& id, uint64_t compute_latency)
    : ModuleBase(id), compute_latency_(compute_latency) {
    // 定义模块信号
    add_signal({"data_in", Signal::Direction::INPUT});
    add_signal({"data_out", Signal::Direction::OUTPUT});
    add_signal({"weight_in", Signal::Direction::INPUT});
    add_signal({"ready", Signal::Direction::INTERNAL, false});
    add_signal({"valid", Signal::Direction::OUTPUT, false});
    
    // 初始化内部状态
    get_signal("ready").value = true;
}

std::vector<std::shared_ptr<Event>> ProcessingUnit::check_triggers(uint64_t current_cycle) {
    std::vector<std::shared_ptr<Event>> events;
    
    auto& data_in = get_signal("data_in");
    auto& weight_in = get_signal("weight_in");
    auto& ready = get_signal("ready");
    
    bool is_ready = false;
    try {
        is_ready = std::any_cast<bool>(ready.value);
    } catch (const std::bad_any_cast&) {
        // 如果类型转换失败，保持false
    }
    
    // 触发条件：输入数据有效、权重有效、且模块就绪
    if (data_in.valid && data_in.valid_cycle <= current_cycle &&
        weight_in.valid && weight_in.valid_cycle <= current_cycle &&
        is_ready) {
        
        auto event = std::make_shared<Event>();
        event->type = Event::Type::MODULE_EVALUATE;
        
        // 捕获当前输入值
        auto input_data = data_in.value;
        auto weight_data = weight_in.value;
        
        event->action = [this, input_data, weight_data, current_cycle]() {
            // 模拟计算过程
            std::cout << "[" << current_cycle << "] PU " << id_ 
                      << ": Processing computation" << std::endl;
            
            // 更新内部状态
            get_signal("ready").value = false;
            
            // 记录MAC操作
            mac_operations_++;
        };
        
        events.push_back(event);
    }
    
    return events;
}

uint64_t ProcessingUnit::get_latency_for_event(const std::shared_ptr<Event>& event) {
    return compute_latency_;
}

void ProcessingUnit::update_output_signals(const std::shared_ptr<Event>& event, uint64_t current_cycle) {
    // 设置输出信号
    get_signal("data_out").value = std::any(42); // 示例输出值
    get_signal("data_out").valid = true;
    get_signal("data_out").valid_cycle = current_cycle;
    
    get_signal("valid").value = true;
    get_signal("valid").valid = true;
    get_signal("valid").valid_cycle = current_cycle;
    
    // 重置就绪状态
    get_signal("ready").value = true;
    
    std::cout << "[" << current_cycle << "] PU " << id_ 
              << ": Output ready" << std::endl;
}

std::unordered_map<std::string, uint64_t> ProcessingUnit::get_module_specific_stats() const {
    return {{"mac_operations", mac_operations_},
            {"compute_latency", compute_latency_}};
}
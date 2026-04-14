#include "modules/MemoryUnit.h"

MemoryUnit::MemoryUnit(const std::string& id, uint64_t read_latency, uint64_t write_latency)
    : ModuleBase(id), read_latency_(read_latency), write_latency_(write_latency) {
    
    add_signal({"addr_in", Signal::Direction::INPUT});
    add_signal({"data_in", Signal::Direction::INPUT});
    add_signal({"data_out", Signal::Direction::OUTPUT});
    add_signal({"wr_en", Signal::Direction::INPUT, false});
    add_signal({"rd_en", Signal::Direction::INPUT, false});
    add_signal({"ready", Signal::Direction::INTERNAL, true});
}

std::vector<std::shared_ptr<Event>> MemoryUnit::check_triggers(uint64_t current_cycle) {
    std::vector<std::shared_ptr<Event>> events;
    
    auto& addr_in = get_signal("addr_in");
    auto& data_in = get_signal("data_in");
    auto& wr_en = get_signal("wr_en");
    auto& rd_en = get_signal("rd_en");
    auto& ready = get_signal("ready");
    
    bool is_wr_en = false;
    bool is_rd_en = false;
    bool is_ready = false;
    
    try {
        is_wr_en = std::any_cast<bool>(wr_en.value);
        is_rd_en = std::any_cast<bool>(rd_en.value);
        is_ready = std::any_cast<bool>(ready.value);
    } catch (const std::bad_any_cast&) {
        // 如果类型转换失败，保持false
    }
    
    if (is_ready && addr_in.valid && addr_in.valid_cycle <= current_cycle) {
        if (is_wr_en && data_in.valid && data_in.valid_cycle <= current_cycle) {
            // 写操作
            auto event = std::make_shared<Event>();
            event->type = Event::Type::MODULE_EVALUATE;
            
            uint64_t addr = 0;
            try {
                addr = std::any_cast<uint64_t>(addr_in.value);
            } catch (const std::bad_any_cast&) {
                addr = 0;
            }
            auto data = data_in.value;
            
            event->action = [this, addr, data, current_cycle]() {
                std::cout << "[" << current_cycle << "] MEM " << id_ 
                          << ": Writing to address " << addr << std::endl;
                
                storage_[addr] = data;
                write_count_++;
                get_signal("ready").value = false;
            };
            
            events.push_back(event);
        } 
        else if (is_rd_en) {
            // 读操作
            auto event = std::make_shared<Event>();
            event->type = Event::Type::MODULE_EVALUATE;
            
            uint64_t addr = 0;
            try {
                addr = std::any_cast<uint64_t>(addr_in.value);
            } catch (const std::bad_any_cast&) {
                addr = 0;
            }
            
            event->action = [this, addr, current_cycle]() {
                std::cout << "[" << current_cycle << "] MEM " << id_ 
                          << ": Reading from address " << addr << std::endl;
                
                if (storage_.find(addr) != storage_.end()) {
                    get_signal("data_out").value = storage_[addr];
                } else {
                    get_signal("data_out").value = std::any(0);
                }
                
                read_count_++;
                get_signal("ready").value = false;
            };
            
            events.push_back(event);
        }
    }
    
    return events;
}

uint64_t MemoryUnit::get_latency_for_event(const std::shared_ptr<Event>& event) {
    bool is_wr_en = false;
    bool is_rd_en = false;
    
    try {
        is_wr_en = std::any_cast<bool>(get_signal("wr_en").value);
        is_rd_en = std::any_cast<bool>(get_signal("rd_en").value);
    } catch (const std::bad_any_cast&) {
        // 如果类型转换失败，保持false
    }
    
    if (is_wr_en) return write_latency_;
    if (is_rd_en) return read_latency_;
    return 1;
}

void MemoryUnit::update_output_signals(const std::shared_ptr<Event>& event, uint64_t current_cycle) {
    bool is_rd_en = false;
    try {
        is_rd_en = std::any_cast<bool>(get_signal("rd_en").value);
    } catch (const std::bad_any_cast&) {
        // 如果类型转换失败，保持false
    }
    
    if (is_rd_en) {
        get_signal("data_out").valid = true;
        get_signal("data_out").valid_cycle = current_cycle;
    }
    
    // 重置控制信号
    get_signal("wr_en").value = false;
    get_signal("rd_en").value = false;
    get_signal("ready").value = true;
    
    // 清除输入有效性
    get_signal("addr_in").valid = false;
    get_signal("data_in").valid = false;
}

std::unordered_map<std::string, uint64_t> MemoryUnit::get_module_specific_stats() const {
    return {
        {"read_count", read_count_},
        {"write_count", write_count_},
        {"storage_size", static_cast<uint64_t>(storage_.size())},
        {"read_latency", read_latency_},
        {"write_latency", write_latency_}
    };
}
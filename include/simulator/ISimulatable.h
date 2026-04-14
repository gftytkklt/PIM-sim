#ifndef ISIMULATABLE_H
#define ISIMULATABLE_H

#include <cstdint>
#include <memory>
#include <string>
#include <any>
#include <typeindex>
#include <unordered_map>

// 前向声明
class EventQueue;
class Event;

/**
 * 模拟模块抽象接口
 * 对应文档5.2.1节的模块原语核心语义
 */
class ISimulatable {
public:
    virtual ~ISimulatable() = default;
    
    // 模块标识
    virtual const std::string& get_id() const = 0;
    virtual std::type_index get_module_type() const = 0;
    
    // 事件驱动接口 (对应算法5.2)
    virtual bool evaluate(uint64_t current_cycle, EventQueue& event_queue) = 0;
    
    // 事件完成处理
    virtual void on_event_finished(std::shared_ptr<Event> event, 
                                   uint64_t current_cycle,
                                   EventQueue& event_queue) = 0;
    
    // 拓扑属性
    virtual int get_topological_depth() const = 0;
    virtual void set_topological_depth(int depth) = 0;
    
    // 控制依赖原语接口 (对应表5-2)
    virtual bool is_available() const = 0;
    virtual void set_available(bool available) = 0;
    
    // 信号管理接口
    virtual bool has_signal(const std::string& name) const = 0;
    virtual std::any get_signal_value(const std::string& name) const = 0;
    virtual void set_signal_value(const std::string& name, 
                                 const std::any& value, 
                                 uint64_t valid_cycle) = 0;
    
    // 连接管理
    virtual void connect_to(const std::string& local_signal, 
                           std::shared_ptr<ISimulatable> target_module,
                           const std::string& target_signal) = 0;
    
    // 性能统计
    virtual void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const = 0;
};

#endif // ISIMULATABLE_H
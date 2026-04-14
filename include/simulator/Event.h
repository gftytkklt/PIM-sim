#ifndef EVENT_HPP
#define EVENT_HPP

#include "ISimulatable.h"
#include <cstdint>
#include <memory>
#include <queue>
#include <vector>
#include <functional>
#include <string>
#include <any>

/**
 * 事件原语
 * 对应文档表5-3的事件原语
 */
struct Event {
    uint64_t trigger_cycle{0};      // 触发时间戳
    uint64_t exec_cycle{0};         // 执行时间戳
    uint64_t finish_cycle{0};       // 完成时间戳
    uint64_t end_cycle{0};          // 结束时间戳
    
    enum class State { 
        PENDING, 
        EXECUTING, 
        FINISHED, 
        ENDED 
    } state{State::PENDING};
    
    enum class Type {
        MODULE_EVALUATE,     // 模块评估事件
        SIGNAL_PROPAGATION,  // 信号传播事件
        COMBINATIONAL_LOGIC, // 组合逻辑事件
        PERFORMANCE_COUNT    // 性能统计事件
    } type{Type::MODULE_EVALUATE};
    
    std::function<void()> action;                     // 功能函数
    std::weak_ptr<ISimulatable> source_module;        // 源模块
    std::weak_ptr<ISimulatable> target_module;        // 目标模块（用于信号传播）
    std::string signal_name;                          // 相关信号名
    std::any signal_value;                            // 信号值
    
    // 事件队列比较器（按执行时间戳排序）
    struct Comparator {
        bool operator()(const std::shared_ptr<Event>& a, 
                       const std::shared_ptr<Event>& b) const;
    };
};
using EventPtr = std::shared_ptr<Event>;

/**
 * 事件队列包装类
 * 对应算法5.2和5.3的事件队列管理
 */
class EventQueue {
private:
    std::priority_queue<EventPtr, std::vector<EventPtr>, Event::Comparator> queue_;
    uint64_t total_events_processed_{0};
    
public:
    EventQueue() = default;
    ~EventQueue() = default;
    
    // 禁止拷贝
    EventQueue(const EventQueue&) = delete;
    EventQueue& operator=(const EventQueue&) = delete;
    
    // 允许移动
    EventQueue(EventQueue&&) = default;
    EventQueue& operator=(EventQueue&&) = default;
    
    void push(const EventPtr& event);
    void push(EventPtr&& event);
    
    EventPtr pop();
    EventPtr peek() const;
    
    bool empty() const;
    size_t size() const;
    
    uint64_t get_total_events_processed() const;
    void clear();
};

#endif // EVENT_H
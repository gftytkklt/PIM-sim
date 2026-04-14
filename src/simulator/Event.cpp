#include "simulator/Event.h"

// Event::Comparator 实现
bool Event::Comparator::operator()(const std::shared_ptr<Event>& a, 
                                  const std::shared_ptr<Event>& b) const {
    // 首先按执行时间戳，其次按类型优先级
    if (a->exec_cycle != b->exec_cycle) {
        return a->exec_cycle > b->exec_cycle; // 最小堆
    }
    // 组合逻辑事件优先级最高
    if (a->type == Event::Type::COMBINATIONAL_LOGIC && 
        b->type != Event::Type::COMBINATIONAL_LOGIC) {
        return false;  // a优先
    }
    if (b->type == Event::Type::COMBINATIONAL_LOGIC && 
        a->type != Event::Type::COMBINATIONAL_LOGIC) {
        return true;   // b优先
    }
    return false;  // 默认FIFO
}

// EventQueue 方法实现
void EventQueue::push(const EventPtr& event) { 
    queue_.push(event); 
}

void EventQueue::push(EventPtr&& event) { 
    queue_.push(std::move(event)); 
}

EventPtr EventQueue::pop() {
    if (queue_.empty()) return nullptr;
    auto event = queue_.top();
    queue_.pop();
    total_events_processed_++;
    return event;
}

EventPtr EventQueue::peek() const { 
    return queue_.empty() ? nullptr : queue_.top(); 
}

bool EventQueue::empty() const { 
    return queue_.empty(); 
}

size_t EventQueue::size() const { 
    return queue_.size(); 
}

uint64_t EventQueue::get_total_events_processed() const { 
    return total_events_processed_; 
}

void EventQueue::clear() {
    while (!queue_.empty()) queue_.pop();
    total_events_processed_ = 0;
}
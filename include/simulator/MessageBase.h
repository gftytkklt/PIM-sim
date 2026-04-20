// MessageTypes.h
#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include <cstdint>
#include <string>
#include <any>
#include <variant>
#include <vector>
#include <functional>

struct GenericMessage {
    std::string task_id;
    uint64_t delay_cycles{0};
    std::any body;
    
    // 便捷构造函数
    GenericMessage() = default;
    GenericMessage(const std::string& tid, const std::any& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(b) {}
    
    template<typename T>
    GenericMessage(const std::string& tid, T&& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(std::forward<T>(b)) {}
};

#endif // MESSAGE_BASE_H
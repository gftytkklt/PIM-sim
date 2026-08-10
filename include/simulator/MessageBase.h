// MessageBase.h
#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include <cstdint>
#include <string>
#include <any>
#include <vector>
#include <functional>

/**
 * 通用消息结构（框架层只约定结构，不预设消息类型）
 *
 * 消息 body 使用 std::any 支持用户自定义任意类型；
 * 消息类型通过 register_message_handler<T>/register_task_handler<T>
 * 模板注册，框架自动登记 body 类型并在发送/分发时校验。
 */
struct GenericMessage {
    std::string task_id;        // 用户自定义任务标识
    uint64_t delay_cycles{0};   // 延迟周期数
    std::any body;              // 用户自定义消息体类型

    // 便捷构造函数
    GenericMessage() = default;
    GenericMessage(const std::string& tid, const std::any& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(b) {}

    template<typename T>
    GenericMessage(const std::string& tid, T&& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(std::forward<T>(b)) {}
};

#endif // MESSAGE_BASE_H
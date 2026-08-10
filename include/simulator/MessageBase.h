// MessageBase.h
#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include <cstdint>
#include <string>
#include <tuple>
#include <variant>
#include <vector>
#include <functional>

// 消息体类型（封闭集合）：
//   monostate          - 无负载消息（如 TS_HELLO）
//   tuple<int,int>     - "init_task"（bank_id, batch_num）
//   tuple<int,string>  - "task_batch_done"（bank_id, core_name/module_id）
//   int                - "SIMD_computation_done"（channel num，死代码预留）
using MessageBody = std::variant<
    std::monostate,
    std::tuple<int, int>,
    std::tuple<int, std::string>,
    int>;

struct GenericMessage {
    std::string task_id;
    uint64_t delay_cycles{0};
    MessageBody body;

    // 便捷构造函数
    GenericMessage() = default;
    GenericMessage(const std::string& tid, const MessageBody& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(b) {}

    template<typename T,
             typename = std::enable_if_t<!std::is_same_v<std::decay_t<T>, MessageBody>>>
    GenericMessage(const std::string& tid, T&& b, uint64_t delay = 0)
        : task_id(tid), delay_cycles(delay), body(std::forward<T>(b)) {}
};

#endif // MESSAGE_BASE_H
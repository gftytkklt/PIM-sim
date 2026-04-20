// MessageTypes.h
#ifndef MESSAGE_BASE_H
#define MESSAGE_BASE_H

#include <cstdint>
#include <string>
#include <any>
#include <variant>
#include <vector>
#include <functional>

// 消息类型枚举
enum class MessageType {
    TASK_COMPLETED,      // 任务完成消息
    DATA_READY,          // 数据就绪消息
    CORE_AVAILABLE,      // 核心可用状态更新
    DEPENDENCY_UPDATE,   // 依赖关系更新
    SCHEDULE_NEW_TASK,   // 调度新任务
    PERFORMANCE_REPORT,   // 性能报告
    CUSTOM               // 自定义消息类型
};

// 消息头（元数据）
// 当前不在module中处理依赖关系，依赖关系由模拟器的事务目录维护
// 如果模块实现了对通信目的地址的建模，也可以传递对应的消息，但可以放在Body里。
struct MessageHeader {
    MessageType type;
    std::string source_core_id;     // 源核心ID
    std::string destination_core_id;// 目标核心ID（如果为空表示发送给模拟器）
    std::string task_id;           // 关联的任务ID
    uint64_t timestamp;            // 消息时间戳
    uint64_t delay_cycles;         // 延迟周期数（多少周期后生效）
};

// 消息体（通用结构）
template<typename... Args>
struct MessageBody {
    std::tuple<Args...> data;  // 使用tuple存储任意数量和类型的参数
    
    template<typename... Ts>
    explicit MessageBody(Ts&&... args) 
        : data(std::forward<Ts>(args)...) {}
    
    // 获取第N个参数
    template<size_t N>
    auto get() -> decltype(std::get<N>(data)) {
        return std::get<N>(data);
    }
    
    // 遍历所有参数的辅助函数
    template<typename Visitor>
    void visit(Visitor&& visitor) {
        std::apply([&visitor](auto&&... args) {
            (visitor(std::forward<decltype(args)>(args)), ...);
        }, data);
    }
};

// 通用消息包装器
class GenericMessage {
private:
    MessageHeader header_;
    std::any body_;
    
public:
    GenericMessage(MessageHeader header, std::any body)
        : header_(std::move(header)), body_(std::move(body)) {}
    
    const MessageHeader& header() const { return header_; }
    
    template<typename T>
    bool is() const {
        return body_.type() == typeid(T);
    }
    
    template<typename T>
    const T& as() const {
        return std::any_cast<const T&>(body_);
    }
    
    template<typename T>
    T& as() {
        return std::any_cast<T&>(body_);
    }
    
    // 便捷构造函数
    template<typename T>
    static GenericMessage create(MessageHeader header, T&& data) {
        return GenericMessage(std::move(header), std::make_any<T>(std::forward<T>(data)));
    }
};

#endif // MESSAGE_BASE_H
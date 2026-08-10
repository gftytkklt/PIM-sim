#ifndef ISIMULATABLE_H
#define ISIMULATABLE_H

#include <cstdint>
#include <memory>
#include <string>
#include <any>
#include <typeindex>
#include <typeinfo>
#include <unordered_map>
#include <vector>
#include "Process.h"
#include "MessageBase.h"
#include "SimulatorEvent.h"

/**
 * 信号定义（框架层约定信号的结构，具体信号由用户建模时声明）
 */
struct Signal {
    std::string name;                       // 用户自定义信号名
    enum class Direction { INPUT, OUTPUT, INTERNAL };
    Direction direction;
    bool valid{false};
    uint64_t valid_cycle{0};
    std::any value;                         // 信号值（内部 std::any 存储）
    std::type_index value_type{typeid(void)}; // 信号值类型（登记，用于连接/赋值校验）

    template<typename T>
    Signal(const std::string& n, Signal::Direction d, T&& v)
        : name(n), direction(d), value(std::forward<T>(v)),
          value_type(value.type()) {}

    Signal(const std::string& n, Signal::Direction d)
        : name(n), direction(d) {}

    Signal() = default;
};

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
    // 返回本周期产生的待调度事件（信号更新/消息发送），由模拟器统一入队
    virtual std::vector<SimulatorEvent> evaluate(uint64_t current_cycle) = 0;
    virtual const std::vector<ProcessEventPtr>& get_active_processes() const = 0;
    virtual const std::vector<ProcessEventPtr>& get_completed_processes() const = 0;
    
    // 消息传递接口
    virtual void handle_message(const GenericMessage& msg) = 0;
    
    // 拓扑属性
    virtual int get_topological_depth() const = 0;
    virtual void set_topological_depth(int depth) = 0;
    
    // 信号管理接口（信号名由用户建模时自定义，模拟器统一管理）
    virtual bool has_signal(const std::string& name) const = 0;
    virtual std::any get_signal_value(const std::string& name) const = 0;
    virtual void set_signal_value(const std::string& name, 
                                 const std::any& value, 
                                 uint64_t valid_cycle) = 0;
    // 查询信号声明信息（名称/方向/值类型），供模拟器注册表收集
    virtual std::vector<std::pair<std::string, Signal::Direction>> get_signal_declarations() const = 0;
    // 查询信号值类型（用于连接/赋值校验）
    virtual std::type_index get_signal_value_type(const std::string& name) const = 0;
    // 查询信号方向（用于连接校验：OUTPUT → INPUT）
    virtual Signal::Direction get_signal_direction(const std::string& name) const = 0;
    
    // 连接管理
    virtual void connect_to(const std::string& local_signal, 
                           std::shared_ptr<ISimulatable> target_module,
                           const std::string& target_signal) = 0;
    
    // 性能统计
    virtual void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const = 0;
};

#endif // ISIMULATABLE_H
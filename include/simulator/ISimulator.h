#ifndef ISIMULATOR_H
#define ISIMULATOR_H

#include <cstdint>
#include <memory>
#include <string>
#include <functional>
#include <typeindex>
#include <typeinfo>
#include <vector>
#include <any>
#include "ISimulatable.h"
#include "MessageBase.h"

class SignalUpdateEvent;

// 任务处理器类型
using TaskHandler = std::function<void(const GenericMessage&)>;

/**
 * 模拟器抽象接口
 *
 * 面向接口编程：测试/工具代码依赖本接口，可注入 mock 实现。
 * 模板方法（register_module/get_module/register_task_handler）
 * 在接口层定义（保持类型安全），通过虚钩子（*_impl）委托给
 * 实现层存储。引擎方法（simulate_cycle 等）为 protected virtual，
 * 测试可继承实现类并覆盖以验证单周期事件流。
 */
class ISimulator {
public:
    virtual ~ISimulator() = default;

    // ========== 非模板虚方法 ==========

    // 连接模块（信号注册表验证：存在性/方向/类型）
    virtual void connect_modules(const std::string& src_id, const std::string& src_signal,
                                 const std::string& dst_id, const std::string& dst_signal) = 0;

    // 运行模拟
    virtual void run() = 0;

    // 获取当前周期
    virtual uint64_t get_current_cycle() const = 0;

    // 检查模拟是否完成
    virtual bool is_simulation_done() const = 0;

    // 获取所有模块（类型擦除版本）
    virtual const std::vector<std::shared_ptr<ISimulatable>>& get_all_modules() const = 0;

    // dump完成的事件到文件
    virtual void dump_completed_events(const std::string& filename) const = 0;

    // 发送消息到核心（直发，不经消息队列）
    virtual void send_message_to_core(const std::string& core_id, const GenericMessage& msg) = 0;

    // ========== 模板方法（类型安全，委托虚钩子） ==========

    // 注册模块：构造具体类型 + 注册进程/消息 + 委托 register_module_impl 存储
    template<typename ModuleType, typename... Args>
    std::shared_ptr<ModuleType> register_module(const std::string& id, int topological_depth, Args... args) {
        auto module = std::make_shared<ModuleType>(id, std::forward<Args>(args)...);
        module->set_topological_depth(topological_depth);
        module->register_processes();
        module->register_message_handlers();
        register_module_impl(module, id, topological_depth);
        return module;
    }

    // 获取模块（带类型检查）
    template<typename ModuleType>
    std::shared_ptr<ModuleType> get_module(const std::string& id) {
        auto module = get_module_impl(id);
        if (module && module->get_module_type() == typeid(ModuleType)) {
            return std::static_pointer_cast<ModuleType>(module);
        }
        return nullptr;
    }

    // 注册任务处理器（通用版：handler 接收 GenericMessage，不登记消息类型）
    template<typename Func>
    void register_task_handler(const std::string& task_id, Func&& handler) {
        register_task_handler_impl(task_id, std::forward<Func>(handler));
    }

    // 注册类型化任务处理器（推荐）
    // handler 接收 const T&，框架自动登记消息体类型 T 并在分发时校验
    template<typename T>
    void register_task_handler(const std::string& task_id, std::function<void(const T&)> handler) {
        TaskHandler wrapper = [handler = std::move(handler)](const GenericMessage& msg) {
            try {
                handler(std::any_cast<const T&>(msg.body));
            } catch (const std::bad_any_cast&) {
                throw std::runtime_error("Task message type mismatch for '" + msg.task_id +
                                         "': expected " + std::string(typeid(T).name()) +
                                         ", got " + msg.body.type().name());
            }
        };
        register_task_handler_impl(task_id, std::move(wrapper), std::type_index(typeid(T)));
    }

    // 便捷版本：发送消息到核心（构造 GenericMessage 后调用虚方法）
    template<typename T>
    void send_message_to_core(const std::string& core_id,
                              const std::string& task_id,
                              T&& data) {
        send_message_to_core(core_id, GenericMessage(task_id, std::forward<T>(data)));
    }

    // 提交信号更新事件（供模块/测试调度信号）
    virtual void schedule_signal_update(const SignalUpdateEvent& event) = 0;

protected:
    // ========== 虚钩子（实现层存储/注册，测试可覆盖） ==========

    // 注册已构造的模块实例（设置深度、注册进程/消息、收集信号、存储）
    virtual void register_module_impl(std::shared_ptr<ISimulatable> module,
                                      const std::string& id, int topological_depth) = 0;

    // 按 id 获取模块实例（不含类型检查）
    virtual std::shared_ptr<ISimulatable> get_module_impl(const std::string& id) = 0;

    // 注册通用任务处理器
    virtual void register_task_handler_impl(const std::string& task_id, TaskHandler handler) = 0;

    // 注册类型化任务处理器（带消息体类型登记）
    virtual void register_task_handler_impl(const std::string& task_id, TaskHandler handler,
                                            std::type_index msg_type) = 0;

    // ========== 引擎钩子（测试插入点，覆盖验证单周期事件流） ==========

    virtual void simulate_cycle() = 0;
    virtual void process_message_events(uint64_t current_cycle) = 0;
    virtual void process_signal_events(uint64_t current_cycle) = 0;
};

#endif // ISIMULATOR_H
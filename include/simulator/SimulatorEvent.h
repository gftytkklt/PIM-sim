#ifndef SIMULATOR_EVENT_H
#define SIMULATOR_EVENT_H

#include <cstdint>
#include <memory>
#include <any>
#include "signals.h"
#include "MessageBase.h"

class ISimulatable;

// 统一事件类型：模块通过 evaluate() 返回，由模拟器统一调度
struct SimulatorEvent {
    enum class Kind { SIGNAL_UPDATE, MESSAGE_SEND };

    Kind kind;
    uint64_t cycle;      // 绝对生效周期（模块在 evaluate(current_cycle) 内已知道当前周期）
    std::weak_ptr<ISimulatable> src_module;  // 源模块
    SignalID signal_name;                    // 仅 SIGNAL_UPDATE 有效
    std::any signal_value;                   // 仅 SIGNAL_UPDATE 有效
    GenericMessage message;                  // 仅 MESSAGE_SEND 有效

    // 工厂：信号更新事件
    static SimulatorEvent make_signal(uint64_t cycle,
                                      std::shared_ptr<ISimulatable> src,
                                      SignalID name,
                                      const std::any& value) {
        SimulatorEvent ev;
        ev.kind = Kind::SIGNAL_UPDATE;
        ev.cycle = cycle;
        ev.src_module = src;
        ev.signal_name = name;
        ev.signal_value = value;
        return ev;
    }

    // 工厂：消息事件
    static SimulatorEvent make_message(uint64_t cycle,
                                       std::shared_ptr<ISimulatable> src,
                                       GenericMessage msg) {
        SimulatorEvent ev;
        ev.kind = Kind::MESSAGE_SEND;
        ev.cycle = cycle;
        ev.src_module = src;
        ev.message = std::move(msg);
        return ev;
    }
};

#endif
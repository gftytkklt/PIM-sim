#ifndef CORE_FACTORY_H
#define CORE_FACTORY_H

#include "simulator/Simulator.h"
#include "simulator/tile2_0/config.h"
#include "simulator/tile2_0/Crossbar.h"
#include "simulator/tile2_0/SIMD.h"
#include "simulator/tile2_0/L1C.h"
#include "simulator/tile2_0/TaskScheduler.h"

// 工厂函数：创建标准 core 模块组 (SIMD + Crossbar + TaskScheduler + L1C) 并连接
template<typename SimulatorType>
inline void create_core_modules(SimulatorType& sim, const std::string& suffix,
                                 const std::array<FmapTask, L1C_BANK>& tasks) {
    sim.template register_module<SIMD>("simd" + suffix, 4);
    sim.template register_module<Crossbar>("crossbar" + suffix, 3);
    sim.template register_module<TaskScheduler>("task_scheduler" + suffix, 2, tasks);
    sim.template register_module<L1C>("L1_cache" + suffix, 1);

    auto ts = "task_scheduler" + suffix;
    auto l1c = "L1_cache" + suffix;
    auto xb = "crossbar" + suffix;
    auto sd = "simd" + suffix;
    sim.connect_modules(ts, SignalID::cache_read_trigger, l1c, SignalID::cache_read_trigger);
    sim.connect_modules(ts, SignalID::cache_read_len, l1c, SignalID::cache_read_len);
    sim.connect_modules(ts, SignalID::cache_write_trigger, l1c, SignalID::cache_write_trigger);
    sim.connect_modules(ts, SignalID::cache_write_len, l1c, SignalID::cache_write_len);
    sim.connect_modules(l1c, SignalID::cache_read_done, ts, SignalID::cache_read_valid);
    sim.connect_modules(l1c, SignalID::cache_write_done, ts, SignalID::cache_write_done);
    sim.connect_modules(ts, SignalID::xbar_computation_trigger, xb, SignalID::computation_trigger);
    sim.connect_modules(ts, SignalID::xbar_switching_trigger, xb, SignalID::switching_trigger);
    sim.connect_modules(xb, SignalID::switching_done, ts, SignalID::xbar_switching_done);
    sim.connect_modules(xb, SignalID::computation_done, ts, SignalID::xbar_computation_done);
    sim.connect_modules(ts, SignalID::pooling_enabled, sd, SignalID::SIMD_pooling_enable);
    sim.connect_modules(sd, SignalID::SIMD_data_valid, ts, SignalID::SIMD_computation_done);
    sim.connect_modules(xb, SignalID::computation_done, sd, SignalID::SIMD_channel_batch);
}

#endif
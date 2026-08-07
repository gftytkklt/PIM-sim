# PIM-sim 模拟器说明

本目录（`src/simulator/`）与 `include/simulator/` 共同构成 PIM-sim 的周期精确
（cycle-accurate）存算架构模拟器，实现基于事件驱动的微架构性能仿真。

---

## 1. 总体定位

模拟器接收算子映射前端生成的推理任务与硬件映射结果，通过事件驱动仿真输出
延迟、吞吐量、功耗等性能指标。核心设计：

- **与微架构细节解耦**：用户以"模块 + 信号 + 事件"描述微架构行为
- **周期精确事件驱动**：自动维护事件队列的执行顺序与触发时序
- **统一建模范式**：为异构功能组件提供统一的数据接口

```
算子映射前端 (Analyzer → CGraph → TGraph → HGraph → DGraph)
                    │ 映射结果 / 任务负载
                    ▼
         ┌───────────────────────────────────────┐
         │  模拟器 (src/simulator)               │
         │  ISimulatable → ModuleBase            │
         │    ├─ Crossbar / SIMD / L1C / TS      │  (单核 tile2_0)
         │    └─ Banking / MultiCore / Tiling    │  (多核上层封装)
         │  事件驱动引擎 (Simulator.h)            │
         │  ProcessManager 状态机 (Process.h)    │
         └───────────────────────────────────────┘
                    │ 性能统计 (周期/事件/占用)
                    ▼
             延迟/吞吐量/功耗结果
```

---

## 2. 目录结构与文件职责

### 2.1 模拟器框架层（`include/simulator/`, `src/simulator/`）

| 文件 | 职责 |
|------|------|
| `ISimulatable.h` | 模块抽象接口 + `Signal` 信号结构 |
| `ModuleBase.h` | 模块基类（信号管理、事件缓冲、消息处理） |
| `Process.h` / `Proecss.cpp` | `ProcessEvent`（事件实例）、`ProcessType`（进程类型）、`ProcessManager`（状态机） |
| `Simulator.h` / `Simulator.cpp` | `CycleAccurateSimulator`（事件驱动引擎、信号注册表、连接管理） |
| `SimulatorEvent.h` | 统一事件类型（信号更新/消息发送），`evaluate()` 返回值 |
| `MessageBase.h` | `GenericMessage` 通用消息（task_id + body） |

### 2.2 Tile2.0 架构模型层（`include/simulator/tile2_0/`, `src/simulator/tile2_0/`）

| 文件 | 职责 |
|------|------|
| `config.h` | `hw_config` 命名空间硬件参数（constexpr） |
| `Crossbar.h/.cpp` | 存算阵列：计算 + 切换 |
| `SIMD.h/.cpp` | 后处理：量化→激活→池化流水 |
| `L1C.h/.cpp` | 本地缓存（多 bank SRAM） |
| `TaskScheduler.h/.cpp` | 任务调度：数据搬运 + 计算触发 |
| `core_factory.h` | `create_core_modules()` 核心模块工厂 |
| `membanking.h/.cpp` | 内存 banking 模拟（XY/YX/Custom 策略） |
| `multicore.h/.cpp` | 多核并行模拟（6 核，硬编码依赖） |
| `tiling.h/.cpp` | 动态 banking tiling 模拟 |
| `tile2_0.h/.cpp` | OPU 单 tile 全流水线模拟 |

---

## 3. 模拟机制

### 3.1 事件状态机（`ProcessEvent`）

`ProcessEvent` 定义五阶段状态：

```
IDLE ──触发──▶ TRIGGERED ──执行──▶ EXECUTING ──完成──▶ FINISHED ──结束──▶ ENDED
  (空闲)        (已触发)            (执行中)            (执行完成)        (事件结束)
```

`ProcessEvent` 维护四类时间戳（触发/执行/完成/结束），`get_timing_stats()`
返回各类延迟（trigger→exec、exec→finish、finish→end、total）。

### 3.2 状态转移驱动（`ProcessManager::drive_state_transitions`）

每个模块在 `evaluate(current_cycle)` 时调用，流程：

1. **触发检查**：遍历 `process_types_`，若无活跃同类事件且触发条件满足，
   `create_active_event` 创建新事件。
2. **状态推进**：对每个活跃事件，用 `while(state_changed)` 循环检查
   `check_exec/check_finish/check_end`，允许单周期内连续状态转换。
3. **清理**：`cleanup_ended_events()` 将 ENDED 事件移入 `completed_events_`
   供性能统计。

### 3.3 事件调度（`CycleAccurateSimulator`）

- 每个模块的 `evaluate()` 返回 `std::vector<SimulatorEvent>`。
- 模块内部通过 `submit_signal_value()` / `submit_message()` 将事件累积到
  `pending_events_`。
- 模拟器在 `simulate_cycle()` 中收集各模块事件，`dispatch_simulator_event()`
  将**相对延迟周期** + `current_cycle_` 转为**绝对生效周期**，推入
  `signal_event_queue_` / `message_queue_`（最小堆优先队列）。

```
simulate_cycle():
  1. process_message_events()      # 处理到期消息
  2. process_signal_events()       # 处理到期信号
  3. process_combinational_logic() # 组合逻辑模块
  4. 对每个 module: evaluate() → dispatch_simulator_event()
  5. check_simulation_complete()
```

### 3.4 仿真拓扑序

事件按**拓扑深度降序**（逆流水线方向）执行，保证"在 T 时刻所有输入数据
已就绪，且当前输出更新不影响同一时刻未执行事件的输入"。
`register_module` 按拓扑深度排序 `modules_`。

### 3.5 信号系统（结构类型化 + 运行时注册表）

信号系统只约定结构，不预设具体信号：

```cpp
struct Signal {
    std::string name;          // 用户建模时自定义
    Direction direction;       // INPUT / OUTPUT / INTERNAL
    std::any value;            // 信号值
    std::type_index value_type; // 值类型（std::any 取内部实际类型）
};
```

**用户建模层**：在模块构造函数中 `add_signal(...)` 声明自己的信号
（名称、数量、方向、类型完全由用户决定）。

**模拟器统一管理**（`CycleAccurateSimulator`）：
- `register_module()` 自动收集模块信号声明到 `signal_registry_`
  （模块ID → 信号名 → {方向, 值类型}）。
- `connect_modules()` 自动验证：信号存在性、方向（源 OUTPUT → 目标 INPUT）、
  值类型匹配。
- `submit_signal_value()` 校验赋值类型与声明类型一致。

### 3.6 模块执行链路（`ModuleBase`）

模块基类职责：
- **信号管理**：`signals_` 映射 + `add_signal/get_signal/get_signal_as<T>/set_signal_value/clear_signal/invalidate_signal`
- **事件缓冲**：`pending_events_` + `submit_signal_value/submit_message` 累积，`evaluate()` 返回并清空
- **进程管理**：`process_manager_`（unique_ptr）+ `register_process/bind_signal_to_process/get_processes_by_signal`
- **消息处理**：`message_handlers_` + `register_message_handler/handle_message`
- **信号注册表查询**：`get_signal_declarations/get_signal_value_type/get_signal_direction`

### 3.7 多核事务机制

多核模拟通过三层抽象扩展单核事件驱动：

| 抽象 | 含义 | 实现 |
|------|------|------|
| **事务构造** | 计算图节点抽象为事务 | 每个核心一个 `CycleAccurateSimulator` 实例 |
| **事务分配** | 硬件映射 | 核心/模块注册（`register_module`） |
| **事务间消息传递** | 多核调度 | `GenericMessage` + `task_handlers_` 消息分发 |

**任务依赖表**：`core_batch_num_map_`（记录每个核心/ bank 的批次数），
监视多核数据传输触发条件。

**消息**：`GenericMessage` 封装核间通信事件参数
（`task_id` + `body` + `delay_cycles`），驱动顶层任务状态更新。

当前多核实现（`multicore.cpp`）以 6 核硬编码依赖关系
（`handle_batch_task_done` 的 switch 逻辑）模拟数据依赖反压与计算批次模式。

---

## 4. 关键功能与接口

### 4.1 核心公共接口

**`ISimulatable`（模块抽象接口）**

```cpp
class ISimulatable {
    virtual std::vector<SimulatorEvent> evaluate(uint64_t current_cycle) = 0;
    virtual bool has_signal(const std::string& name) const = 0;
    virtual std::any get_signal_value(const std::string& name) const = 0;
    virtual void set_signal_value(const std::string& name, const std::any& value, uint64_t valid_cycle) = 0;
    virtual std::vector<std::pair<std::string, Signal::Direction>> get_signal_declarations() const = 0;
    virtual std::type_index get_signal_value_type(const std::string& name) const = 0;
    virtual Signal::Direction get_signal_direction(const std::string& name) const = 0;
    virtual void connect_to(const std::string& local_signal,
                            std::shared_ptr<ISimulatable> target_module,
                            const std::string& target_signal) = 0;
    virtual void get_performance_stats(std::unordered_map<std::string, uint64_t>& stats) const = 0;
};
```

**`ModuleBase`（模块基类，用户建模继承点）**

用户需实现两个纯虚函数：
- `register_processes()`：注册模块的进程类型（触发/执行/完成/结束条件 + 延迟）
- `register_message_handlers()`：注册模块的消息处理函数

**`CycleAccurateSimulator`（模拟器引擎）**

```cpp
template<typename ModuleType, typename... Args>
std::shared_ptr<ModuleType> register_module(const std::string& id, int topological_depth, Args... args);
void connect_modules(const std::string& src_id, const std::string& src_signal,
                     const std::string& dst_id, const std::string& dst_signal);
void run();
template<typename Func>
void register_task_handler(const std::string& task_id, Func&& handler);
void send_message_to_core(const std::string& core_id, const GenericMessage& msg);
uint64_t get_current_cycle() const;
void dump_completed_events(const std::string& filename) const;
```

### 4.2 信号注册表

```cpp
struct SignalMeta {
    Signal::Direction direction;
    std::type_index value_type;
};
std::unordered_map<std::string, std::unordered_map<std::string, SignalMeta>> signal_registry_;
```

`connect_modules` 的三重验证（缺失即抛 `std::runtime_error`）：
1. **存在性**：源/目标信号必须在注册表中
2. **方向**：源必须是 OUTPUT，目标必须是 INPUT
3. **类型**：源信号值类型与目标声明类型一致

### 4.3 性能统计

每个模块维护 `performance_stats_`，`get_performance_stats()` 合并 `get_process_stats()`：
- `total_evaluations`：模块被 evaluate 的次数（= 模拟周期数）
- `signal_updates`：提交的信号更新事件数
- `ProcessManager::get_performance_stats()`：
  - `total_completed_events` / `total_latency` / `avg_latency`
  - `busy_time` / `min_latency` / `max_latency`

模拟器级 `SimulationStats`：`total_cycles` / `total_events` / `modules_processed`。

---

## 5. 建模工作流

### 5.1 新增一个模块类型

```cpp
// 1. 头文件: 继承 ModuleBase
class MyModule : public ModuleBase {
public:
    MyModule(const std::string& id) : ModuleBase(id) {
        add_signal(Signal("my_in", Signal::Direction::INPUT));
        add_signal(Signal("my_out", Signal::Direction::OUTPUT, 0));
    }
    void register_processes() override {
        register_process("my_proc",
            [this]() { return check_trigger(); },   // 触发条件
            [this]() { return check_exec(); },      // 执行条件（可含功能逻辑）
            [this]() { return check_finish(); },    // 完成条件
            [this]() { return check_end(); },       // 结束条件
            latency_);
    }
    void register_message_handlers() override {}
private:
    bool check_trigger();
    bool check_exec();
    bool check_finish();
    bool check_end();
};

// 2. 实现 check_* 条件函数，内部调用 get_signal_as<T>() 读取输入、
//    submit_signal_value() 提交输出
```

### 5.2 在模拟器中实例化并连接

```cpp
class MySimulator : public CycleAccurateSimulator {
public:
    void Init() {
        register_module<MyModule>("mod0", 1);
        register_module<OtherModule>("other0", 0);
        connect_modules("mod0", "my_out", "other0", "other_in");
    }
};
```

`register_module` 自动收集信号声明到注册表，`connect_modules` 自动验证连接合法性。

---

## 6. 测试

8 个 gtest 测试文件（9 个可执行）：

| 测试 | 覆盖 |
|------|------|
| `process_test.cpp` | ProcessEvent 状态机/时间戳；ProcessManager 注册/驱动/清理/统计 |
| `modulebase_test.cpp` | ModuleBase 信号管理、`get_signal_as<T>` 类型安全、信号声明/类型登记 |
| `connect_test.cpp` | 连接验证（合法/缺失信号/方向错误/类型不匹配） |
| `config_test.cpp` | `hw_config` constexpr 与宏一致性 |
| `oputiletest.cpp` | OPU 单 tile 全流水线（周期=5141） |
| `bankingtest.cpp` | BankingSimulator XY/YX/Custom 策略（周期=23024） |
| `multicoretest.cpp` | MulticoreSimulator 多核并行（周期=6867） |
| `tilingtest.cpp` | TilingSimulator 动态策略 |
| `mappingalexnet.cpp` | Analyzer 全流程（无模拟器，ASan 0 泄漏） |

> 注意：ASan 构建下模拟器测试会运行到 max_cycles 而非提前终止
> （TaskScheduler 进程在 ASan 下不完成），为预存现象。时序正确性用
> 普通构建验证，ASan 仅用于内存/泄漏检测。

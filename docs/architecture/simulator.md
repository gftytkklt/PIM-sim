# PIM-sim 模拟器说明

> 本文件为模拟器专项文档，聚焦 `src/cpp/simulator/` 与 `include/simulator/` 的
> 框架功能、接口、抽象层级与模拟机制。

---

## 1. 总体定位

模拟器提供周期单位的事件引擎、聚合 Tile 任务模型和 Tile2.0 架构示例。模块行为、
任务负载、完成判据由架构或调用方明确给出。实际映射通过 `export_workload` 导出，
由 `simulate_workload` 执行，字段和参数见[任务负载契约](workload.md)。
`pim_sim` 的 MNSIM/Booksim 聚合分析提供独立的延迟/功率估计；
事件引擎统计周期、事件和进程时间，功耗需要另外的硬件模型。

核心设计原则（与信号/消息系统的统一规范一致）：

- **框架约定结构，用户定义类型**：框架只定义结构（`Signal`/`GenericMessage`），
  具体信号名、消息类型、模块行为由用户建模时自定义
- **周期精确事件驱动**：自动维护事件队列的执行顺序与触发时序
- **统一建模范式**：为异构功能组件（数字电路、易失存储、非易失存算器件）
  提供统一的数据接口

```
算子映射前端 (Analyzer → CGraph → TGraph → HGraph → DGraph)
                    │ export_workload → aggregate_tile_v1
                    ▼
         ┌───────────────────────────────────────┐
         │  模拟器 (src/cpp/simulator)               │
         │  ISimulator (抽象接口)                 │
         │  CycleAccurateSimulator (事件驱动引擎)  │
         │  ISimulatable → ModuleBase (模块)      │
         │    ├─ Crossbar / SIMD / L1C / TS      │  (单核 tile2_0)
         │    └─ Banking / MultiCore / Tiling    │  (架构封装)
         │  ProcessManager 状态机 (Process.h)    │
         └───────────────────────────────────────┘
                    │ 性能统计 (周期/事件/占用)
                    ▼
             周期/事件轨迹（功耗需独立模型）
```

---

## 2. 抽象层级

模拟器分为三层，职责分离：

```
ISimulator (抽象接口)          —— 契约：定义能力，不持有数据
    ▲ 继承
CycleAccurateSimulator (引擎)  —— 引擎+状态：持有全部数据，实现虚钩子
    ▲ 继承
OPU/Banking/MultiCore/Tiling   —— 架构封装：Init() 装配模块图
```

### 2.1 ISimulator（抽象接口层）—— 契约

- 定义**有什么能力**（连接/运行/注册/调度），不实现任何数据
- **模板方法** = 类型安全"前端处理器"：构造具体模块、调用具体模块方法、
  做类型转换（编译期展开，运行时零开销）
- **虚钩子**（`*_impl`）= 契约中的存储/引擎插入点（声明，交给实现层）
- **引擎钩子**（`protected virtual`）= 测试插入点（可覆盖验证单周期事件流）
- 作用：调用方（架构封装、ConfigLoader、测试）只依赖接口，不关心底层实现

```cpp
// 模板方法在接口层定义，保持类型安全（应用端接口不变）
template<typename ModuleType, typename... Args>
std::shared_ptr<ModuleType> register_module(const std::string& id, int depth, Args... args) {
    auto module = std::make_shared<ModuleType>(id, std::forward<Args>(args)...);
    module->set_topological_depth(depth);
    module->register_processes();               // 具体模块 public 覆盖
    module->register_message_handlers();
    register_module_impl(module, id, depth);    // 虚钩子：运行时委托实现层
    return module;
}
```

### 2.2 CycleAccurateSimulator（引擎实现层）—— 引擎 + 状态

- **持有全部数据**：`signal_event_queue_` / `message_queue_` / `task_handlers_` /
  `module_map_` / `signal_registry_` / `modules_` / `current_cycle_` 等
- **实现虚钩子**：
  - `register_module_impl`：信号收集 + 存储 + 拓扑排序
  - `register_task_handler_impl`（×2）：存 handler + 消息类型登记
  - `get_module_impl`：查 map
- **实现引擎方法**：`simulate_cycle` / `process_message_events` /
  `process_signal_events` / `dispatch_simulator_event`（事件驱动核心循环）
- **实现非模板虚方法**：`connect_modules` / `run` / `send_message_to_core` /
  `dump_completed_events` 等

### 2.3 架构封装层（OPU/Banking/MultiCore/Tiling）—— 拓扑定义

- **不写引擎逻辑**，只在 `Init()` 调用接口方法装配模块图
- 职责：用 `ISimulator` 公共 API 描述"这个架构有哪些模块、怎么连"
- 架构与引擎解耦：换架构只改 `Init()`，换引擎（mock）不改 `Init()`

---

## 3. 目录结构与文件职责

### 3.1 模拟器框架层（`include/simulator/`, `src/cpp/simulator/`）

| 文件 | 职责 |
|------|------|
| `ISimulator.h` | 模拟器抽象接口（虚方法 + 模板方法 + 虚钩子 + 引擎钩子） |
| `Simulator.h` / `Simulator.cpp` | `CycleAccurateSimulator`（事件驱动引擎、信号注册表、连接管理） |
| `ISimulatable.h` | 模块抽象接口 + `Signal` 信号结构 |
| `ModuleBase.h` | 模块基类（信号管理、事件缓冲、消息处理） |
| `Process.h` / `Proecss.cpp` | `ProcessEvent`（事件实例）、`ProcessType`（进程类型）、`ProcessManager`（状态机） |
| `SimulatorEvent.h` | 统一事件类型（信号更新/消息发送），`evaluate()` 返回值 |
| `MessageBase.h` | `GenericMessage` 通用消息（task_id + body，body 为 std::any） |
| `ConfigLoader.h` | 配置驱动模块图加载器（JSON → 模块图） |
| `TaskDependency.h` | 多核事务：任务依赖原语（生产-消费屏障 + 活跃任务表） |

### 3.2 Tile2.0 架构模型层（`include/simulator/tile2_0/`, `src/cpp/simulator/tile2_0/`）

| 文件 | 职责 |
|------|------|
| `config.h` | `hw_config` 命名空间硬件参数（constexpr） |
| `Crossbar.h/.cpp` | 存算阵列：计算 + 切换 |
| `SIMD.h/.cpp` | 后处理：量化→激活→池化流水 |
| `L1C.h/.cpp` | 本地缓存（多 bank SRAM） |
| `TaskScheduler.h/.cpp` | 任务调度：数据搬运 + 计算触发 |
| `core_factory.h` | `create_core_modules()` 核心模块工厂 |
| `tile2_0.h/.cpp` | OPU 单 tile 全流水线模拟（架构封装） |
| `membanking.h/.cpp` | 内存 banking 模拟（XY/YX/Custom 策略，架构封装） |
| `multicore.h/.cpp` | 多核并行模拟（6 核硬编码依赖，架构封装） |
| `tiling.h/.cpp` | 动态 banking tiling 模拟（架构封装） |

---

## 4. 模拟机制

### 4.1 事件状态机（`ProcessEvent`）

`ProcessEvent` 定义五阶段状态：

```
IDLE ──触发──▶ TRIGGERED ──执行──▶ EXECUTING ──完成──▶ FINISHED ──结束──▶ ENDED
  (空闲)        (已触发)            (执行中)            (执行完成)        (事件结束)
```

`ProcessEvent` 维护四类时间戳（触发/执行/完成/结束），`get_timing_stats()`
返回各类延迟（trigger→exec、exec→finish、finish→end、total）。

### 4.2 状态转移驱动（`ProcessManager::drive_state_transitions`）

每个模块在 `evaluate(current_cycle)` 时调用，流程：

1. **触发检查**：按进程注册顺序检查；同类型没有活跃事件且触发条件成立时创建事件。
   每类型每时钟周期至多触发一次，且旧事件结束的当期不再触发同类型新事件，
   避免撤销握手在下一周期才生效时重复消费输入。
2. **状态推进**：对每个活跃事件，用 `while(state_changed)` 循环检查
   `check_exec/check_finish/check_end`，允许单周期内连续状态转换。
3. **清理**：`cleanup_ended_events()` 将 ENDED 事件移入 `completed_events_`
   供性能统计。

### 4.3 时间与事件调度

- `ProcessEvent` 四个时间戳均为绝对周期，必须满足
  `trigger <= exec <= finish <= end`。执行时间表示输入被接收，完成时间表示计算完成，
  结束时间表示输出交接完成。具体握手由模块回调实现。
- `submit_signal_value(name, value, delay_cycles)` 与 `submit_message` 接收相对延迟；
  `schedule_signal_update(SignalUpdateEvent)` 接收绝对周期。溢出或过去事件明确报错。
- `set_signal_value` 只用于提交已经到期的值，例如构造时在周期0设置初值。
  未来值必须排队，不能覆盖当前值；未知信号和类型错误均抛异常。
- 每周期先同步模块时钟，再交付到期消息、交付到期信号、按深度降序求值模块。
  同类、同周期的队列事件按提交顺序交付。同深度模块按注册顺序，模块内进程按注册顺序。
  消息阶段在信号阶段之前是显式契约，不宣称消息与信号之间按同一个全局序号交错执行。
- 求值产生的零延迟事件在当前周期后续 delta 步交付，并再次求值模块，直到稳定。
  进程内部状态推进也会触发后续delta，保证没有外发事件的内部依赖不会遗漏。
  `set_max_delta_steps` 默认1024；不收敛的零延迟网络报错。模型回调应能安全重复检查条件。
  同周期对同一信号的多次写入按最后一次取值；信号是状态变量，不是无损消息队列。
- `send_message_to_core` 是立即调用，只接受零延迟；需要延迟请使用
  `schedule_message_to_core`。后者根据当前周期加消息延迟排队，投递时检查目标模块处理器。
- `ProcessType::latency` 目前是模型元数据；状态完成仍由 `finish_cond` 判定，
  引擎没有因这个参数自动完成事件。固定延迟必须通过延迟信号/消息和完成条件建模。

### 4.4 仿真拓扑序与反馈

`register_module` 使用调用方指定的拓扑深度，按深度降序稳定排序。模型提供模块
连接关系与深度，框架通过零延迟 delta 迭代处理同周期交付和级联求值。未来事件
队列保证反馈值在到期前不可见。端口缓冲深度、数据消费和反压协议由架构模块实现。

### 4.4.1 停止原因与完成判据

调用方应提供 `set_completion_condition`，例如全部任务和通信的期望计数均满足。
只有队列耗尽、所有进程均结束且完成判据为真，`is_simulation_done()` 才为真。

| `StopReason` | 含义 |
|---|---|
| `NotStarted` | 尚无停止结果 |
| `Completed` | 活动进程/事件均耗尽且明确的工作负载完成判据满足 |
| `Quiescent` | 无活动进程/事件，但没有完成证明；可能是未初始化或缺少激励 |
| `MaxCycles` | 用完周期预算，仍未满足上述终止条件 |
| `Error` | 配置、消息、时序或模型回调异常，异常继续向调用方传播 |

`Quiescent` 不自动称为死锁，因为框架不了解自定义模型还等待哪些外部数据；
`MaxCycles` 也不能证明死锁。无进展的活跃进程目前由周期上限截断，尚无通用等待图诊断。
实例只能 `run()` 一次；复现实验必须重新构造实例，避免复用模块或计数器残留。

### 4.5 信号系统（结构类型化 + 运行时注册表）

信号系统**只约定结构，不预设具体信号**：

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

### 4.6 消息系统（通用模板 + 自动注册）

消息系统**只约定消息结构，不预设消息类型**：

```cpp
struct GenericMessage {
    std::string task_id;        // 用户自定义任务标识
    uint64_t delay_cycles{0};   // 延迟周期数
    std::any body;              // 用户自定义消息体类型（std::any 支持任意类型）
};
```

**类型化注册**：用户通过 `register_message_handler<T>`（模块级）或
`register_task_handler<T>`（模拟器级）注册，handler 直接接收 `const T&`，
框架自动登记 `type_index(T)` 并在发送/分发时校验：

```cpp
// 模块级（TaskScheduler 示例）
register_message_handler<std::tuple<int, int>>("init_task",
    [this](const std::tuple<int, int>& data) { ... });

// 模拟器级（类型化任务处理器）
register_task_handler<std::tuple<int, std::string>>("task_batch_done",
    [this](const std::tuple<int, std::string>& data) { ... });
```

**通用版保留**：`void(const GenericMessage&)` handler 仍支持（向后兼容，
不登记类型、不校验）。

**两条消息路径**：

| | 路径 A（进队列） | 路径 B（直发） |
|---|---|---|
| 方向 | 模块 → 模拟器 | 模拟器 → 模块 |
| 入口 | `submit_message` | `send_message_to_core` |
| 数据流 | `pending_events_` → `SimulatorEvent::MESSAGE_SEND` → `message_queue_` → `process_message_events` → `task_handlers_` | `module_map_.find` → `handle_message` → `message_handlers_` |
| 延迟 | 支持 `delay_cycles` | 立即 |
| 典型消息 | `task_batch_done`（任务完成汇报） | `init_task`（任务队列初始化指令） |

### 4.7 模块执行链路（`ModuleBase`）

模块基类职责：
- **信号管理**：`signals_` 映射 + `add_signal/get_signal/get_signal_as<T>/set_signal_value/clear_signal/invalidate_signal`
- **事件缓冲**：`pending_events_` + `submit_signal_value/submit_message` 累积，`evaluate()` 返回并清空
- **进程管理**：`process_manager_`（unique_ptr）+ `register_process/bind_signal_to_process/get_processes_by_signal`
- **消息处理**：`message_handlers_` + `message_types_`（类型登记）+ `register_message_handler/handle_message`
- **信号注册表查询**：`get_signal_declarations/get_signal_value_type/get_signal_direction`
- **消息类型查询**：`get_message_type(task_id)`

### 4.8 多核事务机制

多核模拟通过三层抽象扩展单核事件驱动：

| 抽象 | 含义 | 实现 |
|------|------|------|
| **事务构造** | 计算图节点抽象为事务 | 当前示例在同一引擎中注册多个核心的模块 |
| **事务分配** | 硬件映射 | 核心/模块注册（`register_module`） |
| **事务间消息传递** | 多核调度 | `GenericMessage` + `task_handlers_` 消息分发 |

#### 任务依赖原语（`TaskDependency.h`）

框架约定表项结构，生产-消费屏障由用户建模时自定义（与信号/消息机制同原则）：

```cpp
class TaskDependencyEntry {   // 一个生产-消费屏障
    using ProducerHook   = std::function<void(Entry&, const GenericMessage&)>;
    using ConditionCheck = std::function<bool(const Entry&)>;
    using ConsumerAction = std::function<void()>;
    // ProducerHook: 任务完成消息 → 更新表项状态（计数器）
    // ConditionCheck: 状态是否满足触发条件
    // ConsumerAction: 条件满足时执行的消费者事务（触发下游 init_task 等）
};

class TaskDependencyTable {  // 活跃任务表
    void register_entry(TaskDependencyEntryPtr);  // name 重复时抛异常
    int  on_task_done(const GenericMessage&);     // 广播到所有活跃表项
    TaskDependencyEntryPtr get_entry(const std::string& name);  // 按 name 查询
};
```

**触发语义**：某个生产者的任务完成消息到达时，`on_task_done` 广播到所有活跃
表项，各表项自行判断是否相关（ProducerHook 匹配生产者）并更新状态；满足条件
（ConditionCheck）则执行消费者事务（ConsumerAction）。原始三回调构造器在触发时
清零计数器，一条消息至多触发一轮；递归进入同一个表项会明确报错。

**通用性**：
- **不同生产者可有不同计数阈值**：ConditionCheck 是用户自定义函数，可对每个
  生产者按各自阈值判断（如 ts0.b0≥4 且 ts1.b0≥2 才触发）
- **单生产者可触发多个消费者**：一个生产者消息广播到所有表项，各表项独立
  判断、独立消费计数，互不影响
- **任意消息类型/增量语义**：消息 body 为 `std::any`（用户自定义类型）；
  计数累加量由用户定义（可为 +1，也可为消息携带的 incr 增量）

**便捷注册接口**（`make_increment_dependency`）：覆盖最常见的
"计数累加 + 阈值触发" 模式，用户只需提供三个要素：

```cpp
auto entry = make_increment_dependency(
    "E1",
    // ① 增量提取：从任意消息提取 {计数键, 增量}
    [](const GenericMessage& msg) -> std::pair<std::string, int> {
        auto data = std::any_cast<std::tuple<int, std::string>>(msg.body);
        return {std::get<1>(data) + ":" + std::to_string(std::get<0>(data)), 1};
    },
    // ② 阈值表：每个计数键独立阈值（key → threshold）
    {{"task_scheduler0:0", 4}, {"task_scheduler1:0", 2}},
    // ③ 消费者事务
    [&fired]() { fired++; });
table.register_entry(entry);
```

`make_increment_dependency` 按各键阈值消费计数，保留超额计数；一次较大增量可触发
多轮。例：阈值2、累计7会触发3次，剩余1。空阈值、非正阈值、负增量及计数溢出
会报错；未列出的键不参与此屏障。超额数据保留供后续触发使用。
框架不推断重复消息身份，重试/去重需模型提供任务ID和序号语义。
`on_task_done` 的返回值是触发过的表项数，不是消费者调用总次数。

**模拟器集成**（`CycleAccurateSimulator`）：
- `register_task_dependency(entry)`：注册表项
- `on_task_done(msg)`：驱动所有相关表项状态更新与触发判断

**多核示例**（`MulticoreSimulator`）：6 核数据依赖使用
4 个数据驱动表项（E1-E4），`handle_batch_task_done` 简化为 `on_task_done` 广播。
批次切换后清除对应屏障条目的触发状态。

---

## 5. 关键功能与接口

### 5.1 核心公共接口

**`ISimulator`（模拟器抽象接口）**

```cpp
class ISimulator {
    // 非模板虚方法
    virtual void connect_modules(const std::string& src_id, const std::string& src_signal,
                                 const std::string& dst_id, const std::string& dst_signal) = 0;
    virtual void run() = 0;
    virtual uint64_t get_current_cycle() const = 0;
    virtual bool is_simulation_done() const = 0;
    virtual const std::vector<std::shared_ptr<ISimulatable>>& get_all_modules() const = 0;
    virtual void dump_completed_events(const std::string& filename) const = 0;
    virtual void send_message_to_core(const std::string& core_id, const GenericMessage& msg) = 0;
    virtual void schedule_message_to_core(const std::string& core_id, const GenericMessage& msg);
    virtual void schedule_signal_update(const SignalUpdateEvent& event) = 0;
    // 模板方法（类型安全，委托虚钩子）
    template<typename ModuleType, typename... Args>
    std::shared_ptr<ModuleType> register_module(const std::string& id, int depth, Args... args);
    template<typename ModuleType>
    std::shared_ptr<ModuleType> get_module(const std::string& id);
    template<typename T>
    void register_task_handler(const std::string& task_id, std::function<void(const T&)> handler);
    // 虚钩子（protected，实现层实现）
    virtual void register_module_impl(...) = 0;
    virtual void register_task_handler_impl(...) = 0;
    // 引擎钩子（protected virtual，测试可覆盖）
    virtual void simulate_cycle() = 0;
    virtual void process_message_events(uint64_t) = 0;
    virtual void process_signal_events(uint64_t) = 0;
};
```

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

### 5.2 信号注册表

```cpp
struct SignalMeta {
    Signal::Direction direction;
    std::type_index value_type;
};
std::unordered_map<std::string, std::unordered_map<std::string, SignalMeta>> signal_registry_;
```

`connect_modules` 的验证（缺失即抛 `std::runtime_error`）：
1. **存在性**：源/目标信号必须在注册表中
2. **方向**：源必须是 OUTPUT，目标必须是 INPUT
3. **类型**：源信号值类型与目标声明类型一致
4. **单驱动**：目标输入只能有一个驱动者；多生产者必须使用显式仲裁模块

### 5.3 性能统计

每个模块维护 `performance_stats_`，`get_performance_stats()` 合并 `get_process_stats()`：
- `total_evaluations`：模块被 evaluate 的次数，包含同周期 delta 求值，可能大于周期数
- `signal_updates`：提交的信号更新事件数
- `ProcessManager::get_performance_stats()`：
  - `total_completed_events` / `total_latency` / `avg_latency`
  - `busy_time` / `min_latency` / `max_latency`

模拟器级 `total_cycles` 是实际求值的周期数量，包含周期0；`get_current_cycle()`
在运行回调中是当前绝对周期，run返回后是下一周期。`total_events` 同时统计已交付的
消息和信号。`busy_time` 是已完成事件的执行时长之和，重叠事件会
重复累加，不是硬件资源时间区间的并集，也不是资源利用率；`avg_latency` 使用整数除法。
吞吐量需要以已完成任务数除以明确观测窗口，由调用方计算。

---

## 6. 建模工作流

### 6.1 新增一个模块类型

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

### 6.2 在模拟器中实例化并连接

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

### 6.3 注册类型化消息处理器

```cpp
// 模块级（在 register_message_handlers() 中）
register_message_handler<std::tuple<int, int>>("init_task",
    [this](const std::tuple<int, int>& data) { ... });

// 模拟器级（架构封装 Init() 中）
register_task_handler<std::tuple<int, std::string>>("task_batch_done",
    [this](const std::tuple<int, std::string>& data) { ... });
```

---

## 7. 配置驱动模块图（`SimConfigLoader`）

除了在 `Init()` 中用 C++ 代码硬编码构建模块图，还可用 `include/simulator/ConfigLoader.h`
的 `SimConfigLoader` 从 JSON 配置自动构建模块图。二者并存、互不干扰。

### 7.1 JSON Schema

```json
{
  "modules": [
    { "id": "simd", "type": "SIMD", "depth": 4 },
    { "id": "crossbar", "type": "Crossbar", "depth": 3 },
    { "id": "task_scheduler", "type": "TaskScheduler", "depth": 2,
      "params": { "tasks": [
        { "block_num": 1, "row": 6, "col": 12, "channel_num": 128, "pooling": true },
        { "block_num": 0, "row": 0, "col": 0, "channel_num": 0, "pooling": false }
      ] } },
    { "id": "L1_cache", "type": "L1C", "depth": 1 }
  ],
  "connections": [
    { "src": "task_scheduler", "src_signal": "cache_read_trigger",
      "dst": "L1_cache", "dst_signal": "cache_read_trigger" }
  ]
}
```

- `modules[]`：每个元素 `{ id, type, depth, params? }`。
  `type` 是模块类型字符串；`params` 为可选的构造参数对象（见 7.3 例化策略）。
- `connections[]`：每个元素 `{ src, src_signal, dst, dst_signal }`，可选。
  连接经 `connect_modules` 复用信号注册表做存在性、方向、类型和单驱动验证。
  框架字段在调用工厂前预检；用户工厂或连接声明校验失败后，不支持事务回滚，应丢弃实例。

### 7.2 使用方式

```cpp
#include "simulator/ConfigLoader.h"
#include "simulator/tile2_0/Crossbar.h"
#include "simulator/tile2_0/SIMD.h"
#include "simulator/tile2_0/L1C.h"
#include "simulator/tile2_0/TaskScheduler.h"

class ConfigSimulator : public CycleAccurateSimulator {
public:
    ConfigSimulator(const std::string& json_cfg, uint64_t max_cycles = 200000)
        : CycleAccurateSimulator(max_cycles) {
        // 1. 注册模块类型工厂（面向 ISimulator 接口编程）
        loader_.register_factory("SIMD", [](ISimulator& sim, const std::string& id, int depth, const json::object&) {
            sim.template register_module<SIMD>(id, depth);
        });
        // ... 为每个模块类型注册工厂
        // 2. 从 JSON 加载模块图
        loader_.load(*this, json_cfg);
    }
private:
    SimConfigLoader loader_;
};
```

### 7.3 JSON 模板类的例化策略（工厂注册表）

不同模块类构造参数不同（如 `SIMD`/`Crossbar`/`L1C` 只需 `id`，`TaskScheduler` 还需
`std::array<FmapTask, L1C_BANK>`）。`SimConfigLoader` 无法在编译期知道 JSON 的
`type` 字符串对应哪个具体类，因此采用**工厂注册表 + 模板 `register_module`** 的组合：

1. **工厂注册表**：`register_factory(type_name, factory_lambda)` 将类型字符串映射到
   一个构造工厂。工厂签名统一为
   `void(ISimulator&, const std::string& id, int depth, const boost::json::object& params)`。

2. **模板 `register_module`**：工厂内部调用 `sim.template register_module<ConcreteType>(id, depth, args...)`。
   因为调用处是用户代码，`ConcreteType` 在编译期已知，故：
   - 保持类型安全（不需要 `dynamic_cast` 或类型擦除）
   - 自动触发 `register_processes()` / `register_message_handlers()`
   - 自动把信号声明收集进信号注册表

3. **参数差异处理**：需要额外构造参数的模块，在工厂 lambda 内解析 `params` JSON，
   构造具体类型参数后传给 `register_module`。例如 TaskScheduler 的工厂解析 `tasks` 数组：

```cpp
loader_.register_factory("TaskScheduler", [](ISimulator& sim, const std::string& id, int depth, const json::object& params) {
    std::array<FmapTask, L1C_BANK> tasks{};
    if (params.contains("tasks")) {
        const auto& arr = params.at("tasks").as_array();
        for (size_t i = 0; i < arr.size() && i < L1C_BANK; ++i) {
            const auto& t = arr[i].as_object();
            tasks[i] = FmapTask{
                static_cast<int>(t.at("block_num").as_int64()),
                static_cast<int>(t.at("row").as_int64()),
                static_cast<int>(t.at("col").as_int64()),
                static_cast<int>(t.at("channel_num").as_int64()),
                t.at("pooling").as_bool()
            };
        }
    }
    sim.template register_module<TaskScheduler>(id, depth, tasks);
});
```

**错误处理**：
- 未知模块类型（工厂未注册）→ 抛 `std::runtime_error`（"unknown module type ..."）
- JSON 缺少 `modules` 数组 → 抛 `std::runtime_error`
- 连接验证失败（信号不存在/方向错/类型不匹配）→ 由 `connect_modules` 抛出

**依赖**：`src/cpp/CMakeLists.txt` 需链接 `Boost::json`（使用 CMake 的 `Boost::json` 组件）。

---

## 8. 验证与覆盖

测试文件由 CMake 枚举，使用 `ctest --test-dir <build-dir> -N` 查看当前清单。
`-N` 仅枚举，不执行。日常执行使用 `ctest --test-dir <build-dir> --output-on-failure`。

| 测试 | 验收重点 |
|---|---|
| `simulator_contract_test.cpp` | 零延迟链、稳定事件顺序、未来反馈隔离、停止原因、周期/事件数、延迟核心消息、异常、token守恒 |
| `simulator_config_contract_test.cpp` | 重复模块、未知类型、超范围深度、非法连接预检，无结构性部分注册 |
| `process_test.cpp`、`modulebase_test.cpp` | 状态时间戳、类型、生命周期和消息接口 |
| `connect_test.cpp`、`isimulator_test.cpp` | 连接验证与引擎插入点 |
| `taskdependency_test.cpp` | 不同生产者阈值、广播、重置及多消费者 |
| `configloader_test.cpp`、`oputiletest.cpp`、`bankingtest.cpp`、`multicoretest.cpp`、`tilingtest.cpp` | 架构示例；应以任务守恒、完成判据和停止原因为准 |

普通构建与 ASan/UBSan 构建使用同一组时序、任务完成和资源守恒断言。
`scripts/regression/verify_simulator_traces.py` 比较重复运行和不同构建的事件轨迹；
具体命令见[使用指南](../guides/usage.md)。泄漏检测器需要支持其进程检查方式的运行环境。
硬件时序与功耗精度需要另外的 RTL 或芯片测量校准。

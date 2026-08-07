# PIM-sim 模拟器架构说明

> 对应论文《存算一体芯片协同工具链关键技术研究》第 5 章
> （异构多核存算芯片性能模拟框架研究）
>
> 本目录（`src/simulator/`）与 `include/simulator/` 共同构成 PIM-sim 的
> 周期精确（cycle-accurate）存算架构模拟后端。

---

## 1. 总体定位

模拟器是 PIM-sim 工具链的**性能模拟后端**，接收算子映射前端（`CGraph`→`TGraph`→`HGraph`→`DGraph`）生成的
推理任务与硬件映射结果，通过事件驱动的周期精确仿真，输出延迟、吞吐量、功耗等性能指标。

其核心设计目标（与论文 5.2 一致）：

1. **与微架构细节解耦的性能建模**：用户以"模块 + 信号 + 事件"描述微架构行为，无需关心底层信号传播实现。
2. **周期精确的事件驱动机制**：自动维护事件队列的执行顺序与触发时序。
3. **统一建模范式**：为多核芯片中的异构功能组件（数字电路、易失存储、非易失存算器件）提供统一的数据接口。

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

| 文件 | 职责 | 对应论文 |
|------|------|----------|
| `ISimulatable.h` | 模块抽象接口 + `Signal` 信号结构 | 5.2.1 模块原语（信号属性） |
| `ModuleBase.h` | 模块基类（信号管理、事件缓冲、消息处理） | 5.2.1 模块原语 |
| `Process.h` / `Proecss.cpp` | `ProcessEvent`（事件实例）、`ProcessType`（进程类型）、`ProcessManager`（状态机） | 5.2.1 事件原语 + 5.2.2 状态更新模型 |
| `Simulator.h` / `Simulator.cpp` | `CycleAccurateSimulator`（事件驱动引擎、信号注册表、连接管理） | 5.2.2 事件驱动机制 |
| `SimulatorEvent.h` | 统一事件类型（信号更新/消息发送），`evaluate()` 返回值 | 5.2.2 |
| `MessageBase.h` | `GenericMessage` 通用消息（task_id + body） | 5.3.1 消息原语 |

### 2.2 Tile2.0 架构模型层（`include/simulator/tile2_0/`, `src/simulator/tile2_0/`）

| 文件 | 职责 | 对应 OPU-Tile2.0 组件 |
|------|------|----------------------|
| `config.h` | `hw_config` 命名空间硬件参数（constexpr） | 5.4.1 表5-6 参数 |
| `Crossbar.h/.cpp` | 存算阵列：计算 + 切换 | 存算阵列（WL/BL=1152/256, 40 周期） |
| `SIMD.h/.cpp` | 后处理：量化→激活→池化流水 | 后处理单元（16 宽, 2/3 周期） |
| `L1C.h/.cpp` | 本地缓存（多 bank SRAM） | 本地缓存（4KB×3, 128-bit, 1 周期） |
| `TaskScheduler.h/.cpp` | 任务调度：数据搬运 + 计算触发 | 任务控制器（静态调度, 1 周期） |
| `core_factory.h` | `create_core_modules()` 核心模块工厂 | 单核模块组标准化实例化 |
| `membanking.h/.cpp` | 内存 banking 模拟（XY/YX/Custom 策略） | 多核分块 |
| `multicore.h/.cpp` | 多核并行模拟（6 核，硬编码依赖） | 多核事务 |
| `tiling.h/.cpp` | 动态 banking tiling 模拟 | 分块策略 |
| `tile2_0.h/.cpp` | OPU 单 tile 全流水线模拟 | 单核推理 |

---

## 3. 模拟机制

### 3.1 三层数据流模型（论文 5.1.1）

仿真器将计算数据流抽象为三层：

| 层级 | 定义 | 粒度 |
|------|------|------|
| **全局数据流** | 系统级输入激励，预加载至全局缓存 | 完整计算图 |
| **分块数据流** | 存算核心间算子的输入-输出关系 | 特征图分块 |
| **MVM 数据流** | 存算阵列内部的乘累加批次 | 分块内的批次/激励点 |

### 3.2 微架构图与拓扑属性（论文 5.1.1）

- **微架构行为集合**：`Behav_j = { behav_i^j | μop_i → μarch_j }`，即所有计算操作在节点 j 上的行为集合。
- **微架构节点依赖**：若 `behav_i^j` 的输出被 `behav_i^k` 用作输入，则存在依赖 `μarch_j → μarch_k`。
- **拓扑深度**：`Depth(μarch_j)` 为起点到该节点的最长路径长度（环检测时忽略回边）。
- **依赖方向**：与数据流参考方向一致为**前向依赖**（生产-消费），反之为**反馈依赖**（控制流反馈）。

代码对应：`ModuleBase::topological_depth_`，由 `register_module` 设置。

### 3.3 事件驱动机制（论文 5.2.2）

#### 事件状态机（对应 `ProcessEvent`）

`ProcessEvent` 定义五阶段状态：

```
IDLE ──触发──▶ TRIGGERED ──执行──▶ EXECUTING ──完成──▶ FINISHED ──结束──▶ ENDED
  (空闲)        (已触发)            (执行中)            (执行完成)        (事件结束)
```

对应论文 5.2.1 事件原语的五类时间戳：
- **触发时间戳** `T_trigger`：上级模块已产生所有有效输出
- **执行时间戳** `T_exec`：本模块开始接收数据并计算，状态更新为"占用"
- **完成时间戳** `T_finish`：计算结束，输出置为有效
- **结束时间戳** `T_end`：模块状态恢复可用，输出-输入信号组无效化
- **性能计数时间戳**：事件占用硬件资源的周期数

代码实现：`Process.h` 的 `ProcessEvent`（状态 + 4 个时间戳字段），
`get_timing_stats()` 返回各类延迟（trigger→exec、exec→finish、finish→end、total）。

#### 状态转移驱动（`ProcessManager::drive_state_transitions`）

每个模块在 `evaluate(current_cycle)` 时调用，流程：

1. **触发检查**：遍历 `process_types_`，若无活跃同类事件且触发条件满足，`create_active_event` 创建新事件。
2. **状态推进**：对每个活跃事件，用 `while(state_changed)` 循环检查 `check_exec/check_finish/check_end`，允许单周期内连续状态转换（对应真实硬件中"一旦条件满足立即执行"）。
3. **清理**：`cleanup_ended_events()` 将 ENDED 事件移入 `completed_events_` 供性能统计。

#### 事件调度（`CycleAccurateSimulator`）

- 每个模块的 `evaluate()` 返回 `std::vector<SimulatorEvent>`（解耦后设计）。
- 模块内部通过 `submit_signal_value()` / `submit_message()` 将事件累积到 `pending_events_`。
- 模拟器在 `simulate_cycle()` 中收集各模块事件，`dispatch_simulator_event()` 将**相对延迟周期** + `current_cycle_` 转为**绝对生效周期**，推入 `signal_event_queue_` / `message_queue_`（最小堆优先队列）。

```
simulate_cycle():
  1. process_message_events()      # 处理到期消息
  2. process_signal_events()       # 处理到期信号
  3. process_combinational_logic() # 组合逻辑模块
  4. 对每个 module: evaluate() → dispatch_simulator_event()
  5. check_simulation_complete()
```

#### 仿真拓扑序（论文 5.2.2 并发事件）

事件按**拓扑深度降序**（逆流水线方向）执行，保证"在 T 时刻所有输入数据已就绪，且当前输出更新不影响同一时刻未执行事件的输入"。`register_module` 按拓扑深度排序 `modules_`。

两类违例依赖及消除方法（论文 5.2.2）：
- **前向组合逻辑依赖**（延迟为 0 的传播，如加法器）：当前时刻后级依赖前级未执行事件的输出 → 框架自动检测并插入高优先级事件栈优先执行。
- **反馈时序逻辑依赖**（后级在 >T 时刻修改前级信号）：通过数据队列化（入队）防止当前有效数据被覆盖。队列长度 ≤ 2（正确反压条件下）。

### 3.4 信号系统（结构类型化 + 运行时注册表）

信号系统**只约定结构，不预设具体信号**：

```cpp
struct Signal {
    std::string name;          // 用户建模时自定义
    Direction direction;       // INPUT / OUTPUT / INTERNAL
    std::any value;            // 信号值
    std::type_index value_type; // 值类型（std::any 取内部实际类型）
};
```

**用户建模层**：在模块构造函数中 `add_signal(Signal("my_sig", Direction::INPUT, 0))` 声明自己的信号
（名称、数量、方向、类型完全由用户决定）。

**模拟器统一管理**（`CycleAccurateSimulator`）：
- `register_module()` 自动收集模块信号声明到 `signal_registry_`（模块ID → 信号名 → {方向, 值类型}）。
- `connect_modules()` 自动验证：信号存在性、方向（源 OUTPUT → 目标 INPUT）、值类型匹配。
- `submit_signal_value()` 校验赋值类型与声明类型一致。

**拷贝消除策略**：论文 5.2.2 提到接口原语采用拷贝消除——用单一变量表示绑定的输入/输出信号组有效值，
正确性依赖事件执行顺序（拓扑序）合理安排。

### 3.5 模块执行链路（`ModuleBase`）

模块基类职责：
- **信号管理**：`signals_` 映射 + `add_signal/get_signal/get_signal_as<T>/set_signal_value/clear_signal/invalidate_signal`
- **事件缓冲**：`pending_events_` + `submit_signal_value/submit_message` 累积，`evaluate()` 返回并清空
- **进程管理**：`process_manager_`（unique_ptr）+ `register_process/bind_signal_to_process/get_processes_by_signal`
- **消息处理**：`message_handlers_` + `register_message_handler/handle_message`
- **信号注册表查询**：`get_signal_declarations/get_signal_value_type/get_signal_direction`

### 3.6 多核事务机制（论文 5.3.1）

多核模拟通过三层抽象扩展单核事件驱动：

| 抽象 | 含义 | 实现 |
|------|------|------|
| **事务构造** | 计算图节点抽象为事务 | 每个核心一个 `CycleAccurateSimulator` 实例 |
| **事务分配** | 硬件映射 | 核心/模块注册（`register_module`） |
| **事务间消息传递** | 多核调度 | `GenericMessage` + `task_handlers_` 消息分发 |

**任务依赖原语**（论文 5.3.1 表5-4）：维护活跃计算任务表 `core_batch_num_map_`
（记录每个核心/ bank 的批次数），监视多核数据传输触发条件。

**消息原语**（论文 5.3.1 表5-5）：`GenericMessage` 封装核间通信事件参数
（`task_id` + `body` + `delay_cycles`），驱动顶层任务状态更新。

当前多核实现（`multicore.cpp`）以 6 核硬编码依赖关系（`handle_batch_task_done` 的 switch 逻辑）
模拟 OPU-Tile2.0 的数据依赖反压机制与计算批次模式。

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

> 注意：ASan 构建下模拟器测试会运行到 max_cycles 而非提前终止（TaskScheduler 进程在 ASan 下不完成），
> 为预存现象。时序正确性用普通构建验证，ASan 仅用于内存/泄漏检测。

---

## 7. 已知限制与后续工作

1. **反压机制建模**（论文 5.2.2 数据队列、5.3.1 反压）：`SIMD_compute_ready` 信号已声明但未接线；
   多核反压依赖目前以 `core_batch_num_map_` 软件计数模拟，非硬件 ready/valid 握手。
2. **事件计数器自动化**（算法 5.1）：当前事件由模块条件函数动态触发，未实现从任务参数静态生成事件集合。
3. **组合逻辑依赖检测**（论文 5.2.2）：框架预留了机制但未自动化。
4. **多核事务机制泛化**：`MulticoreSimulator` 为硬编码 6 核依赖，未实现通用的任务依赖原语/消息原语。
5. **配置驱动模块图**：拓扑仍硬编码在 C++ `Init()` 中，未从 JSON/YAML 加载。
6. **ISimulator 抽象接口**：`CycleAccurateSimulator` 为具体类，未提取虚接口（影响 mock 测试）。

对应 TASKS.md "模拟器架构优化" 章节待办项。

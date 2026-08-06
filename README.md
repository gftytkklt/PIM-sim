# PIM-sim（PIMapping）

> **PIMapping: A Tile-Level Dataflow Optimization Framework for PIM-Architecture**  
> Ziqian Zhu, Yifei Zhou, Jinsen Zhu, Yuxuan Wang, Hongbing Pan — *IEEE TCAD 2025*

面向存内计算（PIM）架构的 **tile 级数据流优化与性能评估框架**。由**算子映射前端**（C++ 图分析引擎）和**性能模拟后端**（cycle-accurate 仿真器）两大部分组成，构成从 ONNX 模型输入到硬件级延迟、吞吐量、功耗指标的**全流程仿真工具链**。CMake 项目名：`PIMapping`。

## 架构总览

```
ONNX 模型 ──(onnx_analysis.py)──▶ NNkernel 数组
                                   │
                       pybind11 ──────────┘
                                   ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │  算子映射前端 (libPIMapping.a)                                   │
  │                                                                 │
  │  Analyzer ──▶ CGraph ──▶ TGraph ──▶ HGraph ──▶ DGraph           │
  │  (crossbar级)  (tile级)    (硬件映射)   (动态调度)              │
  │                                                                 │
  │  Mapper（物理映射）  Scheduler（拥塞感知路由）                  │
  │  strategy/（PIMAPPING / SPATEM / HITM / MNSIM / TILE2_0）       │
  └─────────────────────────────────────────────────────────────────┘
                                   │
                       pybind11 ──────────┘
                                   ▼
  ┌─────────────────────────────────────────────────────────────────┐
  │  性能模拟后端 (Python + C++ Simulator)                           │
  │                                                                 │
  │  MappingInfo.py ──▶ Booksim（NoC 仿真） + MNSIM（硬件建模）     │
  │  Simulator/ ──▶ Cycle-accurate 事件驱动仿真（Tile2.0 架构）     │
  │  perf.py ──▶ 延迟/吞吐量/功耗分析 + 可视化                      │
  └─────────────────────────────────────────────────────────────────┘
```

## 方法论

PIMapping 的工作流分为三个阶段：**递进式部署表示生成** → **数据流图构建与优化** → **性能评估**。

### Tile 级数据流表示

框架定义了四个层次的图表示，从细粒度到粗粒度逐步推导：

| 表示 | 图类 | 定义 | 关键属性 |
|------|------|------|----------|
| **C-VDFG** | `CGraph` | Crossbar 级虚拟数据流图。将 MVM 算子按 Crossbar 尺寸沿并行通道（BL）和部分和（WL）维度拆分，每个算子分配 ⌈H/WL⌉×⌈W/BL⌉ 个 Crossbar | 节点：layer, ifm/ofm, cin/cout。边类型：Psum（层内累加）、Prop（层间传播）。数据量按通道交集比例计算 |
| **T-VDFG** | `TGraph` | Tile 级虚拟数据流图。在 Tile 内 Crossbar 数量约束下，将 C-Node 合并为 T-Node | 合并原则：数据依赖驱动（高复用率节点优先合并）+ 最小化片间通信。T-Edge 数据量为融合 C-Edge 的叠加 |
| **HCG** | `HGraph` | 硬件连接图。将 T-Node 映射到物理 2D Tile 阵列，数据依赖通过 NoC 拓扑实现，引入中继节点 | 节点：物理 Tile 位置 + 映射的虚拟 Tile。边：物理路径 + 数据量。中继开销累积 |
| **DHCG** | `DGraph` | 动态硬件连接图。按流水线阶段将 HCG 划分为多个子图，每个子图对应一个并行计算阶段 | 划分依据：算子拓扑排序 → 线性化 → 分段（LS/LP 均为特例）。DHCG 为拥塞分析的基本单元 |

所有图类基于 Boost Graph Library（BGL）构建，支持有向图、无向图、双向图操作。

### 基于数据邻近性的物理映射（Algorithm 1）

目标：最小化片间通信代价 ΣCommData(i,j) × Distance(i,j)。

**步骤 1 — Crossbar 级节点分解：** 算子按 Crossbar 尺寸拆分，生成 C-VDFG。

**步骤 2 — Tile 级节点聚类：** 定义连接强度度量 `Intensity Map`，其优化目标为 α×Γ_Intra − β×Γ_Inter（Γ_Intra 量化片内数据复用收益，Γ_Inter 捕获片间通信开销）。算法流程：
1. 初始化 Intensity Map，构建最大堆
2. 弹出堆顶节点作为聚类种子，遍历邻居，选择强度最大的节点加入聚类
3. 动态更新邻居归属和聚类内强度度量
4. 持续至 Tile 内 Crossbar 填满，生成 T-Node

**步骤 3 — 物理节点放置：** 将坐标映射问题转化为离散 2D 谱嵌入问题。构建图拉普拉斯矩阵 L = D − A，求解近优节点嵌入，离散化后得到最终 HCG 物理映射。

### 拥塞感知的路径调度（Algorithm 2）

目标：最小化各 D-Edge 的拥塞度量之和。

**拥塞度量定义：**
- **BCE 度量**（边介数中心性）：识别关键通信路径，结合数据依赖量化各边的重要性：
  ```
  BCE(e_D, DG) = Σ η(ns,nt|e_D) / η(ns,nt) × e_T(ns,nt)
  ```
- **Tile 级拥塞度量 C-Σ**：量化通信资源竞争导致的影响数据量。假设路径按数据量升序排列 P₁,...,Pₙ，则 C(e) = Σᵢ₌₁ⁿ⁻² Dᵢ + 2Dₙ₋₁

**调度算法：**
1. 按流水线分段生成 DHCG，初始化 BCE
2. 路径按曼哈顿距离升序、数据量降序排列
3. 对每条路径，使用加权 Dijkstra 寻找最短路径，权重 w(e) = C'(e) × BCE(e)
4. 更新拥塞图，继续下一条路径

该权重设计平衡了两个目标：降低拥塞 + 避免过度使用热点路径。

### 表达力

框架可表达多种计算优化技术：
- **权重复制：** 建模为多个独立并行 CNode 流，ifmap/ofmap 按复制因子 N 缩减为 1/N，通过特征图依赖路由输出组到输入组
- **零值跳过：** 通过量化计算减少比例，自动调整 C-Edge 数据量
- **OU 级计算：** 将 CNode 尺寸配置为 OU 维度，同时缩放每 Crossbar 的 OU 数量

## 工程结构

```
.
├── include/                      # C++ 头文件
│   ├── graph.h                   # 数据流图类层次（CGraph / TGraph / HGraph / DGraph）
│   ├── analyzer.h                # 顶层 Analyzer（编排完整分析流水线）
│   ├── mapper.h                  # 物理 tile 映射（shared_ptr 注入，支持插件化替换）
│   ├── scheduler.h               # 拥塞感知路径调度（shared_ptr 注入，BCE + Dijkstra）
│   ├── util.h                    # 辅助函数与通用类型（Path, OptType 等）
│   ├── logger.h                  # C++ 结构化日志（PIM_INFO/PIM_WARN/PIM_ERROR）
│   ├── errors.h                  # 统一异常层次（PIMException/GraphError 等）
│   ├── strategy/                 # 策略模式（按图层次组织）
│   │   └── StrategyBase.h        # 基类 + 工厂函数 + 类型别名
│   └── simulator/                # Cycle-accurate 仿真器
│       ├── ISimulatable.h        # 模块接口
│       ├── Process.h             # 进程状态机（IDLE → TRIGGERED → EXECUTING → FINISHED）
│       ├── ModuleBase.h          # CRTP 模块基类（信号/消息/进程管理）
│       ├── Simulator.h           # 事件驱动仿真引擎
│       └── tile2_0/              # Tile 2.0 架构模型
│           ├── Crossbar.h        # 交叉开关矩阵（计算 + 切换）
│           ├── SIMD.h            # SIMD 流水线（量化→激活→池化）
│           ├── L1C.h             # L1 缓存（SRAM 多 bank）
│           ├── TaskScheduler.h   # 任务调度器（数据搬运 + 计算触发）
│           ├── tile2_0.h         # OPU 单 tile 仿真器
│           ├── membanking.h      # 内存 banking 仿真器
│           ├── multicore.h       # 多核仿真器
│           └── tiling.h          # 动态 banking tiling 仿真器
│
├── src/                          # C++ 源文件（按图层次拆分）
│   ├── CMakeLists.txt            # 编译静态库 libPIMapping.a（GLOB_RECURSE 自动发现）
│   ├── cgraph.cpp                # CGraph: C-VDFG（crossbar 级）实现
│   ├── tgraph.cpp                # TGraph: T-VDFG（tile 级）+ 5 种 create_tnodes_* 策略
│   ├── hgraph.cpp                # HGraph: HCG（硬件连接图）zigzag/greedy/SPATEM 映射
│   ├── dgraph.cpp                # DGraph: DHCG（动态调度）BCE/XY 路由
│   ├── graph_io.cpp              # operator<< 重载（CNode/CEdge/TNode/TEdge/HNode/HEdge/DNode）
│   ├── analyzer.cpp              # 分析编排 + 日志初始化
│   ├── mapper.cpp                # 物理映射算法（BFS/zigzag/SPATEM）
│   ├── scheduler.cpp             # 拥塞感知路由（BCE + Dijkstra + XY）
│   ├── util.cpp                  # 辅助函数（距离/中位数/路径/组合）
│   ├── strategy/
│   │   ├── CStrategy.cpp         # CGraph 策略（Default/MNSIM/TILE2_0）
│   │   ├── TStrategy.cpp         # TGraph 策略（MNSIM/PIMAPPING/SPATEM/TILE2_0）
│   │   ├── HStrategy.cpp         # HGraph 策略（MNSIM/PIMAPPING/SPATEM）
│   │   └── DStrategy.cpp         # DGraph 策略（Default/PIMAPPING/TILE2_0）
│   └── simulator/
│       ├── Process.cpp           # 进程状态机
│       ├── Simulator.cpp         # 仿真引擎
│       └── tile2_0/              # Tile 2.0 各模块实现
│
├── test/                         # Google Test 测试
│   ├── CMakeLists.txt            # 每个 .cpp 自动生成一个测试可执行文件
│   ├── mappingalexnet.cpp        # Analyzer 全流程测试（AlexNet 风格 kernel）
│   ├── bankingtest.cpp           # BankingSimulator 测试（XY/YX/Custom 策略）
│   ├── multicoretest.cpp         # MulticoreSimulator 多核并行测试
│   ├── oputiletest.cpp           # OPU 单 tile 全流水线测试
│   └── tilingtest.cpp            # TilingSimulator 动态策略测试
│
├── result_develop/               # 回归测试基础设施
│   ├── scripts/
│   │   ├── collect_reference.py  # 收集参考基线数据
│   │   ├── compare.py            # 比对当前结果与参考基线
│   │   └── regression_test.sh    # 一键构建 + 测试
│   └── reference/                # 参考基线数据（生成，不跟踪）
│
├── MNSIM/                        # MNSIM Python 硬件建模库
│   ├── Hardware_Model/           # 硬件组件模型（Crossbar, PE, Tile, ADC, DAC 等）
│   ├── Latency_Model/            # 延迟估算（Tile/PE/Pooling 级）
│   ├── Energy_Model/             # 能耗估算
│   ├── Area_Model/               # 面积估算
│   ├── Power_Model/              # 推理功耗估算
│   ├── Accuracy_Model/           # 精度建模（Crossbar 非理想性等）
│   ├── Mapping_Model/            # 行为级映射 + Tile 连接图
│   ├── Interface/                # 训练/测试接口（网络定义、量化、数据集）
│   └── NoC/                      # 片上网络估算
│
├── pytorch/                      # PyTorch 参考模型
│   ├── model.py                  # FSRCNN 模型
│   └── mc_cnn_fast.py            # FastMcCnn 模型（默认转换示例）
│
├── models/                       # ONNX 模型文件（.onnx，不跟踪）
├── demo/                         # 快速测试用 ONNX 模型（.onnx，不跟踪）
├── runs/                         # 运行时日志（生成：cpp_analysis.log, perf.log）
├── results/                      # 输出结果 .pkl / .csv / .pdf（生成）
│
├── main.cpp                      # pybind11 模块入口（导出 pimapping 模块）
├── CMakeLists.txt                # 根 CMake（项目 PIMapping，生成 pimapping.so）
├── build.sh                      # 编译脚本（force clean + cmake + make）
├── test.sh                       # 测试脚本（编译 + 运行 gtest）
├── env.sh                        # 环境加载脚本（module load）
├── SimConfig.ini                 # 硬件仿真参数配置
├── techfile.txt                  # Booksim 功耗建模工艺文件
├── booksim                       # Booksim 2.0 NoC 仿真器（预编译二进制）
├── booksim_cfg                   # Booksim 配置模板
│
├── perf.py                       # 主性能分析脚本（全流程编排 + 可视化）
├── onnx_analysis.py              # ONNX 模型解析 → NNkernel 提取
├── torch2onnx.py                 # PyTorch 模型 → ONNX 转换
├── MappingInfo.py                # 延迟估算（Booksim + MNSIM 集成）
├── logger.py                     # 彩色日志工具
└── plot_simulator_stats.py       # 仿真器时间线可视化
```

## 快速开始

### 1. 加载环境

```bash
source env.sh
```

在组内服务器上加载所需模块（GCC 11.4, CMake 3.28, Python 3.11, Boost 1.84, GTest 1.14, pybind11 2.13）。其他设备需自行安装对应依赖。

### 2. 编译并测试

```bash
./build.sh                 # 编译（force clean）
./test.sh                  # 编译 + 运行全部回归测试
./test.sh mappingalexnet   # 编译 + 运行指定测试

# 启用 AddressSanitizer
cmake -S . -B build -DENABLE_ASAN=ON
make -C build -j$(nproc)
```

### 3. 执行性能分析

```bash
python3 perf.py
```

### 4. 回归测试

```bash
# 简易回归（demo 模型）
python3 result_develop/scripts/compare.py

# 全量回归（所有模型）
python3 result_develop/scripts/compare.py --full

# 一键构建 + 测试
bash result_develop/scripts/regression_test.sh
```

### 5. 代码格式化

```bash
clang-format -i src/*.cpp include/*.h
```

### 6. 配置校验

```bash
python3 config_validator.py [SimConfig.ini]
```

## 核心组件

### 数据流图层次

四层图表示（详见上方方法论章节），对应 `include/graph.h` 中的 `CGraph` / `TGraph` / `HGraph` / `DGraph` 类，均基于 Boost Graph Library（BGL）构建。

### 优化策略对比

PIMapping 将映射优化（mapping_opt）和调度优化（sched_opt）解耦，通过组合形成四种策略，另加 TILE2_0 高级流水线：

| 策略 | 映射方式 | 路由方式 | 优化目标 |
|------|----------|----------|----------|
| **MNSIM** | 顺序 tiling + zigzag | XY 路由 | 基线方案 |
| **HITM** | 同 MNSIM | 拥塞感知 BCE 路由 | 仅调度优化 |
| **SPATEM** | OU 级 tiling + zigzag | XY 路由 | 仅映射优化 |
| **PIMAPPING** | Intensity-Map 聚类 + 谱嵌入 | 拥塞感知 BCE 路由 | 映射+调度联合优化（本文方案） |
| **TILE2_0** | 容量约束 tiling + 贪心 | 拥塞感知 BCE 路由 | 高级流水线架构 |

策略通过 `OptInfo`（`mapping_opt`, `sched_opt`）和 `OptType` 枚举控制，在 `perf.py` 中自动遍历所有组合。

### 实验结果

在六种典型 DNN 模型（AlexNet, VGG16, ResNet50, DenseNet-121, Inception-v4, YOLOv5m）上评估，硬件参数：Crossbar 256×256, 32 Crossbars/Tile, 2D-Mesh NoC, 8Gb/s 带宽。

**整体性能**（vs MNSIM 基线）：
- 延迟降低 **47%–69%**（平均 **57.5%**）
- 吞吐量提升 **1.39×–6.03×**（平均 **3.17×**）
- 在复杂拓扑网络（DenseNet-121, Inception-v4）上仍保持显著提升，而 HITM/SPATEM 在此类网络上出现退化

**延迟分解分析：**
- 计算延迟显著降低，但更高并行度加剧通信需求
- SPATEM 实现最高计算延迟降低（~85%），但通信开销激增近一个数量级
- PIMapping 在计算并行度与通信开销之间取得平衡，实现总体最优

**带宽敏感性：** 随着 NoC 带宽提升，拥塞对性能的惩罚递减，各策略趋于计算上限。PIMapping 在所有带宽下相比 HITM 延迟降低 34.5%–44.6%，吞吐量提升 1.25×–1.89×。

**流水线策略：** 支持 LS（逐层同步）和 LP（多层并行）两种流水线模式。LP 显著降低推理延迟，但可能因增加单阶段内并行通信而降低吞吐量。

**Crossbar 尺寸探索：** PIMapping 在 256×256 配置下达到最优性能，体现了并行可扩展性与架构兼容性之间的权衡。

### 物理映射器（Mapper）

实现 Algorithm 1 的物理映射流程，通过策略模式注入不同映射算法：

- `CStrategyDefault::analysis()`：调用 `create_dup_num()` 进行吞吐量均衡复制
- `TStrategyPIMAPPING::analysis()`：调用 `create_tnodes_PIMAPPING()` 基于 Intensity Map 最大堆聚类
- `HStrategyPIMAPPING::analysis()`：调用 `greedy_mapping()` 基于 BFS 贪心放置
- `HStrategyMNSIM::analysis()`：调用 `zigzag_mapping()` 蛇形顺序放置（基线）
- `HStrategySPATEM::analysis()`：调用 `SPATEM_mapping()` OU 级放置

Mapper 通过 `shared_ptr<Mapper>` 注入到 `HGraph`，支持运行时替换映射策略。

### 路径调度器（Scheduler）

实现 Algorithm 2 的拥塞感知调度流程，通过策略模式注入不同路由算法：

- `DStrategyPIMAPPING::analysis()`：调用 `bce_routing()` 拥塞感知 BCE 路由
- `DStrategyDefault::analysis()`：调用 `xy_routing()` 简单 XY 路由（基线）

Scheduler 通过 `shared_ptr<Scheduler>` 注入到 `DGraph`，支持运行时替换路由策略。

核心算法：
- `init_bce()`：对 2D Mesh 图计算 Brandes 边介数中心性（BCE），识别关键通信资源
- `congestion_aware_routing()`：路径按曼哈顿距离升序、数据量降序排列，使用加权 Dijkstra 寻路。权重 w(e) = C-Σ(e) × BCE(e)，平衡拥塞降低与热点避免
- `xy_routing()`：简单 XY 路由（基线）

### Cycle-Accurate 仿真器

位于 `include/simulator/` 和 `src/simulator/`，基于事件驱动的模块化仿真框架，实现周期精确的存算架构性能模拟。其理论模型见《模拟器》第五章。

#### 数据流驱动模型

仿真器将计算数据流抽象为三层：**全局数据流**（系统级输入激励）→ **分块数据流**（存算核心间的输入-输出关系）→ **MVM 数据流**（存算阵列内部批次计算与乘累加流水）。在微架构层面，计算操作 μopᵢ 在微架构节点 μarchⱼ 上的行为映射为 (data, condition) 集合组，定义了参数和状态机依赖。微架构图的拓扑属性（深度、依赖方向）由数据流参考方向决定，确保模拟过程中数据流动的正确性与一致性。

计算流水线由状态机驱动而非指令流：片上缓存容量约束下输入特征图分块加载 → 本地缓存按列读取，累积连续 N 列有效数据后触发计算 → 激励分发逻辑生成访存请求 → 寄存器堆按周期移位输入形成计算流水 → 结果经后处理发送至总线。事件计数器在事件执行后更新，反映当前执行状态，驱动后续事件触发。

#### 三类核心原语

| 原语 | 职责 | 关键语义 |
|------|------|----------|
| **模块原语** | 微架构统一封装 | 信号属性（输入/输出信号列表、有效性、时间戳）、功能函数属性（触发条件、执行周期数、状态转换逻辑）、接口属性（模块间信号绑定与传输） |
| **控制依赖原语** | 维护模块调用顺序 | 可用性（占用状态标志、占用/释放条件）、调用条件（输入/内部信号组合与触发函数绑定、优先级定义） |
| **事件原语** | 执行机制抽象 | 功能属性（绑定的功能函数、执行状态）、时间属性（触发/执行/完成/结束/性能计数五种时间戳），分别建模计算行为（模块内部执行）和通信行为（模块间接口交互） |

#### 事件驱动机制

事件状态更新模型定义五阶段流程：**触发时间戳**（前级输出就绪）→ **执行时间戳**（模块开始接收数据并计算，状态更新为"占用"）→ **完成时间戳**（计算结束，输出置为有效）→ **结束时间戳**（模块状态恢复可用，信号组无效化）。

并发事件按拓扑深度降序（逆流水线方向）执行，保证"在 T 时刻所有输入数据已就绪，且当前输出更新不影响同一时刻未执行事件的输入"。两类违例依赖通过组合逻辑前移（插入高优先级事件栈）和时序输出入队（数据队列化）消除。

#### 多核事务机制

多核模拟通过三层抽象扩展单核事件驱动：**事务构造**（计算图节点抽象为事务）→ **事务分配**（硬件映射）→ **事务间消息传递**（多核调度）。任务依赖原语维护活跃计算任务表，消息原语封装核间通信事件参数。事务机制实现了架构无关的消息传递与架构相关的回调处理的分离设计。

#### Tile 2.0 架构模型

基于 OPU-Tile2.0 真实芯片架构建模，包含 Crossbar（存算阵列计算+切换）、SIMD（量化→激活→池化流水线）、L1C（多 bank SRAM 缓存）、TaskScheduler（任务调度与数据搬运）四大模块，以及 BankingSimulator（内存 banking）、MulticoreSimulator（多核并行）、TilingSimulator（动态 tiling）等上层封装。

## Python 集成

### pimapping 模块（pybind11）

C++ 核心通过 `main.cpp` 导出为 Python 模块 `pimapping`：

```python
import pimapping

# 核心分析函数
result, comm_info = pimapping.analyze(kernels, hw_info, opt_info)

# 调试函数
pimapping.test()
```

导出的数据结构：`NNkernel`, `DepInfo`, `HWInfo`, `OptInfo`, `AnalysisResult`, `DeployInfo`, `CommInfo`, `CommSeg`, `Path`, `CNode`。

导出的模拟器类型：`ProcessEvent`（事件生命周期和性能统计）、`ProcessState`（IDLE/TRIGGERED/EXECUTING/FINISHED/ENDED 枚举）。

### 主要 Python 脚本

| 脚本 | 功能 |
|------|------|
| `perf.py` | 入口模块（35 行），re-export 分析/绘图函数 |
| `analysis.py` | 性能分析函数（perf_analysis, get_noc_perf, power_analysis 等） |
| `plotting.py` | 可视化函数（延迟/吞吐量/功耗/带宽图表） |
| `onnx_analysis.py` | ONNX 模型解析 → NNkernel 提取 |
| `MappingInfo.py` | 延迟估算集成（MNSIM tile 级计算延迟 + Booksim NoC 通信延迟） |
| `torch2onnx.py` | PyTorch → ONNX 转换工具 |
| `config_validator.py` | SimConfig.ini schema 验证（30+ 参数的类型/范围/必填检查） |
| `logger.py` | 彩色日志工具（输出到 `runs/perf.log`） |

## 硬件配置

编辑 `SimConfig.ini` 修改硬件参数，主要配置项：

| 层级 | 关键参数 |
|------|----------|
| Device | 工艺节点、器件类型（NVM/SRAM）、面积、读写延迟/电压 |
| Crossbar | 阵列尺寸（WL, BL）、子阵列大小、单元类型 |
| PE | PIM 类型（模拟/数字）、DAC/ADC 精度、Buffer 大小 |
| Digital | 数字模块频率、加法器/移位寄存器/寄存器参数 |
| Tile | PE 数量与排列、Pooling 尺寸、片内/片间带宽 |
| Architecture | Buffer 选择、Tile 数量与排列、NoC 使能 |

## 注意事项

- 所有命令需在项目根目录下执行（脚本使用相对路径）
- `build.sh` 每次执行会**强制清理** `build/` 目录
- `.gitignore` 为白名单模式：默认忽略所有文件，仅追踪 `.cpp`/`.h`/`.py`/`.sh`/`CMakeLists.txt`/`.md`/`.ini`/`.cfg` 等代码文件。添加新文件类型需更新 `.gitignore`
- 编译启用 `-Wall -Wextra -Wpedantic` 警告，构建类型默认 `Release`（支持 `-DCMAKE_BUILD_TYPE=Debug`）
- C++ 日志输出到 `runs/cpp_analysis.log`（Python 日志输出到 `runs/perf.log`）
- 测试可执行文件需链接 `pthread`（已在 `test/CMakeLists.txt` 中配置）
- Python 模块 `pimapping` 必须可导入 — 构建产物 `.so` 需在项目根目录或 `PYTHONPATH` 中
# PIM-sim（PIMapping）

面向存算一体芯片的神经网络映射与性能评估框架，采用 C++20、Python、pybind11 和 Boost Graph。C++ 核心提供层次图、物理映射、通信调度与事件模拟；Python 提供 ONNX 解析、MNSIM/Booksim 集成和结果导出。

## 功能与分析路径

```text
ONNX → pim_sim.onnx_analysis → NNkernel → Analyzer → CGraph → TGraph → HGraph → DGraph
                                            ├─ analyze → MNSIM / Booksim → 聚合性能估计
                                            └─ export_workload → 任务 JSON
                                                                  └─ simulate_workload → 周期结果
```

| 路径 | 输入与输出 | 模型范围 |
|---|---|---|
| 映射与聚合性能分析 | ONNX、硬件配置与映射策略 → 分块、部署、通信统计及性能 JSON | MNSIM 按 Tile/分段聚合计算；Booksim 使用端点流量与 DOR 路由，带宽系数采用延迟比值估计 |
| 聚合 Tile 周期模拟 | 实际映射与显式时序参数 → 任务、通信、资源占用与停止原因 | 每 Tile 同时执行一个任务；逐依赖单播、逐跳传输、全局有界网络准入；累加依赖作为计算前置条件 |
| Tile2.0 架构示例 | 手工任务与编译期硬件参数 → OPU、Banking、MultiCore、Tiling 事件轨迹 | Crossbar、SIMD、L1C、TaskScheduler 模块；输入按逻辑批次流式供给 |

ONNX 前端以静态 batch=1 的普通 Conv（group=1）与 Gemm 建立计算节点，其他算子沿张量依赖跳过，不分配独立计算节点或计算时延。Pool/Resize 更新传输尺寸，Concat 保留输入来源与目标通道偏移，Flatten/Reshape 转换元素范围，Add/Mul 等保留各数据来源。其他合法算子保留数据依赖，并在结果中记录采用的近似处理。卷积自身的输出尺寸用于部分和累加，后处理尺寸用于传播通信，两者分别保存。

模型格式、权重形状、通道、依赖和资源容量始终检查。`--strict-operators` 可额外要求仅使用保持形状的算子子集；默认使用上述计算图投影。输出描述逻辑权重分块与工作量，不包含张量数值计算、物理权重编码或 MMIO 程序。性能数值取决于所选模型和时序参数，使用真实芯片结果前需要校准。

## 快速开始

需要 Linux、CMake 3.28+、支持 C++20 的编译器、Boost graph/json、GTest、pybind11 和 Python 开发环境。完整安装与运行步骤见[使用指南](docs/guides/usage.md)。

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-analysis.txt
scripts/build.sh
scripts/test.sh
scripts/python.sh -m unittest discover -s tests/python -p 'test_*.py' -v

# 生成小模型并执行周期模拟，输出路径须尚未存在
scripts/python.sh examples/create_tiny_model.py artifacts/runs/tiny.onnx
scripts/python.sh examples/run_mapped_workload.py artifacts/runs/tiny.onnx \
  --timing configs/workloads/tiny.json --xbar 1 1 --xbars-per-tile 1 \
  --mesh 1 2 --output artifacts/runs/tiny-cycle
```

小模型预期完成 2 个任务，发送与接收各 1 字节，`makespan_cycles=7`、`latency_ns=14`。样例时序是手算参数。构建默认写入 `build/default/`，Python 扩展在 `build/default/python/`；`scripts/python.sh` 选择解释器并设置模块路径。构建采用增量方式，普通构建与检查构建可用 `BUILD_DIR` 隔离。

MNSIM/Booksim 示例会生成自己的 ONNX 输入：

```bash
scripts/python.sh examples/run_analysis_example.py --output-dir artifacts/runs/analysis-example
scripts/python.sh -m pim_sim --help
```

完整聚合分析需要可执行的 Booksim，位置与依赖说明见[使用指南](docs/guides/usage.md)。大型模型、Python 环境、研究参考和运行结果由使用者在本地准备。

## 图与模拟语义

- **CGraph** 按输入/输出通道划分逻辑权重矩阵。特征图大小和通信量以标量元素计；通道区间从 1 开始且包含两端。`DepInfo.dep_layer` 指消费者层号，`-1` 表示终端；传播依赖可分别指定源/目标通道与投影后的元素量，以支持池化、拼接与展平。
- **TGraph** 将 C 节点聚合到 Tile，按源 C 节点、目标 Tile、依赖类型及通道区间统计多播发送量。孤立节点也分配资源。
- **HGraph** 将 Tile 放置到二维 mesh。显式网格容量不足时报错；`(0,0)` 自动选择网格。可用放置策略包含 zigzag、median/BFS 贪心和 SPATEM 启发式。
- **DGraph** 按拓扑深度与固定 `pipeline_depth` 分段，保留只有计算的阶段。路由支持 XY 和 BCE/C-Σ 顺序贪心，并以固定规则处理等价路径。
- **周期引擎** 以整数 cycle 计时。同周期消息先于信号交付，模块按深度降序稳定求值，零延迟事件通过 delta 迭代处理。架构模型定义输入消费、输出有效和握手规则。
- **完成判断** 同时要求工作负载完成、进程结束和事件排空。结果区分 `Completed`、`Quiescent`、`MaxCycles`、`Error`；未完成的任务模型结果使用 `null` 表示完整推理延迟。

`OptInfo` 的两项开关分别控制映射与调度优化，保留 MNSIM、HITM、SPATEM、PIMAPPING 四组名称。聚合通信统计的多播假设与周期任务模型的逐边单播假设不同，比较结果时应同时检查数据量、路由和资源规则。

## 代码与文档

| 位置 | 内容 |
|---|---|
| `include/`、`src/cpp/` | C++ 图、策略、映射、调度、周期引擎及架构模块 |
| `src/bindings/python.cpp` | `pimapping` Python 扩展接口 |
| `src/python/pim_sim/` | Python 分析包、模型解析、缓存及可视化 |
| `configs/`、`examples/` | 硬件/网络/任务配置与可运行示例 |
| `third_party/` | MNSIM 源码与 Booksim 集成位置 |
| `tests/`、`scripts/` | 可复用测试、构建及回归工具 |

继续阅读[目录说明](docs/guides/project-layout.md)、[模拟器架构](docs/architecture/simulator.md)和[任务负载契约](docs/architecture/workload.md)。默认运行文件归入 `artifacts/`，可用 `PIM_ARTIFACTS_DIR` 改变根目录；模型、配置、策略和实现内容共同参与缓存身份。

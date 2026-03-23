# PIM-sim 使用指南

本项目是一个面向存内计算（PIM）架构的神经网络映射与性能评估框架，支持从 ONNX 模型输入到硬件级延迟、功耗、通信等指标的全流程仿真分析。

## ▶️ 快速开始

### 1. 加载运行环境

在组内服务器上可执行以下命令加载所需模块和库：

```bash
./env.sh
```

该脚本会自动加载编译器、Python 环境及所有依赖库，并验证版本是否匹配。
> 对于其他设备和个人环境，请自行安装所需模块和库。

---

### 2. 编译 C++ 核心模块并运行基础测试

执行测试脚本以完成编译和功能验证：

```bash
./test.sh
```

此命令将：
- 清理并重建 `build/` 目录
- 使用 CMake 编译 C++ 源码生成共享库
- 运行 Google Test 基础回归测试（确保核心分析逻辑正常）

> 若需运行特定测试项：`./test.sh <test_name>`

---

### 3. 执行性能测速分析

运行主性能评估脚本：

```bash
python3 perf.py
```

该脚本将自动：
- 从 `models/` 目录加载 ONNX 模型（如 `resnet18.onnx`）
- 执行算子映射、Tile 分配、通信路径生成
- 调用 Booksim 仿真片上网络（NoC）延迟与功耗
- 计算端到端延迟、吞吐量、能效等指标
- 生成结果文件（`.pkl`, `.csv`）和可视化图表（PDF）

> 💡 **提示**：确保 `models/` 目录中包含待测 ONNX 模型。若无模型，可先使用 torch2onnx.py 转换示例网络。

---

## 🔧 可选工具

### 转换 PyTorch 模型为 ONNX

```bash
python3 torch2onnx.py
```

默认生成 `mc_cnn` 示例模型的 ONNX 文件并保存至 `models/`。

### 单独分析 ONNX 模型

```bash
python3 onnx_analysis.py [path/to/model.onnx]
```

若未指定路径，默认分析 `models/resnet18.onnx`。

---

## 📁 输出结果

- **日志**：`runs/perf.log`
- **原始数据**：`results/*.pkl`, `results/*.csv`
- **图表**：`results/*.pdf`（含延迟、吞吐量、通信开销等对比图）

---

## 📝 注意事项

- 所有路径均为相对路径，请在项目根目录下执行命令。
- 首次运行 perf.py 时会触发 Booksim 仿真，耗时较长；后续运行将复用缓存结果。
- 修改硬件配置（如 Xbar 尺寸、位宽等）请编辑 SimConfig.ini 。

--- 

按照以上三步（**加载环境 → 编译测试 → 执行分析 **），即可完成完整的 PIM 架构性能评估流程。
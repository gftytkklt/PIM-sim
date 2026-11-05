# 项目目录说明

项目按源码、配置、示例、验证和运行产物分层。Python 包名为 `pim_sim`，C++ 扩展名为 `pimapping`，CMake 项目名为 `PIMapping`。

```text
PIMapping/
├── CMakeLists.txt                # 总构建入口
├── include/                     # C++ 公共头文件
├── src/
│   ├── cpp/                     # 图、策略、模拟器实现
│   ├── bindings/python.cpp      # pybind11 接口
│   └── python/pim_sim/          # Python 分析包
├── configs/
│   ├── hardware/SimConfig.ini   # MNSIM 硬件配置
│   ├── booksim/                 # 网络与工艺配置
│   └── workloads/tiny.json      # 小模型时序示例
├── examples/                    # 输入生成与运行示例
├── tests/{cpp,python}/          # 可复用的正确性与集成测试
├── scripts/                     # 构建、运行、测试与回归入口
├── third_party/                 # 外部分析组件
├── docs/{guides,architecture}/  # 使用与架构说明
├── experiments/                 # 本地实验输入及参考的约定位置
├── build/                       # 本地构建输出
└── artifacts/                   # 运行产物
```

`build/`、`.venv/`、大型模型、实验参考与运行产物不随源码分发。`experiments/` 中的 README 说明本地数据的准备位置。测试和小模型生成示例不依赖下载大型模型。

## 运行入口

| 入口 | 用途 |
|---|---|
| `scripts/build.sh` | 增量配置与编译，默认 `build/default/` |
| `scripts/test.sh [测试名]` | 构建并执行 CTest，可指定精确测试名 |
| `scripts/python.sh -m pim_sim` | 启动 Python 分析命令 |
| `scripts/python.sh examples/run_mapped_workload.py …` | 导出实际映射任务并运行周期模拟 |
| `scripts/env.sh` | 组内 module 服务器依赖加载 |
| `scripts/regression/` | 显式参考生成、结果比较与事件轨迹比较 |

Python 启动器按 `PYTHON`、本地 `.venv/bin/python3`、系统 `python3` 的顺序选择解释器，并设置所选构建的扩展、Python 源码和第三方目录。`BUILD_DIR` 相对值按仓库根目录解析；自定义构建与 Python 运行应使用同一个值。

配置和依赖的默认路径相对仓库定位，与当前目录无关。用户显式传入的模型、配置、输出路径相对调用位置解析。`scripts/python.sh` 保留调用者当前目录；在仓库外运行时应传入示例脚本的绝对路径。

## 数据与运行产物

| 路径 | 内容 |
|---|---|
| `experiments/inputs/demo/` | 本地小规模研究输入，作为分析 CLI 默认模型目录 |
| `experiments/inputs/models/` | 本地模型集合 |
| `experiments/inputs/xbars/` | 阵列尺寸实验输入 |
| `experiments/references/legacy/` | 使用者自行准备的历史回归参考 |
| `artifacts/logs/` | C++ 与 Python 日志 |
| `artifacts/cache/` | 内容身份缓存及绘图库缓存 |
| `artifacts/results/` | 图表等结果文件 |
| `artifacts/runs/` | 每次分析的配置、临时流量和结果 |

`PIM_ARTIFACTS_DIR` 可覆盖默认的 `artifacts/` 根目录。相对覆盖值按调用者工作目录解析；显式命令行输出路径优先。运行结果、模型和参考分开保存，便于重复执行与比较。

`configs/hardware/SimConfig.ini` 配置 MNSIM 路径；`configs/workloads/*.json` 配置聚合任务周期模型；`include/simulator/tile2_0/config.h` 中的 `hw_config` 是 Tile2.0 示例编译期参数。三者分别生效，修改一项不会自动改写其他模型。

详见[使用指南](usage.md)、[模拟器架构](../architecture/simulator.md)和[任务负载契约](../architecture/workload.md)。

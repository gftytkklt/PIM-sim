# PIM-sim 使用指南

以下命令从仓库根目录执行。`scripts/build.sh` 和 `scripts/test.sh` 会自行定位仓库；`scripts/python.sh` 保留调用者的工作目录，显式输入/输出相对路径按调用位置解析。目录职责见[项目布局](project-layout.md)。

## 环境与构建

Ubuntu 24.04 可安装以下开发依赖：

```bash
sudo apt-get update
sudo apt-get install -y cmake g++ libboost-all-dev libboost-json-dev \
  libgtest-dev pybind11-dev python3 python3-dev python3-pip python3-venv
python3 -m venv .venv
.venv/bin/python -m pip install -r requirements-analysis.txt
scripts/build.sh -DCMAKE_BUILD_TYPE=Release
```

组内 module 服务器可先执行 `source scripts/env.sh`。该脚本需要 `module` 命令及对应已安装模块；其他机器使用本机依赖。`requirements-analysis.txt` 是分析与绘图依赖，PyTorch 模型导出示例另外需要匹配的 PyTorch/torchvision 环境。

构建脚本优先使用 `PYTHON` 指定的解释器，其次使用 `.venv/bin/python3`，最后使用系统 `python3`。`BUILD_DIR` 默认 `build/default`，相对值按仓库根目录解析；`BUILD_JOBS` 默认 2。可用 `CMAKE_BIN` 指定 CMake。构建增量执行，失败返回非零，不自动删除构建目录。

```bash
scripts/test.sh                       # 构建并运行全部 CTest
scripts/test.sh workload_test         # 精确匹配测试名，无匹配时报错
scripts/python.sh -c 'import pimapping; print(pimapping.__file__)'
scripts/python.sh -m unittest discover -s tests/python -p 'test_*.py' -v
```

Python 启动器设置 `<BUILD_DIR>/python`、`src/python` 和 `third_party` 的导入路径，并用 `-P` 避免当前目录覆盖模块。常用包入口为 `pim_sim`；底层扩展保持 `import pimapping`。指定独立构建时，构建、测试和 Python 启动器使用同一 `BUILD_DIR`。

## 映射与 MNSIM/Booksim 聚合分析

MNSIM 源码位于 `third_party/MNSIM/`。完整网络分析需要可执行文件 `third_party/booksim/bin/booksim`，其网络配置为 `configs/booksim/mesh.cfg`，工艺参数为 `configs/booksim/techfile.txt`。缺少程序、执行失败、超时或缺少必要统计时会报错。仅查看命令帮助和执行 `--mapping-only` 不需要运行 Booksim。

```bash
scripts/python.sh -m pim_sim --help
# 自动生成小模型，运行四种策略和聚合性能分析
scripts/python.sh examples/run_analysis_example.py --output-dir artifacts/runs/analysis-example
# 使用自己的受支持 ONNX 模型目录
scripts/python.sh -m pim_sim --models-dir path/to/models --mesh 4,4 \
  --xbars-per-tile 8 --config configs/hardware/SimConfig.ini \
  --output artifacts/runs/my-analysis.json
```

示例输出目录须尚未存在，已有结果文件不会覆盖。`--mapping-only` 只执行映射，`--single-strategy` 只执行同时开启映射和调度优化的一组。`--mesh 0,0` 自动选择尺寸；显式尺寸不足时报错。默认输入目录是本地 `experiments/inputs/demo/`，公开源码不附带大型 ONNX 文件，首次使用可先运行自动生成输入的示例。

计算节点采用静态 batch=1 的 Conv（group=1）与 Gemm。默认跳过其他算子的独立计算，沿真实张量连接保留数据来源；Pool/Resize、Concat、Flatten/Reshape、Add/Mul 等分别更新传输尺寸、通道位置或来源集合。未知的合法非矩阵算子使用依赖近似投影，其名称和处理方式写入 `provenance.projections`。卷积原始输出与后处理的通信量分别计算，避免池化尺寸改变部分和体积，或把展平后的全连接输入误算为单通道。

ONNX 格式与必要的静态形状、权重、通道关系始终检查。若需要额外检查算子子集，可加入 `--strict-operators`；它会拒绝改变形状或超出该子集的算子。该选项不影响默认的跳过策略。

```bash
# 部署映射：本地目录中的所有模型、四组策略
scripts/python.sh -m pim_sim --models-dir experiments/inputs/models --mapping-only \
  --xbar-size 256,256 --xbars-per-tile 8 --mesh 0,0 \
  --output artifacts/runs/models-mapping.json
# 额外算子子集检查（可选）
scripts/python.sh -m pim_sim --models-dir path/to/models --mapping-only --strict-operators
```

JSON 输出包含模型与实现来源、硬件配置、策略、分段数据量及估计假设。图中的数据量是标量元素，按元素位宽换算传输字节；MNSIM 返回 ns，聚合分析输出秒。逻辑 Xbar 尺寸必须与 MNSIM 配置一致才能联合估计。逻辑阵列容量与 MNSIM 的 PE/group 组织分别配置。MNSIM 按每 Tile 每分段聚合计算，Booksim 按端点流量运行 DOR；带宽比率为估计系数，这些参数需要针对目标硬件校准。

## 映射到周期任务模拟

无需下载模型即可生成两层 1×1 Conv：

```bash
scripts/python.sh examples/create_tiny_model.py artifacts/runs/tiny.onnx
scripts/python.sh examples/run_mapped_workload.py artifacts/runs/tiny.onnx \
  --timing configs/workloads/tiny.json --xbar 1 1 --xbars-per-tile 1 \
  --mesh 1 2 --output artifacts/runs/tiny-cycle
```

预期完成 2 个任务、收发各 1 字节，`makespan_cycles=7`、`latency_ns=14`。任务 0 用 2 周期；通信串行化和跳延迟各 1 周期；任务 1 用 3 周期。`configs/workloads/tiny.json` 仅为这个手算示例提供参数。

使用自己的模型和时序参数：

```bash
scripts/python.sh examples/run_mapped_workload.py path/to/model.onnx \
  --timing path/to/timing.json --xbar 256 256 --xbars-per-tile 1 \
  --mesh 2 2 --max-cycles 1000000 --output artifacts/runs/my-workload
```

时序 JSON 必须明确每层**每个 C 分块任务**的周期、元素位宽、链路字节/周期、每跳周期、全局在途传输容量与时钟周期 ns。模型每 Tile 同时占用一个任务，所有输入依赖收到后开始计算；网络满时生产者保持资源占用。完整字段和资源规则见[任务负载契约](../architecture/workload.md)。

输出 `workload.json` 包含实际映射节点、同核与跨核依赖及路由。`result.json` 包含任务开始/计算完成/资源释放、消息发送/接收、占用时间、收发字节及停止原因。未完成返回非零退出码，完整推理延迟为 `null`。结果 JSON 和工作量 JSON 不携带张量数值。

```python
import json
import pimapping

workload = pimapping.export_workload(kernels, hw_info, opt_info, json.dumps(timing))
result = json.loads(pimapping.simulate_workload(workload, max_cycles=1000000))
assert result["completed"] and result["stop_reason"] == "Completed"
assert result["sent_bytes"] == result["received_bytes"]
```

`kernels`、`hw_info`、`opt_info` 使用扩展公开类型构造，完整调用见 `examples/run_mapped_workload.py`。OPU/Banking/MultiCore/Tiling 示例通过各自模块图和手工任务运行，模块接口见[模拟器说明](../architecture/simulator.md)。

## 检查构建与确定性

```bash
BUILD_DIR=build/sanitized scripts/build.sh -DCMAKE_BUILD_TYPE=Debug \
  -DENABLE_ASAN=ON -DENABLE_UBSAN=ON
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  ctest --test-dir build/sanitized --output-on-failure --no-tests=error --timeout 60
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 UBSAN_OPTIONS=halt_on_error=1 \
  scripts/python.sh scripts/regression/verify_simulator_traces.py \
    --normal build/default --sanitized build/sanitized --output artifacts/tests/traces.json
```

普通与 ASan/UBSan 构建使用相同完成、周期和守恒断言；轨迹脚本比较正常重复运行与检查构建。LeakSanitizer 需要支持其进程检查方式的环境，ptrace 限制导致的检测失败应与代码检查结果分别记录。Python 分析使用普通扩展，避免直接加载需要预加载运行库的 ASan 扩展。

## 回归参考与输出

```bash
scripts/python.sh scripts/regression/compare.py --help
scripts/python.sh scripts/regression/compare.py --ctest-only --reference-dir path/to/reference
```

回归脚本支持 `--perf-only`、`--full` 与 `--build-dir`。性能比较使用本地 `experiments/inputs/demo/`，`--full` 另使用 `experiments/inputs/models/`；默认参考目录为 `experiments/references/legacy/`。模型与研究参考不随源码分发，缺失参考、无效参考、空比较或执行失败都会返回失败。`collect_reference.py --output-dir <新目录>` 显式生成候选参考，已有目录不覆盖；更新参考应附独立的预期依据和结果差异说明。

默认日志位于 `artifacts/logs/`，缓存位于 `artifacts/cache/`，图表位于 `artifacts/results/`，分析运行文件位于 `artifacts/runs/`。设置 `PIM_ARTIFACTS_DIR=/path/to/output` 可统一改变默认输出根目录；显式 `--output`/`--output-dir` 仍按传入路径使用。缓存身份包含模型、配置、策略、实现与依赖内容，历史研究参考与运行缓存分别管理。

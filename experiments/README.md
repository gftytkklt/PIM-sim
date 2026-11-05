# 实验数据目录

本目录约定本地实验输入与回归参考的位置。大型模型、压缩归档和参考结果由使用者准备，不随源码分发。

| 目录 | 用途 |
|---|---|
| `inputs/demo/` | 小规模研究输入，分析 CLI 默认读取此目录中的 ONNX 文件 |
| `inputs/models/` | 模型集合，完整性能回归使用 |
| `inputs/xbars/` | 阵列尺寸实验输入 |
| `references/legacy/` | 已有回归参考 |
| `archives/` | 本地保留的原始压缩包 |

无需大型模型即可运行 `examples/create_tiny_model.py` 和 `examples/run_analysis_example.py`。测试生成的输入和新运行结果建议写入 `artifacts/`，操作步骤见[使用指南](../docs/guides/usage.md)。

回归比较与参考生成分开：比较缺少参考时返回失败；`scripts/regression/collect_reference.py` 需要显式指定一个尚不存在的输出目录。

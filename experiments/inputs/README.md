# 模型输入

`demo/`、`models/` 和 `xbars/` 分别保存本地小规模实验、模型集合和阵列尺寸实验的 ONNX 输入。分析器只处理 `.onnx` 文件，并验证支持的算子、静态形状和依赖。

大型模型不随源码分发。可用 `scripts/python.sh examples/create_tiny_model.py artifacts/runs/tiny.onnx` 生成两层 Conv 输入，或运行自动准备输入的聚合分析示例。详见[使用指南](../../docs/guides/usage.md)。

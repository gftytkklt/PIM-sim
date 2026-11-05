# 回归参考

默认比较目录为 `legacy/`，参考文件不随源码分发。`scripts/regression/compare.py --reference-dir <目录>` 可选择已有参考；参考缺失、损坏或无有效比较条目时返回失败。

`scripts/regression/collect_reference.py --output-dir <新目录>` 用于显式生成候选参考。候选结果应结合输入、配置、版本和独立预期检查后再采用；已有目录不会被覆盖。完整命令见[使用指南](../../docs/guides/usage.md)。

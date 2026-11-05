# 聚合 Tile 任务负载契约

`pimapping.export_workload` 将 Analyzer 的 C/T/H/D 图导出为版本化 JSON，`pimapping.simulate_workload` 在周期事件引擎上执行 `aggregate_tile_v1` 模型。任务保留 CGraph 节点身份、通道分块、资源分配、依赖与实际路由。

接口定义位于 `include/simulator/Workload.h`，实现在 `src/cpp/simulator/Workload.cpp`，绑定位于 `src/bindings/python.cpp`。运行入口是 `examples/run_mapped_workload.py`，用法见[使用指南](../guides/usage.md)。

## 输入JSON（schema_version=1）

|字段|单位及语义|校验/生成来源|
|---|---|---|
|model|固定`aggregate_tile_v1`|不混入Tile2.0常量|
|time_unit / data_unit|`cycle` / `element`|错误单位拒绝|
|hardware|tile_shape、xbar_shape、xbars_per_tile、pipeline_depth|来自HWInfo；映射容量约束；实际自动网格须解析成明确尺寸|
|strategy|mapping_opt、sched_opt|保留映射与调度优化开关|
|timing.layer_cycles|各层**每个C分块**计算周期，可为0|调用者显式给出；不是整层总周期|
|element_bits|每个依赖数据元素位数|字节数=ceil(elements×bits/8)，检查溢出|
|link_bytes_per_cycle / hop_latency_cycles|链路串行化带宽/每跳延迟|前者>0，后者≥0|
|network_capacity|全局在途传输条数|正整数；非路由器缓冲字节数|
|clock_period_ns|周期转ns|必须有限且>0；乘积不得溢出|
|tasks[].id|CGraph顶点ID|从0连续唯一；不以层号代替任务ID|
|layer / kernel_shape|源算子层号/窗口尺寸|分块输入通道数×窗口面积≤WL，输出通道数≤BL|
|input_channels / output_channels|从1开始的闭区间|正数、有序，保留不连续的各独立区间|
|ifmap_elements / ofmap_elements|分块内输入/输出标量元素**总数**|不是每通道空间尺寸；须整除对应通道数|
|tile / xbar_slot|物理坐标/Tile内槽号|C→T归属、H映射；槽唯一且容量合法|
|dependencies[]|每条实际C边|同核、累加、平行边均保留|
|src / dst / kind / channels|端点、accumulation/propagation、源通道包络|端点存在、无自环/环；带projection的传播边按投影流核对源包络，其余边沿用两端共享通道范围|
|elements / bytes / route|标量数/字节数/含首尾坐标的路径|非负；字节数与位宽一致；体积按下述投影或旧格式规则校验；路径匹配DGraph源Tile/源层/目标Tile，邻接、边界、无环均检查|
|projection.dep_layer / tensor_id|目标MVM层号/投影视图标识|仅传播边使用；层号必须等于目标任务层；标识是字符串，用于区分投影视图|
|projection.source_channels / destination_channels|源MVM输出/目标MVM输入的完整依赖通道闭区间|两者可有不同起点和长度；须分别包含于对应层全部任务所覆盖的通道并集|
|projection.elements / stream_range|投影后完整依赖的元素数E/本条C边的0基半开元素区间|E是显式非负整数；从源、目标任务通道切片重算交集，区间和边elements必须与交集一致|

新导出的传播边携带`projection`。E描述池化、拼接、展平等投影之后的传输量，不能由原MVM任务的`ofmap_elements`反推，也不要求等于目标通道完整空间体积。任务保留原MVM计算输入/输出形状。对于一侧长度为N的完整通道区间，第i个通道边界对应虚拟流位置`floor(E*i/N)`；每个任务通道块映射为流切片，本条边的`stream_range`是源切片和目标切片的交集，边`elements`等于交集长度。计算使用整数前缀，避免浮点取整和中间乘积溢出。`channels`保留源通道包络；展平切片可以只占一个源通道的部分空间元素，不能再按源通道数整除验证。

例如源2通道各4个元素经展平变为目标8个输入通道，完整流为`[0,8)`。源通道1对应`[0,4)`，目标通道4…6对应`[3,6)`，该C边只传交集`[3,4)`的1个元素，源包络为`[1,1]`。

没有`projection`的旧JSON继续接受原格式：非零边的`channels`必须同时落在两端相应通道范围内，体积等于源对应通道的完整空间体积，两端每通道空间体积相同。累加边始终使用此规则，不允许附加投影描述。`elements=0`是明确的纯控制依赖，仍须交付token，传输不占网络容量且不产生带宽延迟；投影格式还要求重算交集确实为空，两个非空切片仅端点相邻不构成依赖。

JSON是可审查的任务/时序契约，不携带张量数值或权重数值，不执行数值正确性验证。投影元数据的内部一致性不等于对任意非MVM算子完成了数值或硬件时延验证；相关后处理时延仍由调用者在适当粒度提供。

## 执行、反压与结果

- 每Tile一次只占用一个任务，按稳定任务ID仲裁。C节点权重槽位仍静态唯一，计算资源采用保守串行抽象。
- 所有输入依赖已接收才启动计算。计算完成进入发送状态；所有输出被网络接纳后释放生产者Tile。网络满时保留占用，不能提前报告任务结束。
- 同核依赖即时交付；跨核数据按真实映射路由逐跳store-and-forward。每跳先串行化`ceil(bytes/bandwidth)`周期，再加hop_latency；各方向独立占用链路（全双工假设）。容量按传输条目计，接收后释放。
- 每条C边单独发送/接收。T层可能对多播去重，因此本模型逻辑流量不保证等于旧聚合流量。旧Scheduler对无向边合计拥塞，本模型按方向分别计链路占用，两者不是同一NoC争用模型。
- 零延迟任务在同一周期delta内推进，迭代上限按已验证DAG规模设置。空负载在时刻0完成。实例每次重新创建，重复模拟应返回相同JSON。
- `Completed`要求全部任务释放、全部依赖交付且引擎排空。`MaxCycles`/`Quiescent`不输出完整推理延迟（null）。结果记录计算finish和资源release两个不同时间戳，后者可因反压晚于前者。
- 完成时makespan为最后完成/交付的绝对时刻；从时刻0起，时刻7完成表示7周期跨度。引擎处理了0…7共8个周期点，因此`elapsed_cycles=8`与`makespan_cycles=7`不矛盾。资源利用率=占用跨度/makespan；未完成时只给截至观察窗口的busy_cycles，utilization=null。

## 参数与模型范围

- `layer_cycles` 对每个 C 分块分别计时。多个输入通道分块的累加边也作为计算启动前置条件，因此相关任务保守串行执行。
- 网络采用逐跳存储转发、方向独立链路和全局在途传输数量上限。逐路由器 FIFO、credit、虚通道和 Booksim 在线协同需要另外的网络模型。
- 该模型在任务层描述计算、通信和资源占用。ADC/DAC、访存、SIMD 的独立事件，以及输入消费和 ready/valid 信号协议由具体微架构模型定义。
- 时序参数由调用者给出，JSON 不生成寄存器、MMIO、DMA 地址或权重编码。针对目标硬件使用时，应提供与任务粒度匹配的参数和校准依据。

## 验证入口

`tests/cpp/workload_test.cpp` 检查手算周期、容量、反压、异常、收发守恒和停止原因，并覆盖Pool传输缩减、Concat目标通道偏移、Flatten跨块3/1/2/2元素分配、整数前缀非整除情形、旧JSON兼容及伪造投影描述。`tests/cpp/mapping_invariants_test.cpp` 检查真实分块映射与累加/传播依赖保留；`tests/python/test_workload.py` 从生成的 ONNX 输入执行完整接口路径。

```bash
scripts/test.sh workload_test
scripts/test.sh mapping_invariants_test
scripts/python.sh -m unittest discover -s tests/python -p 'test_workload.py' -v
```

# PIM-sim 开发工作事项

基于对代码框架、论文原理和当前实现状态的分析，整理以下工作事项。

## 重构

### 代码组织
- [x] **拆分 graph.cpp（~1400 行）**：按图层次分离为 `cgraph.cpp`、`tgraph.cpp`、`hgraph.cpp`、`dgraph.cpp`、`graph_io.cpp`，保留 `graph.h` 作为统一头文件
- [x] **策略模式重构**：策略文件按图层次拆分为 `CStrategy.cpp`、`TStrategy.cpp`、`HStrategy.cpp`、`DStrategy.cpp`
- [x] **Mapper 解耦**：将 `Mapper` 从 `HGraph` 中解耦为 `shared_ptr` 注入依赖，支持不同映射策略的插件化替换
- [x] **Scheduler 解耦**：将 `Scheduler` 从 `DGraph` 中解耦为 `shared_ptr` 注入依赖，支持 XY 路由、BCE 路由、自定义路由的插件化
- [x] **`.gitignore` 标准化**：保留白名单模式，仅追踪代码框架文件，移除 `libmain.so` 跟踪
- [x] **CMake 优化**：添加 `-Wall -Wextra -Wpedantic` 警告选项、区分 Debug/Release 构建、消除 CMP0148 警告

### 接口规范
- [x] **统一错误处理**：添加 `errors.h` 统一异常层次（PIMException/GraphError/MappingError/SchedulingError/ConfigError）
- [x] **日志系统**：C++ 侧添加结构化日志（`logger.h`，PIM_INFO/PIM_WARN/PIM_ERROR 宏），输出到 `runs/cpp_analysis.log`
- [x] **命名规范**：统一 snake_case（util.h/cpp 函数名、graph.h 枚举/结构体），修复拼写错误

## 功能完善

### 算子映射前端
- [ ] **补全 DHCG 分段策略**：当前仅实现 LS（逐层同步）和固定 pipeline_depth 分段。根据论文定义，应支持任意拓扑排序分段，包括 LP（多层并行）和自适应分段
- [ ] **Intensity Map 调参**：当前 α/β/γ/δ/ε/ζ 系数硬编码，需支持从 SimConfig.ini 读取或运行时校准
- [ ] **谱嵌入实现验证**：当前 `greedy_mapping()` 使用 BFS 贪心，论文 Algorithm 1 描述的是离散 2D 谱嵌入（拉普拉斯特征映射）。需对比实现或确认 BFS 为近似替代
- [ ] **权重复制建模**：当前 `create_dup_num()` 实现了基础复制因子计算，但缺少对应的 C-Edge 数据量调整和输出组路由逻辑
- [ ] **支持更多 DNN 算子**：当前仅支持 Conv/Gemm 的 MVM 分解。需扩展支持 Attention（Transformer）、Depthwise Conv、Element-wise 等

### 性能模拟后端
- [ ] **模拟器与映射前端集成**：当前 `src/simulator/` 和 `src/graph.cpp` 通过 TILE2_0 策略松散耦合。需实现完整的映射结果→模拟事件生成流水线（对应论文 5.3.2 节）
- [ ] **事件计数器自动化**：实现 Algorithm 5.1 的推理事件计数，自动从映射结果和架构参数生成事件队列
- [ ] **组合逻辑依赖检测**：自动检测前向组合逻辑依赖（零延迟功能函数），将其插入高优先级事件栈
- [ ] **时序依赖入队**：实现反馈时序依赖的数据队列化机制（论文 5.2.2 节）
- [ ] **多核事务机制完善**：当前 `MulticoreSimulator` 为硬编码 4 核测试。需实现通用的任务依赖原语和消息原语，支持任意核数的事务驱动模拟
- [ ] **反压机制建模**：实现数据生产者的阻塞/释放逻辑，以及通信网络缓存满时的反压传播

### Python 集成
- [ ] **pybind11 绑定扩展**：导出 Simulator 相关类（`Process`、`ModuleBase`、`Simulator`），支持 Python 侧构造和运行模拟
- [ ] **`perf.py` 重构**：1778 行单文件，职责混杂（分析、可视化、缓存、并行）。拆分为 `analysis.py`、`plotting.py`、`cache.py`
- [ ] **配置校验**：`SimConfig.ini` 缺少参数校验，添加 schema 验证和友好错误提示

## Bug 修复

### 图分析
- [ ] **`BaseGraph::get_adjacent_edges()`**：原 README 标注"目前它有问题"，需修复或移除
- [ ] **`build_depth_map()` 环检测**：当前仅通过拓扑排序检测环，但未处理自环和平行边
- [ ] **`CGraph::inter_layer_conn()` 通道交集计算**：当源/目标节点通道范围不连续时，交集计算可能遗漏或重复

### 映射与调度
- [ ] **`Mapper::bfs_heuristic_mapping()` 边界条件**：当网格已满或依赖节点无可用邻居时，回退策略不明确
- [ ] **`Scheduler::congestion_aware_routing()` 权重计算**：验证 C-Σ 公式 `Σ₁ⁿ⁻² Dᵢ + 2Dₙ₋₁` 与论文算法 2 的实现一致性
- [ ] **BCE 归一化**：`init_bce()` 中的 BCE 值按最大值归一化，但最大 BCE 值可能为 0（单路径场景），导致除零

### 模拟器
- [ ] **`Simulator::run()` 事件丢失**：当事件队列中同时存在信号事件和消息事件时，优先级排序可能错误
- [ ] **`Process::execute()` 状态机**：FINISHED 状态下重复触发可能导致状态不一致
- [ ] **`ModuleBase` 信号更新竞态**：拷贝消除策略下，同一时刻多模块写入同一信号变量可能导致数据覆盖

### 集成
- [x] **`perf.py` 缓存键冲突**：缓存文件名添加模型名哈希，支持旧格式兼容
- [x] **Booksim 进程泄漏**：`subprocess.run` 添加 `timeout=120` 和异常捕获，防止僵尸进程

## 工程化

- [x] **添加 CI**：GitHub Actions：`build.sh` + `test.sh`，Ubuntu 24.04 环境
- [x] **代码格式化**：添加 `.clang-format` 配置
- [ ] **测试覆盖**：当前仅 5 个 gtest 测试，需补充 CGraph/TGraph/HGraph/DGraph 单元测试、Mapper/Scheduler 单元测试、模拟器模块单元测试
- [x] **内存安全**：裸指针审查完成，已统一为智能指针（仅 pybind11/BGL 必须场景保留），启用 AddressSanitizer 编译选项（`cmake -DENABLE_ASAN=ON`）
- [x] **文档**：C++ 公共 API 添加 Doxygen 注释（analyzer.h, logger.h, errors.h, graph.h），Python 添加 docstring
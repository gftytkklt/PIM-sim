# PIMapping工程结构

算子映射前端代码框架，工程结构如下所示（随开发进度更新）：

```bash
.
├── include/                # 头文件
│   ├── graph.h             # 数据流图类声明
│   └── util.h              # 与数据流图无关的辅助函数声明
├── src/                    # 源文件
│   ├── graph.cpp           # 数据流图成员函数实现
│   ├── util.cpp            # 与数据流图无关的辅助函数
│   └── CMakeLists.txt      # CMake 配置文件
├── test/                   # 测试文件
│   ├── test1.cpp           # 测试基本成员函数的功能
│   ├── test2.cpp           # 测试uniformsplit
│   └── CMakeLists.txt      # CMake 配置文件
├── build.sh                # 测试脚本
├── CMakeLists.txt          # CMake 配置文件
├── README.md               # 项目说明文件
└── .gitignore              # Git 忽略文件
```

## 解析器输入

解析器基于python和onnx库，接收NN算法的标准onnx格式输入。功能是提取算法的MVM算子级别数据流图，其它算子对数据流的影响被合并在前级MVM节点中。算法的onnx表示被转换为后端可以分析的NNKernel数组，一个实例如下：
算法包括conv1-relu-avgpooling-conv2四层，onnx格式如下：

```json
{
  "ir_version": 7,
  "opset_import": [
    {
      "domain": "",
      "version": 11
    }
  ],
  "graph": {
    "name": "SequentialModel",
    "input": [
      {
        "name": "input",
        "type": {
          "tensor_type": {
            "elem_type": 1,  // FLOAT
            "shape": {
              "dim": [
                { "dim_value": 1 },
                { "dim_value": 3 },
                { "dim_value": 224 },
                { "dim_value": 224 }
              ]
            }
          }
        }
      }
    ],
    "output": [
      {
        "name": "output",
        "type": {
          "tensor_type": {
            "elem_type": 1,  // FLOAT
            "shape": {
              "dim": [
                { "dim_value": 1 },
                { "dim_value": 64 },
                { "dim_value": 112 },
                { "dim_value": 112 }
              ]
            }
          }
        }
      }
    ],
    "initializer": [
      // 在这里定义权重和偏置初始化（如果有）
    ],
    "node": [
      {
        "op_type": "Conv",
        "name": "Conv1",
        "input": ["input", "Conv1_W", "Conv1_B"],
        "output": ["Conv1_Output"],
        "attribute": [
          {
            "name": "kernel_shape",
            "ints": [3, 3]
          },
          {
            "name": "strides",
            "ints": [1, 1]
          },
          {
            "name": "pads",
            "ints": [1, 1, 1, 1]
          },
          // 其他Conv1的属性
        ]
      },
      {
        "op_type": "Relu",
        "name": "ReLU1",
        "input": ["Conv1_Output"],
        "output": ["ReLU1_Output"],
        "attribute": []
      },
      {
        "op_type": "AveragePool",
        "name": "AvgPool1",
        "input": ["ReLU1_Output"],
        "output": ["AvgPool1_Output"],
        "attribute": [
          {
            "name": "kernel_shape",
            "ints": [2, 2]
          },
          {
            "name": "strides",
            "ints": [2, 2]
          },
          // 其他AvgPool的属性
        ]
      },
      {
        "op_type": "Conv",
        "name": "Conv2",
        "input": ["AvgPool1_Output", "Conv2_W", "Conv2_B"],
        "output": ["Conv2_Output"],
        "attribute": [
          {
            "name": "kernel_shape",
            "ints": [3, 3]
          },
          {
            "name": "strides",
            "ints": [1, 1]
          },
          {
            "name": "pads",
            "ints": [1, 1, 1, 1]
          },
          // 其他Conv2的属性
        ]
      }
    ]
  }
}
```

数据流分析器只关心MVM算子的数据流图，因此该图被解析为以下格式的NNKernel数组：

```cpp
    std::vector<NNkernel> kernels = { 
    {0, {3,3}, {256,384}, std::vector<Depinfo>{{1,std::make_pair(1,384)}},{224, 224},{224, 224}}, 
    {1, {3,3}, {384,384}, std::vector<Depinfo>{{2,std::make_pair(1,384)}},{224, 224},{224, 224}},
    };
```

注意这里的变化：在ONNX表示中，conv1和conv2可能是layer/layer+3的层index关系，但在NNkernel数组中，它们的index为0和1，conv1的depinfo中，表示和conv2依赖关系的index应为conv2在NNkernel数组中的index1，而非原始的layer+3，该工作由前端解析器完成转换，以提高后端解析的效率。

相关结构体的介绍见下节。

## `Struct and enum class`

本节介绍graph.h中公共可见的结构体定义。

### `Struct Depinfo`

用于描述算子级别依赖关系的结构体，成员介绍如下：

- `int dep_layer`: 按NNKernel数组下标计算，该算子输出目的节点下标，从0开始计数。
- `std::pair<int,int> dep_chan`: 该目标节点依赖的输出通道范围，从1开始计数。

### `Struct NNkernel`

用于描述算法中MVM算子的数据依赖，成员介绍如下：

- `int layer`: 和该结构体在`std::vector<NNkernel> kernels`中的index一致，实际上可以忽略
- `std::pair<int,int> wsize`: 卷积核滑窗的w, h，或全连接层矩阵的w, h
- `std::pair<int,int> channel`: 卷积核输出输出通道个数
- `std::vector<Depinfo> depinfo`: 该算子目的节点的依赖关系数组，见`Struct Depinfo`
- `std::pair<int,int> ifmap_size, ofmap_size`: 输入、输出特征图大小(w, h)

### `enum class DepType`

用于描述边的数据依赖类型

- `ErrorType`: debug类型
- `Accum`: 层内部分和累加
- `Prop`: 层间fmap传递

### `Struct CNode`

C-VDFG的节点类型，用于描述crossbar-level的算子

- `int layer`: 和用于推导该节点的NNKernel一致，原因同上也可以忽略
- `int ofmap_size`: 节点输出特征图大小，这里是datavolume
- `std::pair<int,int> id_cin, id_cout`: 输入、输出通道范围

### `Struct CEdge`

C-VDFG的有向边类型，用于描述CNode的数据依赖关系

- `DepType c_type`: 有向边的数据依赖类型。
- `int datavolume`: 有向边传输的数据量。

## `class BaseGraph`

`BaseGraph` 是基于BGL (Boost Graph Library)提供的接口构造的图模板类，实现了通用的图操作封装。不同数据流图需要为该模板类提供权重结构体。

### 成员变量

该类是抽象模板类，没有成员变量，成员函数通过传入派生类的图引用进行图的基本操作。

### 模板别名

- `Graph`
BGL有向图别名。

- `UGraph`
BGL无向图别名。

- `BiGraph`
BGL双向图别名。

- `Node`
BGL节点描述符。

- `Edge`
BGL边描述符。

- `NodeProperty`
节点权重结构体。

- `EdgeProperty`
边权重结构体。

### 成员函数 (BaseGraph)

- `BaseGraph()`
构造函数，实际上没有做任何事。

- `void add_node(const NodeProperty& node_prop, Graph& g)`
封装boost的添加节点函数。

- `void add_edge(int v1, int v2, const EdgeProperty& edge_prop, Graph& g)`
封装boost的添加边函数。

- `void remove_node(int v, Graph& g)`
封装boost的移除节点函数，由于boost库只移除出边，因此额外添加了入边搜索（O(E)）。

- `void remove_edge(int v1, int v2, Graph& g)`
封装boost的移除边函数，使用节点index作为输入。

- `void remove_edge(const Edge& e, Graph& g)`
封装boost的移除边函数，使用边描述符作为输入。

- `virtual const Graph& get_graph() const`
纯虚函数，派生类必须根据自己持有的图对象重写该函数。

- `const NodeProperty& get_node_property(int v, const Graph& g) const`
NodeProperty接口函数，返回结构体引用。

- `const EdgeProperty& get_edge_property(int v1, int v2, const Graph& g) const`
EdgeProperty接口函数，返回结构体引用，使用节点index作为输入。

- `const EdgeProperty& get_edge_property(const Edge& e, const Graph& g) const`
EdgeProperty接口函数，返回结构体引用，使用边描述符作为输入。

- `void set_node_property(int v, const NodeProperty& node_prop, Graph& g)`
设置节点NodeProperty。

- `void set_edge_property(int v1, int v2, const EdgeProperty& edge_prop, Graph& g)`
设置边EdgeProperty，使用节点index作为输入。

- `void set_edge_property(const Edge& e, const EdgeProperty& edge_prop, Graph& g)`
设置边EdgeProperty，使用边描述符作为输入。

- `std::vector<int> get_adjacent_nodes(int v, const Graph& g) const`
获取所有该节点出边指向的节点index列表。

- `std::vector<Edge> get_adjacent_edges(int v, const Graph& g) const`
获取所有出边的描述符，目前它有问题。

- `virtual void analysis()`
纯虚函数，派生类必须根据自己的数据流分析任务重写该函数。

- `virtual void print_graph_info(const Graph& cg) const`
虚函数，打印图信息，可以被派生类重写。

## `Class CGraph`

通过`<CNode, CEdge>`实例化模板基类的派生类，描述C-VDFG。

### 成员变量、结构体

- `struct AccBlk`
维护部分和累加关系，包括涉及的节点和输出通道范围。

- `struct CDep`
维护层间数据依赖关系，包括部分和块、index（同上原因可忽略）、由kernel推导得来的依赖关系。

- `const std::vector<NNkernel>& kernels`
NN算子数组，元素需要满足拓扑序，且各元素dep相关元素符合相关约束。

- `Graph cg`
通过`<CNode, CEdge>`实例化的有向图类别。

- `std::pair<int, int> CNode_size`
crossbar大小、PE映射策略等约束节点大小的尺寸参数。

- `std::vector<CDep> dep_infos`
层间依赖关系数组，每个元素代表一个算子的依赖关系。

### 成员函数 (CGraph)

- `CGraph(const std::vector<NNkernel>& kernels, std::pair<int, int> CNode_size)`
构造函数。

- `void analysis() final`
基于NNkernel生成cg有向图。

- `const Graph& get_graph() const`
得到只读cg引用。

- `Graph& get_graph()`
得到可修改的cg引用。

- `void print_graph_info() const`
打印该层级数据流信息。

- `void debug()`
用于调用基类中protect类型函数的调试接口，方便单独测试函数功能且不改变封装。

- `void create_cnodes()`
拆分算子为CNode。

- `void conn_accblk()`
构造层内数据依赖。

- `void inter_layer_conn()`
构造层间数据依赖。

# Python-C++ Integration for Model Conversion and Analysis

## 简介
本模块将所有的 C++ 源文件打包为共享库，允许通过 Python 调用 C++ 的 `main.cpp` 中的 `test` 函数，并进行后续的测试。提供了一键编译脚本和两个主要的 Python 命令，用于模型转换和分析。

---

## 功能概览
1. **C++模块打包**  
   通过根目录下的 `CMakeLists.txt`，将所有的 C++ 源文件打包生成共享库 `libmain.so`。
   
2. **Python 调用 C++**  
   Python 可以直接调用 `main.cpp` 中的 `test` 函数，用于快速测试功能。

3. **模型转换与分析工具**  
   - `torch2onnx.py`：将 PyTorch 模型转换为 ONNX 格式，并解析模型。
   - `onnx_analysis.py`：对指定的 ONNX 模型进行分析。

---

## 使用方法

### 1. 编译共享库
运行根目录下的 `build.sh` 脚本编译 `libmain.so`。  
**注意：** 如果需要修改编译设置，请取消 `build.sh` 中的注释。

```bash
./build.sh

```

### 2. Python 脚本
提供两个主要的 Python 命令：

#### 2.1 `torch2onnx.py`
初始化参数，将 PyTorch 模型转换为 ONNX 格式。 

```bash
python3 torch2onnx.py
```

- 默认情况下提供`mccnn`模型的转换作为示例，如果需要转换其他模型，可以修改 `torch2onnx.py` 文件中的相应代码，指定模型的路径及转换参数。


#### 2.2 `onnx_analysis.py`
分析 ONNX 模型，并输出一些有用的信息。你可以通过以下命令运行：

```bash
python3 onnx_analysis.py [onnx_model_path]
```

- 如果未指定 `onnx_model_path`，默认会加载 `models` 文件夹中的 `resnet18` 模型。

### 3. 测试功能
`main.cpp` 中提供了一个 `test` 函数，可以通过 Python 调用进行测试。运行 Python 脚本时会自动调用该函数。

### 4. 注意事项
- 在运行 `torch2onnx.py` 时，确保你已经安装了 PyTorch 和 ONNX 的相关依赖。
- 如果转换其他模型，记得在 `torch2onnx.py` 中修改模型加载部分的代码。
- `onnx_analysis.py` 需要提供 ONNX 模型的路径，否则会默认加载 `models/resnet18`。

## 环境依赖

- Python 3.x
- PyTorch
- ONNX
- pybind11
- CMake
- Ubuntu 24 LTS 或其他支持的操作系统

# PIMapping工程结构
算子映射前端代码框架，工程结构如下所示（随开发进度更新）
```bash
.
├── src/                    # 源文件
│   ├── graph.cpp           # 目前只有<<操作符重载
│   └── CMakeLists.txt      # CMake 配置文件
├── include/                # 头文件
│   └── graph.h             # 数据流图类声明
├── test/                   # 测试文件
│   ├── test1.cpp           # 测试基本成员函数的功能
│   └── CMakeLists.txt      # CMake 配置文件
├── build.sh                # gpt生成的模板
├── CMakeLists.txt          # CMake 配置文件
├── README.md               # 项目说明文件
├── tepmlate.md             # gpt生成的模板
└── .gitignore              # Git 忽略文件
```
结合论文内容，相关数据结构如下所示：
## `class BaseGraph`

### 类介绍
`BaseGraph` 是基于BGL (Boost Graph Library)提供的接口构造的图模板类，实现了通用的图操作封装。不同数据流图需要为该模板类提供权重结构体

### 成员变量
该类是抽象模板类，没有成员变量，成员函数通过传入派生类的图引用进行图的基本操作。

### 模板别名
- `Graph`: BGL有向图别名
- `UGraph`: BGL无向图别名
- `BiGraph`: BGL双向图别名
- `Node`: BGL节点描述符
- `Edge`: BGL边描述符
- `NodeProperty`: 节点权重结构体
- `EdgeProperty`: 边权重结构体

### 方法

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
设置节点NodeProperty

- `void set_edge_property(int v1, int v2, const EdgeProperty& edge_prop, Graph& g)`
设置边EdgeProperty，使用节点index作为输入。

- `void set_edge_property(const Edge& e, const EdgeProperty& edge_prop, Graph& g)`
设置边EdgeProperty，使用边描述符作为输入。

- `std::vector<int> get_adjacent_nodes(int v, const Graph& g) const`
获取所有该节点出边指向的节点index列表

- `std::vector<Edge> get_adjacent_edges(int v, const Graph& g) const`
获取所有出边的描述符，目前它有问题。

- `virtual void analysis()`
纯虚函数，派生类必须根据自己的数据流分析任务重写该函数。

- `virtual void print_graph_info(const Graph& cg) const`
虚函数，打印图信息，可以被派生类重写。


# Reaemd模板
提供可以复制粘贴的模板
```bash
.
├── ref code/               # 之前工程的参考代码（避免打乱框架代码结构）
├── src/
│   ├── main.cpp            # 程序入口
│   ├── core/               # 核心模块
│   │   ├── core.cpp
│   │   └── core.h
│   ├── utils/              # 工具模块
│   │   ├── utils.cpp
│   │   └── utils.h
│   └── network/            # 网络模块
│       ├── network.cpp
│       └── network.h
├── include/                # 公共头文件目录
│   ├── core/               # 核心模块头文件
│   │   └── core.h
│   ├── utils/              # 工具模块头文件
│   │   └── utils.h
│   └── network/            # 网络模块头文件
│       └── network.h
├── build/                  # 构建输出目录
├── tests/                  # 测试文件
│   ├── test_core.cpp
│   ├── test_utils.cpp
│   └── test_network.cpp
├── CMakeLists.txt          # CMake 配置文件
├── README.md               # 项目说明文件
└── .gitignore              # Git 忽略文件
```
## 类名：`MyClass`

### 类介绍
`MyClass` 是一个示例类，展示如何创建并管理类的成员变量和方法。它实现了一个基本的接口，可以存储和操作数据。该类包含了一个整数成员、一个字符串成员，以及一个整数向量成员。

### 成员变量
- `int data`：存储整数数据，表示类的某个关键数值。
- `std::string name`：存储类实例的名称，用于标识对象。
- `std::vector<int> values`：存储一系列整数值，用于存放额外的数据。

### 方法

#### `MyClass(int data, const std::string& name)`
构造函数，初始化类实例的成员变量。

#### `addNode()`

该成员函数用于向图中添加一个新节点。

##### **参数**
- **node_id (int)**: 新节点的唯一标识符。
- **node_data (NodeData)**: 与节点关联的数据对象。

##### **返回值**
- **bool**: 如果节点成功添加到图中，返回 `true`；否则返回 `false`。

##### **示例**
```cpp
bool Graph::addNode(int node_id, NodeData node_data) {
    if (nodes.find(node_id) != nodes.end()) {
        // 节点ID已存在
        return false;
    }
    // 向图中添加新节点
    nodes[node_id] = node_data;
    return true;
}
```
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
## 类名：`BaseGraph`

### 类介绍
`BaseGraph` 是基于BGL（Boost Graph Library）提供的接口构造的图模板类。不同数据流图需要为该模板类提供权重结构体

### 成员变量
- `int data`：存储整数数据，表示类的某个关键数值。
- `std::string name`：存储类实例的名称，用于标识对象。
- `std::vector<int> values`：存储一系列整数值，用于存放额外的数据。

### 方法

#### `MyClass(int data, const std::string& name)`
构造函数，初始化类实例的成员变量。

```cpp
MyClass::MyClass(int data, const std::string& name)
    : data(data), name(name) {}
```
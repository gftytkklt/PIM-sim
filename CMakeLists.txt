cmake_minimum_required(VERSION 3.10)

# 设置项目名称
project(PIMapping)

# 设置 C++ 标准
set(CMAKE_CXX_STANDARD 20)

# 设置源代码和头文件目录
add_subdirectory(src)
add_subdirectory(test)

# 启用测试
enable_testing()

cmake_minimum_required(VERSION 3.10)

# project name
project(PIMapping)

# C++ standard
set(CMAKE_CXX_STANDARD 20)

# enable testing
enable_testing()

# src and test directories
add_subdirectory(src)
add_subdirectory(test)

# 兼容性策略设置
# cmake_policy(SET CMP0148 OLD)
# 查找pybind11包
find_package(pybind11 REQUIRED)

# 设置 include 目录
include_directories(${PROJECT_SOURCE_DIR}/include)
include_directories(${PROJECT_SOURCE_DIR}/src)

# 定义库的输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../)

# 查找源文件和头文件
file(GLOB_RECURSE SOURCES1 "src/*.cpp")
file(GLOB_RECURSE SOURCES2 "include/*.h")
# 编译库并生成Python模块
add_library(main MODULE main.cpp ${SOURCES1} ${SOURCES2})
# 链接pybind11库
target_link_libraries(main PRIVATE pybind11::module)
cmake_minimum_required(VERSION 3.10)

# project name
project(PIMapping)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建类型默认值
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# 编译警告选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-reorder -Wno-unused-parameter")

# 定义库的输出目录
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../)

# pybind11 兼容性
cmake_policy(SET CMP0148 NEW)
find_package(pybind11 REQUIRED)

# enable testing
enable_testing()

# src and test directories
add_subdirectory(src)
add_subdirectory(test)

add_library(PIModule MODULE main.cpp)

set_target_properties(PIModule PROPERTIES
    OUTPUT_NAME "pimapping"
    PREFIX ""
    SUFFIX ${PYTHON_MODULE_EXTENSION}
)

target_link_libraries(PIModule PRIVATE
    PIMapping
    pybind11::module
)

target_include_directories(PIModule PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)
cmake_minimum_required(VERSION 3.28)

# project name
project(PIMapping)

# C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 构建类型默认值
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

# Sanitizers are also useful in optimized builds; keep them in separate build dirs.
option(ENABLE_ASAN "Enable AddressSanitizer" OFF)
option(ENABLE_UBSAN "Enable UndefinedBehaviorSanitizer" OFF)
if(ENABLE_ASAN)
    add_compile_options(-fsanitize=address -fno-omit-frame-pointer)
    add_link_options(-fsanitize=address)
endif()
if(ENABLE_UBSAN)
    add_compile_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
    add_link_options(-fsanitize=undefined -fno-sanitize-recover=undefined)
endif()

# 编译警告选项
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -Wpedantic")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wno-sign-compare -Wno-reorder -Wno-unused-parameter")

# 定义库的输出目录
set(PIMAPPING_PYTHON_OUTPUT_DIR "${CMAKE_BINARY_DIR}/python" CACHE PATH
    "Directory for the pimapping Python extension")

# pybind11 兼容性
if(POLICY CMP0148)
    cmake_policy(SET CMP0148 NEW)
endif()
set(PYBIND11_FINDPYTHON ON)
find_package(Python3 REQUIRED COMPONENTS Interpreter Development.Module)
find_package(pybind11 REQUIRED)

# enable testing
enable_testing()

# src and test directories
add_subdirectory(src/cpp)
add_subdirectory(tests/cpp)

pybind11_add_module(PIModule src/bindings/python.cpp)

set_target_properties(PIModule PROPERTIES
    OUTPUT_NAME "pimapping"
    LIBRARY_OUTPUT_DIRECTORY "${PIMAPPING_PYTHON_OUTPUT_DIR}"
)

target_link_libraries(PIModule PRIVATE
    PIMapping
    pybind11::module
)

target_include_directories(PIModule PRIVATE
    ${CMAKE_SOURCE_DIR}/include
)

#!/bin/bash

# 1. 判断是否需要清理构建目录
if [ "$1" == "clean" ]; then
  echo "Cleaning the build directory..."
  rm -rf build/*
  exit 0  # 执行完清理操作后退出
fi

# 2. 创建 build 目录（如果不存在）
if [ ! -d "build" ]; then
  mkdir build
fi

# 3. 进入 build 目录
cd build

# 4. 运行 CMake 配置
echo "Running CMake..."
cmake ..

# 5. 执行构建
echo "Building the project..."
make

# 6. 判断是否传入了测试参数
if [ -z "$1" ]; then
  # 如果没有提供参数，执行回归测试
  echo "No test specified. Running all tests (regression tests)..."
  ctest -N
  # ctest
else
  # 如果提供了测试名称，则只运行指定的测试
  echo "Running specific test: $1"
  # ctest -N -R "$1"
  ctest -R "$1" -V
fi

#!/bin/bash
# ===============================
# 自动加载环境脚本
# ===============================

# 加载 GCC 编译器
module load gcc/11.4.0

# 加载 CMake
module load cmake/3.28.0

# 加载 Python
module load python/3.11.9

# 加载 Boost 库
module load boost/1.84.0

# 加载 Google Test
module load gtest/1.14.0

# 加载 pybind11
module load pybind11/2.13.0

# ===============================
# 环境验证
# ===============================
echo "=== Loaded Modules ==="
module list

echo "=== Versions Check ==="
gcc --version | head -n1
cmake --version | head -n1
python3 --version

# Python 库检查
python3 -c "import torch; print('PyTorch:', torch.__version__)"
python3 -c "import numpy, onnx, matplotlib; print('Other libs: OK')"

echo "=== Environment Ready ==="
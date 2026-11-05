#!/usr/bin/env bash
# 仅用于组内module服务器，可从任意目录 source；本地依赖环境见仓库 docs/guides/usage.md。
if ! type module >/dev/null 2>&1; then
    echo '当前环境没有module命令；请使用本机依赖/虚拟环境，参见docs/guides/usage.md。' >&2
    return 1 2>/dev/null || exit 1
fi
for pim_module in gcc/11.4.0 cmake/3.28.0 python/3.11.9 boost/1.84.0 gtest/1.14.0 pybind11/2.13.0; do
    if ! module load "$pim_module"; then
        echo "加载依赖失败：$pim_module" >&2
        unset pim_module
        return 1 2>/dev/null || exit 1
    fi
done
unset pim_module
module list
if ! python3 -c 'import numpy, onnx, matplotlib, pandas; print("Python analysis dependencies: OK")'; then
    return 1 2>/dev/null || exit 1
fi
gcc --version | head -n1
cmake --version | head -n1
python3 --version

#!/bin/bash

# 1. force clean
echo "Cleaning the build directory..."
rm -rf ./build
mkdir build

# 2. compilation processes in ./build
cd build
echo "Running CMake..."
cmake ..
echo "Building the project..."
make


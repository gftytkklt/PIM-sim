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

# 3. test processes
if [ -z "$1" ]; then
  # default: run all tests
  echo "No test specified. Running all tests (regression tests)..."
  ctest -N
  # ctest
else
  # run specific test
  echo "Running specific test: $1"
  # ctest -N -R "$1"
  ctest -R "$1" -V # verbose for testinfo display
fi

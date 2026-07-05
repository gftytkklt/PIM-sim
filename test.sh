#!/bin/bash

#1. exec build.sh
./build.sh

cd build

#2. run test
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

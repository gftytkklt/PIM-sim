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
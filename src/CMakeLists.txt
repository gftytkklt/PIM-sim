# 自动收集 src 目录及其子目录下的所有 .cpp 文件
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# 自动收集 include 目录下的所有 .h 文件
file(GLOB_RECURSE HEADERS "${CMAKE_SOURCE_DIR}/include/*.h")

# 查找 Boost，并要求必须找到 graph 组件
find_package(Boost REQUIRED COMPONENTS graph)

# 创建静态库 PIMapping
add_library(PIMapping STATIC ${SOURCES})

# # 创建可执行文件 PIMapping
# add_executable(PIMapping ${SOURCES})

# 如果有需要链接的库，可以在这里进行链接
target_link_libraries(PIMapping PRIVATE Boost::graph)

# 设置 my_program 的包含目录，指向 include 目录
target_include_directories(PIMapping PUBLIC ${CMAKE_SOURCE_DIR}/include)
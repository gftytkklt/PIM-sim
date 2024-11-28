# 自动收集 test 目录下的所有 .cpp 文件
file(GLOB_RECURSE TEST_SOURCES "test/*.cpp")

# 对每个测试文件，创建一个可执行文件并注册为测试
foreach(test_file ${TEST_SOURCES})
    # 获取不包含路径和扩展名的文件名
    get_filename_component(test_name ${test_file} NAME_WE)

    # 创建每个测试文件的可执行文件
    add_executable(${test_name} ${test_file})

    # 将 src 中的目标（例如 my_program）链接到测试目标
    target_link_libraries(${test_name} PRIVATE PIMapping)

    # 注册为 CTest 测试
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
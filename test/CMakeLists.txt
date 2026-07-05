# all test files
file(GLOB_RECURSE TEST_SOURCES "${CMAKE_SOURCE_DIR}/test/*.cpp")

#find googletest
find_package(GTest REQUIRED)

# create a test target for each test file
foreach(test_file ${TEST_SOURCES})
    # get the name of the test file
    get_filename_component(test_name ${test_file} NAME_WE)

    # create an executable target for the test
    add_executable(${test_name} ${test_file})

    # link the test target with the library
    target_link_libraries(${test_name} PRIVATE PIMapping GTest::GTest GTest::Main pthread)

    # add the test to the test suite
    add_test(NAME ${test_name} COMMAND ${test_name})
endforeach()
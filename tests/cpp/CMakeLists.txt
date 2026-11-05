# all test files
file(GLOB_RECURSE TEST_SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")
get_filename_component(PIMAPPING_BUILD_NAME "${CMAKE_BINARY_DIR}" NAME)
set(PIMAPPING_TEST_OUTPUT_DIR "${CMAKE_SOURCE_DIR}/artifacts/tests/${PIMAPPING_BUILD_NAME}"
    CACHE PATH "Root directory for isolated CTest artifacts")

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
    set(test_output_dir "${PIMAPPING_TEST_OUTPUT_DIR}/${test_name}")
    file(MAKE_DIRECTORY "${test_output_dir}")
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "${test_output_dir}"
        ENVIRONMENT "PIM_ARTIFACTS_DIR=${test_output_dir}")
endforeach()

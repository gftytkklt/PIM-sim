# Bindings live outside this subtree and must not enter the core static library.
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp")

find_package(Boost REQUIRED COMPONENTS graph json)

add_library(PIMapping STATIC ${SOURCES})

target_compile_options(PIMapping PRIVATE -fPIC)
target_compile_definitions(PIMapping PUBLIC
    PIM_SIM_DEFAULT_ARTIFACTS_DIR="${CMAKE_SOURCE_DIR}/artifacts")

target_link_libraries(PIMapping PRIVATE Boost::graph Boost::json)

target_include_directories(PIMapping PUBLIC ${CMAKE_SOURCE_DIR}/include)

file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

set(Boost_NO_BOOST_CMAKE ON)
find_package(Boost REQUIRED COMPONENTS graph)

add_library(PIMapping STATIC ${SOURCES})

target_compile_options(PIMapping PRIVATE -fPIC)

target_link_libraries(PIMapping PRIVATE Boost::graph)

target_include_directories(PIMapping PUBLIC ${CMAKE_SOURCE_DIR}/include)
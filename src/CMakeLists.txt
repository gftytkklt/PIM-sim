file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

find_package(Boost REQUIRED COMPONENTS graph json)

add_library(PIMapping STATIC ${SOURCES})

target_compile_options(PIMapping PRIVATE -fPIC)

target_link_libraries(PIMapping PRIVATE Boost::graph Boost::json)

target_include_directories(PIMapping PUBLIC ${CMAKE_SOURCE_DIR}/include)
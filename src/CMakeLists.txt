# collect all source files
file(GLOB_RECURSE SOURCES "${CMAKE_SOURCE_DIR}/src/*.cpp")

# collect all header files
file(GLOB_RECURSE HEADERS "${CMAKE_SOURCE_DIR}/include/*.h")

# find Boost
find_package(Boost REQUIRED COMPONENTS graph)

# create a library named PIMapping
add_library(PIMapping SHARED ${SOURCES})

# use add_executable to create an executable
# add_executable(PIMapping ${SOURCES})

# link the Boost::graph library to my_program
target_link_libraries(PIMapping PRIVATE Boost::graph)

# add the include directory to the target
target_include_directories(PIMapping PUBLIC ${CMAKE_SOURCE_DIR}/include)
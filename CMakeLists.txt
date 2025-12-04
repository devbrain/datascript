cmake_minimum_required(VERSION 3.20)
project(DataScript LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)

# Build options
option(DATASCRIPT_BUILD_TESTS "Build unit tests" OFF)

add_subdirectory(lib)
add_subdirectory(ds)

if(DATASCRIPT_BUILD_TESTS)
    add_subdirectory(unittest)
endif()




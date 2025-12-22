cmake_minimum_required(VERSION 3.20)
project(DataScript LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Build options
option(DATASCRIPT_BUILD_TESTS "Build unit tests" ON)

# Include compiler warning configuration
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/CompilerWarnings.cmake)

add_subdirectory(lib)
add_subdirectory(ds)

if(DATASCRIPT_BUILD_TESTS)
    add_subdirectory(unittest)
endif()

# Include CMake module for users who include DataScript via FetchContent
# This makes datascript_generate() available to parent projects
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DataScriptGenerate.cmake)




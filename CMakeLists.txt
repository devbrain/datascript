# =============================================================================
# DataScript - Binary Format Compiler
# =============================================================================

cmake_minimum_required(VERSION 3.20)

project(datascript
    VERSION 1.0.0
    DESCRIPTION "Modern C++ binary format compiler"
    LANGUAGES C CXX
)

# =============================================================================
# Neutrino CMake Integration
# =============================================================================

include(FetchContent)

if(NOT DEFINED NEUTRINO_CMAKE_DIR)
    FetchContent_Declare(
        neutrino_cmake
        GIT_REPOSITORY https://github.com/devbrain/neutrino-cmake.git
        GIT_TAG master
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(neutrino_cmake)
    set(NEUTRINO_CMAKE_DIR "${neutrino_cmake_SOURCE_DIR}/cmake")
endif()

list(APPEND CMAKE_MODULE_PATH "${NEUTRINO_CMAKE_DIR}")
include(NeutrinoInit)

# =============================================================================
# Options
# =============================================================================

neutrino_define_library_options(datascript)

# Host tools option - the 'ds' code generator runs on the build machine
neutrino_is_top_level(_datascript_is_top_level)
if(_datascript_is_top_level)
    option(NEUTRINO_DATASCRIPT_BUILD_HOST_TOOLS "Build the ds code generator" ON)
else()
    option(NEUTRINO_DATASCRIPT_BUILD_HOST_TOOLS "Build the ds code generator" OFF)
endif()

# =============================================================================
# C++ Standard
# =============================================================================

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)


# =============================================================================
# Library
# =============================================================================

add_subdirectory(lib)

# =============================================================================
# Host Tools (Code Generator)
# =============================================================================

# When cross-compiling (e.g., Emscripten), the ds tool must be built for the host
# Users need to either:
# 1. Build ds natively first and set DS_EXECUTABLE
# 2. Have ds installed in PATH
if(NEUTRINO_DATASCRIPT_BUILD_HOST_TOOLS)
    if(CMAKE_CROSSCOMPILING)
        # When cross-compiling, try to find a host-built ds executable
        find_program(DS_EXECUTABLE ds
            HINTS ${DS_HOST_BUILD_DIR}
            DOC "Path to host-built ds code generator"
        )
        if(DS_EXECUTABLE)
            message(STATUS "Using host ds: ${DS_EXECUTABLE}")
            # Create imported target for the host ds
            add_executable(ds IMPORTED GLOBAL)
            set_target_properties(ds PROPERTIES IMPORTED_LOCATION "${DS_EXECUTABLE}")
        else()
            message(WARNING
                "Cross-compiling but no host 'ds' executable found.\n"
                "Either:\n"
                "  1. Build ds natively first: cmake -B build-host && cmake --build build-host --target ds\n"
                "  2. Set DS_EXECUTABLE to the path of the host-built ds\n"
                "  3. Install ds to PATH\n"
                "Code generation (datascript_generate) will not work without ds."
            )
        endif()
    else()
        # Native build - build the ds tool
        add_subdirectory(ds)
    endif()
endif()

# =============================================================================
# Tests
# =============================================================================

if(NEUTRINO_DATASCRIPT_BUILD_TESTS)
    enable_testing()
    add_subdirectory(test)
endif()

# =============================================================================
# Installation
# =============================================================================

if(NEUTRINO_DATASCRIPT_INSTALL)
    include(GNUInstallDirs)
    include(CMakePackageConfigHelpers)

    # Install library
    install(TARGETS datascript
        EXPORT datascriptTargets
        LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
        ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
        RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    )

    # Install headers
    install(DIRECTORY lib/include/datascript
        DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
    )

    # Install ds executable (if built)
    if(TARGET ds AND NOT CMAKE_CROSSCOMPILING)
        install(TARGETS ds
            RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
        )
    endif()

    # Install CMake config files
    install(EXPORT datascriptTargets
        FILE datascriptTargets.cmake
        NAMESPACE neutrino::
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/datascript
    )

    configure_package_config_file(
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/datascriptConfig.cmake.in
        ${CMAKE_CURRENT_BINARY_DIR}/datascriptConfig.cmake
        INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/datascript
    )

    write_basic_package_version_file(
        ${CMAKE_CURRENT_BINARY_DIR}/datascriptConfigVersion.cmake
        VERSION ${PROJECT_VERSION}
        COMPATIBILITY SameMajorVersion
    )

    install(FILES
        ${CMAKE_CURRENT_BINARY_DIR}/datascriptConfig.cmake
        ${CMAKE_CURRENT_BINARY_DIR}/datascriptConfigVersion.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/datascript
    )

    # Install the DataScriptGenerate.cmake module for users
    install(FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/cmake/DataScriptGenerate.cmake
        DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/datascript
    )
endif()

# =============================================================================
# CMake Module for FetchContent Users
# =============================================================================

# Make datascript_generate() available to parent projects
include(${CMAKE_CURRENT_SOURCE_DIR}/cmake/DataScriptGenerate.cmake)

# =============================================================================
# Summary
# =============================================================================

neutrino_is_top_level(_datascript_is_top_level)
if(_datascript_is_top_level)
    neutrino_print_options(datascript)
    message(STATUS "")
    message(STATUS "datascript ${PROJECT_VERSION} Configuration:")
    message(STATUS "  C++ Standard:     20")
    message(STATUS "  Library type:     ${NEUTRINO_DATASCRIPT_BUILD_SHARED}")
    message(STATUS "  Build tests:      ${NEUTRINO_DATASCRIPT_BUILD_TESTS}")
    message(STATUS "  Build host tools: ${NEUTRINO_DATASCRIPT_BUILD_HOST_TOOLS}")
    message(STATUS "  Cross-compiling:  ${CMAKE_CROSSCOMPILING}")
    message(STATUS "  Install:          ${NEUTRINO_DATASCRIPT_INSTALL}")
    message(STATUS "")
endif()

# =============================================================================
# DataScript Library
# =============================================================================

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Allow FetchContent_Populate for ExternalProject_Add pattern (CMP0169)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# =============================================================================
# External Tool: re2c Scanner Generator
# =============================================================================

find_program(RE2C_EXECUTABLE re2c)

if(NOT RE2C_EXECUTABLE)
    message(STATUS "re2c not found, will build from source")

    FetchContent_Declare(
        re2c
        GIT_REPOSITORY "https://github.com/skvadrik/re2c.git"
        GIT_TAG "3.1"
    )

    FetchContent_GetProperties(re2c)
    if(NOT re2c_POPULATED)
        FetchContent_Populate(re2c)
    endif()

    # Build re2c as external project
    if(MSVC)
        set(RE2C_WARNING_FLAGS "/W0")
    else()
        set(RE2C_WARNING_FLAGS "-w")
    endif()

    include(ExternalProject)
    ExternalProject_Add(
        re2c_build
        SOURCE_DIR ${re2c_SOURCE_DIR}
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DCMAKE_C_FLAGS=${RE2C_WARNING_FLAGS}
            -DCMAKE_CXX_FLAGS=${RE2C_WARNING_FLAGS}
            -DRE2C_BUILD_RE2GO=OFF
            -DRE2C_BUILD_RE2RUST=OFF
            -DRE2C_BUILD_LIBS=OFF
            -DRE2C_BUILD_BENCHMARKS=OFF
            -DRE2C_REBUILD_DOCS=OFF
        BUILD_COMMAND ${CMAKE_COMMAND} --build <BINARY_DIR> --target re2c
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS <BINARY_DIR>/re2c
    )
    ExternalProject_Get_Property(re2c_build BINARY_DIR)
    set(RE2C_EXECUTABLE ${BINARY_DIR}/re2c)
    set(RE2C_TARGET re2c_build)
else()
    message(STATUS "Found re2c: ${RE2C_EXECUTABLE}")
endif()

# =============================================================================
# Build Lemon Parser Generator (Bundled)
# =============================================================================

set(LEMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lemon)
set(LEMPAR_FILE ${LEMON_DIR}/lempar.c)
set(LEMON_OUT "${CMAKE_CURRENT_BINARY_DIR}/gen")

add_executable(lemon ${LEMON_DIR}/lemon.c)
# Disable all warnings for third-party lemon tool
if(MSVC)
    target_compile_options(lemon PRIVATE /W0)
else()
    target_compile_options(lemon PRIVATE -w)
endif()

file(MAKE_DIRECTORY ${LEMON_OUT})

# =============================================================================
# Generate Parser
# =============================================================================

set(PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_parser.y)
set(PARSER_C ${LEMON_OUT}/datascript_parser.c)
set(PARSER_H ${LEMON_OUT}/datascript_parser.h)

add_custom_command(
    OUTPUT ${PARSER_C} ${PARSER_H}
    COMMAND $<TARGET_FILE:lemon> -T${LEMPAR_FILE} -d${LEMON_OUT} ${PARSER_Y} || true
    DEPENDS ${PARSER_Y} lemon ${LEMPAR_FILE}
    COMMENT "Generating parser with lemon"
)

# =============================================================================
# Generate Scanner
# =============================================================================

set(SCANNER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_scanner.re)
set(SCANNER_C ${LEMON_OUT}/datascript_scanner.c)

add_custom_command(
    OUTPUT ${SCANNER_C}
    COMMAND ${RE2C_EXECUTABLE} -o ${SCANNER_C} ${SCANNER_RE}
    DEPENDS ${SCANNER_RE} ${RE2C_TARGET} ${PARSER_H}
    COMMENT "Generating scanner with re2c"
)

# =============================================================================
# Library Sources
# =============================================================================

set(DATASCRIPT_SOURCES
    # Generated files
    ${SCANNER_C}
    ${PARSER_C}

    # Parser
    src/parser/ast_builder.cc
    src/parser/parser_context.cc
    src/parser/parser.cc
    src/parser/module_loader.cc

    # Semantic analysis
    src/semantic/diagnostics.cc
    src/semantic/symbol_table.cc
    src/semantic/phase0_desugar.cc
    src/semantic/phase1_symbols.cc
    src/semantic/phase2_name_resolution.cc
    src/semantic/phase3_type_checking.cc
    src/semantic/phase4_constant_evaluation.cc
    src/semantic/phase5_size_calculation.cc
    src/semantic/phase6_constraint_validation.cc
    src/semantic/phase7_reachability.cc
    src/semantic/analyze.cc

    # IR
    src/ir/ir_builder.cc

    # Code Generation
    src/codegen/base_renderer.cc
    src/codegen/command_builder.cc
    src/codegen/code_writer.cc
    src/codegen/cpp/cpp_code_writer.cc
    src/codegen/cpp/cpp_writer_context.cc
    src/codegen/cpp/cpp_helper_generator.cc
    src/codegen/cpp/cpp_expression_renderer.cc
    src/codegen/cpp/cpp_library_mode.cc
    src/codegen/cpp/cpp_renderer.cc
    src/codegen/cpp/cpp_renderer_plugin.cc
    src/codegen/datascript/datascript_renderer.cc
    src/codegen/renderer_registry.cc

    # Kaitai Struct support
    src/ksy/ksy_to_ir_builder.cc
)

# =============================================================================
# Library Target
# =============================================================================

if(NEUTRINO_DATASCRIPT_BUILD_SHARED)
    add_library(datascript SHARED ${DATASCRIPT_SOURCES})
else()
    add_library(datascript STATIC ${DATASCRIPT_SOURCES})
endif()

# Create neutrino:: alias
add_library(neutrino::datascript ALIAS datascript)

# =============================================================================
# Target Configuration
# =============================================================================

target_compile_features(datascript PUBLIC cxx_std_20)

target_include_directories(datascript
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/src
        ${CMAKE_CURRENT_BINARY_DIR}
)

# External dependencies as SYSTEM to suppress warnings
target_include_directories(datascript SYSTEM
    PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# =============================================================================
# Compiler Warnings
# =============================================================================

# Apply strict warnings to the library
neutrino_target_warnings(datascript)

# Disable warnings for generated C files (re2c scanner, lemon parser)
if(MSVC)
    set_source_files_properties(${SCANNER_C} ${PARSER_C} PROPERTIES COMPILE_FLAGS "/W0")
else()
    set_source_files_properties(${SCANNER_C} ${PARSER_C} PROPERTIES COMPILE_FLAGS "-w")
endif()

# =============================================================================
# Library Properties
# =============================================================================

set_target_properties(datascript PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    POSITION_INDEPENDENT_CODE ON
    FOLDER "Libraries"
)

# =============================================================================
# Export Header (for shared library builds)
# =============================================================================

if(NEUTRINO_DATASCRIPT_BUILD_SHARED)
    include(GenerateExportHeader)
    generate_export_header(datascript
        EXPORT_FILE_NAME ${CMAKE_CURRENT_BINARY_DIR}/datascript/export.h
    )
    target_include_directories(datascript
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_BINARY_DIR}>
    )
endif()

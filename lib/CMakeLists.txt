cmake_minimum_required(VERSION 3.16)
project(datascript_lib)

include(FetchContent)
set(FETCHCONTENT_QUIET OFF)

# Allow FetchContent_Populate for ExternalProject_Add pattern (CMP0169)
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

# =========================================================================
# Option 1: Try to find system re2c first
# =========================================================================
find_program(RE2C_EXECUTABLE re2c)

if(NOT RE2C_EXECUTABLE)
    message(STATUS "re2c not found, will build from source")

    # Fetch re2c source
    FetchContent_Declare(
        re2c
        GIT_REPOSITORY "https://github.com/skvadrik/re2c.git"
        GIT_TAG "3.1"
    )

    # Use GetProperties + Populate pattern (compatible with ExternalProject_Add)
    FetchContent_GetProperties(re2c)
    if(NOT re2c_POPULATED)
        FetchContent_Populate(re2c)
    endif()

    # Build re2c as external project (since it's a tool, not a library)
    include(ExternalProject)
    ExternalProject_Add(
        re2c_build
        SOURCE_DIR ${re2c_SOURCE_DIR}
        CMAKE_ARGS
            -DCMAKE_BUILD_TYPE=Release
            -DRE2C_BUILD_RE2GO=OFF
            -DRE2C_BUILD_RE2RUST=OFF
            -DRE2C_BUILD_BENCHMARKS=OFF
        INSTALL_COMMAND ""
        BUILD_BYPRODUCTS <BINARY_DIR>/re2c
    )
    ExternalProject_Get_Property(re2c_build BINARY_DIR)
    set(RE2C_EXECUTABLE ${BINARY_DIR}/re2c)
    set(RE2C_TARGET re2c_build)
else()
    message(STATUS "Found re2c: ${RE2C_EXECUTABLE}")
endif()



# =========================================================================
# Build lemon from bundled source (it's just 2 files)
# =========================================================================
set(LEMON_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lemon)
set(LEMPAR_FILE ${LEMON_DIR}/lempar.c)
set(LEMON_OUT "${CMAKE_CURRENT_BINARY_DIR}/gen")
set(PARSER_Y ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_parser.y)
set(PARSER_C ${LEMON_OUT}/datascript_parser.c)
set(PARSER_H ${LEMON_OUT}/datascript_parser.h)


add_executable(lemon ${LEMON_DIR}/lemon.c)
file(MAKE_DIRECTORY ${LEMON_OUT})

add_custom_command(
    OUTPUT ${PARSER_C} ${PARSER_H}
    COMMAND $<TARGET_FILE:lemon> -T${LEMPAR_FILE} -d${LEMON_OUT} ${PARSER_Y} || true
    DEPENDS ${PARSER_Y} lemon ${LEMPAR_FILE}
    COMMENT "Generating parser with lemon (accepting shift/reduce conflicts)"
)

# =========================================================================
# Generate scanner from re2c specification
# =========================================================================
set(SCANNER_RE ${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_scanner.re)
set(SCANNER_C ${LEMON_OUT}/datascript_scanner.c)

add_custom_command(
        OUTPUT ${SCANNER_C}
        COMMAND ${RE2C_EXECUTABLE} -o ${SCANNER_C} ${SCANNER_RE}
        DEPENDS ${SCANNER_RE} ${RE2C_TARGET} ${PARSER_H}
        COMMENT "Generating scanner with re2c"
)

# =========================================================================
# Build library
# =========================================================================
add_library(datascript STATIC
        # Generated C files
        ${SCANNER_C}
        ${PARSER_C}
        ${PARSER_H}


        src/parser/ast_builder.cc
        src/parser/ast_builder.h
        src/parser/ast_holder.hh
        src/parser/scanner_context.h
        src/parser/parser_context.h
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

        # IR and Code Generation
        src/ir/ir_builder.cc

        # Command-based Code Generation
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

        # Kaitai Struct to IR Builder
        src/ksy/ksy_to_ir_builder.cc

        include/datascript/ast.hh
        include/datascript/parser.hh
        include/datascript/parser_error.hh
        include/datascript/semantic.hh
        include/datascript/ir.hh
        include/datascript/ir_builder.hh
        include/datascript/codegen.hh
        include/datascript/codegen_commands.hh
        include/datascript/command_builder.hh
        include/datascript/codegen/code_writer.hh
        include/datascript/codegen/option_description.hh
        include/datascript/codegen/cpp/cpp_code_writer.hh
        include/datascript/codegen/cpp/cpp_helper_generator.hh
        include/datascript/codegen/cpp/cpp_expression_renderer.hh
        include/datascript/codegen/cpp/cpp_renderer.hh
        include/datascript/codegen/datascript/datascript_renderer.hh
        include/datascript/base_renderer.hh
        include/datascript/renderer_registry.hh
        include/datascript/ksy/ksy_to_ir_builder.hh
)

target_include_directories(datascript
        PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src/
        PRIVATE ${CMAKE_CURRENT_BINARY_DIR})

# External dependencies as SYSTEM to suppress warnings
target_include_directories(datascript SYSTEM
        PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/external)

# Apply strict warnings to the library
target_set_strict_warnings(datascript)

# Disable warnings for generated C files (re2c scanner, lemon parser)
# These are auto-generated and may contain code patterns that trigger warnings
if(MSVC)
    set_source_files_properties(${SCANNER_C} ${PARSER_C} PROPERTIES COMPILE_FLAGS "/W0")
else()
    set_source_files_properties(${SCANNER_C} ${PARSER_C} PROPERTIES COMPILE_FLAGS "-w")
endif()


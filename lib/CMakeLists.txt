# =============================================================================
# DataScript Library
# =============================================================================

# =============================================================================
# Grammar (Lemon Parser & re2c Scanner)
# =============================================================================

set(LEMON_OUT "${CMAKE_CURRENT_BINARY_DIR}/gen")

neutrino_add_grammar(
    NAME datascript
    PARSER "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_parser.y"
    SCANNER "${CMAKE_CURRENT_SOURCE_DIR}/src/parser/datascript_scanner.re"
    OUTPUT_DIR "${LEMON_OUT}"
    ALLOW_CONFLICTS ON
    OUTPUT_SOURCES DATASCRIPT_GRAMMAR_SOURCES
    OUTPUT_HEADERS DATASCRIPT_GRAMMAR_HEADERS
)

# =============================================================================
# Library Sources
# =============================================================================

set(DATASCRIPT_SOURCES
    # Generated files
    ${DATASCRIPT_GRAMMAR_SOURCES}

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
    set_source_files_properties(${DATASCRIPT_GRAMMAR_SOURCES} PROPERTIES COMPILE_FLAGS "/W0")
else()
    set_source_files_properties(${DATASCRIPT_GRAMMAR_SOURCES} PROPERTIES COMPILE_FLAGS "-w")
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

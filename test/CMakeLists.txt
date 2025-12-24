# =============================================================================
# DataScript Tests
# =============================================================================

# =============================================================================
# Code Generation for E2E Testing
# =============================================================================

set(CODEGEN_SCHEMAS
    simple_types
    bit_manipulation
    network_packet
    e2e_primitives
    e2e_arrays
    e2e_enums
    e2e_bitfields
    e2e_choices
    e2e_conditionals
    e2e_endianness
    e2e_parameterized
    e2e_expressions
    e2e_real_world
    e2e_strings
    e2e_function_calls
    e2e_subtypes
    e2e_labels_alignment
    e2e_labels_complex
    e2e_exe_format
)

set(CODEGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/codegen/generated)
file(MAKE_DIRECTORY ${CODEGEN_OUTPUT_DIR})

set(GENERATED_HEADERS "")
foreach(SCHEMA ${CODEGEN_SCHEMAS})
    set(SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/codegen/schemas/${SCHEMA}.ds)
    set(HEADER_FILE ${CODEGEN_OUTPUT_DIR}/${SCHEMA}.h)

    add_custom_command(
        OUTPUT ${HEADER_FILE}
        COMMAND $<TARGET_FILE:ds> -q -t cpp --cpp-output-name=${SCHEMA}.h -o ${CODEGEN_OUTPUT_DIR} ${SCHEMA_FILE}
        DEPENDS ds ${SCHEMA_FILE}
        COMMENT "Generating ${SCHEMA}.h from ${SCHEMA}.ds"
        VERBATIM
    )

    list(APPEND GENERATED_HEADERS ${HEADER_FILE})
endforeach()

add_custom_target(generate_test_headers ALL DEPENDS ${GENERATED_HEADERS})

# =============================================================================
# Library Mode Code Generation
# =============================================================================

set(LIBRARY_MODE_SCHEMAS
    library_mode_test
    library_mode_choices
    library_mode_strings
    library_mode_bitfields
    library_mode_subtypes
    library_mode_parameterized
    library_mode_endianness
    library_mode_labels_alignment
    library_mode_conditionals
    library_mode_inline_discrim
    inline_discrim_default_bug
)

set(LIBRARY_MODE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/codegen/library_mode_generated)
file(MAKE_DIRECTORY ${LIBRARY_MODE_OUTPUT_DIR})

set(LIBRARY_MODE_GENERATED_HEADERS "")
foreach(SCHEMA ${LIBRARY_MODE_SCHEMAS})
    set(SCHEMA_FILE ${CMAKE_CURRENT_SOURCE_DIR}/codegen/schemas/${SCHEMA}.ds)
    set(RUNTIME_HEADER ${LIBRARY_MODE_OUTPUT_DIR}/${SCHEMA}_runtime.h)
    set(PUBLIC_HEADER ${LIBRARY_MODE_OUTPUT_DIR}/${SCHEMA}.h)
    set(IMPL_HEADER ${LIBRARY_MODE_OUTPUT_DIR}/${SCHEMA}_impl.h)

    add_custom_command(
        OUTPUT ${RUNTIME_HEADER} ${PUBLIC_HEADER} ${IMPL_HEADER}
        COMMAND $<TARGET_FILE:ds> -q -t cpp --cpp-mode=library --cpp-output-name=${SCHEMA}.h -o ${LIBRARY_MODE_OUTPUT_DIR} ${SCHEMA_FILE}
        DEPENDS ds ${SCHEMA_FILE}
        COMMENT "Generating library mode headers for ${SCHEMA}"
        VERBATIM
    )

    list(APPEND LIBRARY_MODE_GENERATED_HEADERS ${RUNTIME_HEADER} ${PUBLIC_HEADER} ${IMPL_HEADER})
endforeach()

add_custom_target(generate_library_mode_headers ALL DEPENDS ${LIBRARY_MODE_GENERATED_HEADERS})

# =============================================================================
# Fetch doctest via neutrino-cmake
# =============================================================================

include(${NEUTRINO_CMAKE_DIR}/deps/doctest.cmake)
neutrino_fetch_doctest()

# =============================================================================
# Test Executable
# =============================================================================

add_executable(datascript_unittest
    main.cc
    codegen/test_cpp_expr_renderer.cc
    codegen/test_command_builder.cc
    codegen/test_command_builder_exception_safety.cc
    codegen/test_cpp_renderer.cc
    codegen/test_cpp_renderer_validation.cc
    codegen/test_renderer_registry.cc
    codegen/test_datascript_renderer.cc
    codegen/test_code_writer.cc
    codegen/test_cpp_writer_context.cc
    codegen/test_complex_expressions.cc
    codegen/test_integration.cc
    codegen/test_labels_alignment.cc
    codegen/test_user_functions.cc
    parser/ast/test_integer_literals.cc
    parser/ast/test_parser_basic.cc
    parser/ast/test_docstrings.cc
    parser/ast/test_docstring_edges.cc
    parser/ast/test_identifiers_qualified_names.cc
    parser/ast/test_expressions.cc
    parser/ast/test_bit_fields.cc
    parser/ast/test_constraints.cc
    parser/ast/test_package_import.cc
    parser/ast/test_enums.cc
    parser/ast/test_arrays.cc
    parser/ast/test_structs.cc
    parser/ast/test_unions.cc
    parser/ast/test_choices.cc
    parser/ast/test_field_access.cc
    parser/ast/test_array_indexing.cc
    parser/ast/test_function_calls.cc
    parser/ast/test_functions.cc
    parser/ast/test_endianness_directives.cc
    parser/ast/test_subtypes.cc
    parser/test_module_loading.cc
    semantic/test_symbol_collection.cc
    semantic/test_subtypes.cc
    semantic/test_name_resolution.cc
    semantic/test_type_checking.cc
    semantic/test_parameter_validation.cc
    semantic/test_constant_evaluation.cc
    semantic/test_size_calculation.cc
    semantic/test_constraint_validation.cc
    semantic/test_reachability.cc
    semantic/test_keyword_validation.cc
    ir/test_codegen_basic.cc
    ir/test_codegen_arrays.cc
    ir/test_codegen_variable_arrays.cc
    ir/test_codegen_conditionals.cc
    ir/test_codegen_bitfields.cc
    ir/test_codegen_constant_folding.cc
    ir/test_codegen_constraints.cc
    ir/test_codegen_enums.cc
    ir/test_codegen_ranged_arrays.cc
    ir/test_codegen_unions.cc
    ir/test_codegen_choices.cc
    ir/test_codegen_comprehensive.cc
    ir/test_codegen_parameterized.cc
    ir/test_codegen_subtypes.cc
    ir/test_size_calculation.cc
    codegen/test_generated_simple_types.cc
    codegen/test_generated_bit_manipulation.cc
    codegen/test_generated_network_packet.cc
    codegen/e2e/test_e2e_primitives.cc
    codegen/e2e/test_e2e_arrays.cc
    codegen/e2e/test_e2e_enums.cc
    codegen/e2e/test_e2e_bitfields.cc
    codegen/e2e/test_e2e_choices.cc
    codegen/e2e/test_e2e_conditionals.cc
    codegen/e2e/test_e2e_endianness.cc
    codegen/e2e/test_e2e_parameterized.cc
    codegen/e2e/test_e2e_expressions.cc
    codegen/e2e/test_e2e_real_world.cc
    codegen/e2e/test_e2e_strings.cc
    codegen/e2e/test_e2e_function_calls.cc
    codegen/e2e/test_e2e_subtypes.cc
    codegen/e2e/test_e2e_labels_alignment.cc
    codegen/e2e/test_e2e_labels_complex.cc
    codegen/e2e/test_e2e_exe_format.cc
    codegen/test_library_mode_choices.cc
    codegen/test_library_mode_strings.cc
    codegen/test_library_mode_bitfields.cc
    codegen/test_library_mode_subtypes.cc
    codegen/test_library_mode_parameterized.cc
    codegen/test_library_mode_endianness.cc
    codegen/test_library_mode_labels_alignment.cc
    codegen/test_library_mode_conditionals.cc
    codegen/test_library_mode_inline_discrim.cc
    codegen/test_inline_discrim_default_bug.cc
    codegen/test_choice_discriminator_e2e.cc
    codegen/test_choice_default_case_bug.cc
    codegen/test_inline_discriminator_peek.cc
    codegen/test_range_discriminator.cc
    bugreport/test_bugreport_fixes.cc
)

# Dependencies
add_dependencies(datascript_unittest generate_test_headers generate_library_mode_headers)

target_link_libraries(datascript_unittest
    PRIVATE
        datascript
        doctest::doctest
)

# Include directories
target_include_directories(datascript_unittest
    PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/src
        ${CODEGEN_OUTPUT_DIR}
        ${LIBRARY_MODE_OUTPUT_DIR}
)

target_compile_definitions(datascript_unittest
    PRIVATE
        UNITTEST_HOME="${CMAKE_CURRENT_SOURCE_DIR}"
)

# Apply warnings (but don't treat as errors for test code)
target_set_warnings_no_error(datascript_unittest)

# Set target properties
set_target_properties(datascript_unittest PROPERTIES
    FOLDER "Tests"
)

# Register with CTest
include(CTest)
add_test(NAME datascript_tests COMMAND datascript_unittest)

# =============================================================================
# Code Generation for E2E Testing
# =============================================================================

# Define the schemas to generate headers from
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

# Create output directory for generated headers
set(CODEGEN_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/codegen/generated)
file(MAKE_DIRECTORY ${CODEGEN_OUTPUT_DIR})

# Add custom commands to generate headers from schemas
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

# Create a custom target that depends on all generated headers
add_custom_target(generate_test_headers ALL DEPENDS ${GENERATED_HEADERS})

# =============================================================================
# Library Mode Code Generation for E2E Testing
# =============================================================================

# Define schemas to generate library mode headers from
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
)

# Create output directory for library mode generated headers
set(LIBRARY_MODE_OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/codegen/library_mode_generated)
file(MAKE_DIRECTORY ${LIBRARY_MODE_OUTPUT_DIR})

# Add custom commands to generate library mode headers (3 files per schema)
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

# Create a custom target for library mode headers
add_custom_target(generate_library_mode_headers ALL DEPENDS ${LIBRARY_MODE_GENERATED_HEADERS})

# =============================================================================
# Fetch doctest if not already available
# =============================================================================
find_package(doctest QUIET)

if(NOT doctest_FOUND)
    message(STATUS "doctest not found, fetching from GitHub...")

    include(FetchContent)
    set(FETCHCONTENT_QUIET OFF)

    # Allow FetchContent_Populate to be used (suppress CMP0169 warning)
    if(POLICY CMP0169)
        cmake_policy(SET CMP0169 OLD)
    endif()

    # Configure doctest options
    set(DOCTEST_WITH_TESTS OFF CACHE INTERNAL "")
    set(DOCTEST_WITH_MAIN_IN_STATIC_LIB OFF CACHE INTERNAL "")
    set(DOCTEST_NO_INSTALL ON CACHE INTERNAL "")

    FetchContent_Declare(
            doctest
            GIT_REPOSITORY https://github.com/doctest/doctest.git
            GIT_TAG v2.4.12
            GIT_PROGRESS TRUE
    )

    # Fetch the content first
    FetchContent_GetProperties(doctest)
    if(NOT doctest_POPULATED)
        FetchContent_Populate(doctest)

        # Patch the CMakeLists.txt to fix deprecation warning
        file(READ ${doctest_SOURCE_DIR}/CMakeLists.txt DOCTEST_CMAKE_CONTENT)
        # Replace any version less than 3.10 with 3.10
        string(REGEX REPLACE "cmake_minimum_required\\(VERSION [0-9]+\\.[0-9]+\\)"
                "cmake_minimum_required(VERSION 3.10)"
                DOCTEST_CMAKE_CONTENT "${DOCTEST_CMAKE_CONTENT}")
        file(WRITE ${doctest_SOURCE_DIR}/CMakeLists.txt "${DOCTEST_CMAKE_CONTENT}")

        # Now add the subdirectory
        add_subdirectory(${doctest_SOURCE_DIR} ${doctest_BINARY_DIR} EXCLUDE_FROM_ALL)
    endif()

    # Verify doctest::doctest target was created
    if(TARGET doctest::doctest)
        message(STATUS "doctest::doctest target created successfully")
    elseif(TARGET doctest)
        # Create alias if only 'doctest' target exists
        add_library(doctest::doctest ALIAS doctest)
        message(STATUS "Created doctest::doctest alias")
    else()
        message(FATAL_ERROR "doctest failed to create expected targets")
    endif()
else()
    message(STATUS "Using system-provided doctest")
endif()


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
        # codegen/test_library_mode_e2e.cc  # TODO: Update to use actual library mode schema
        codegen/test_library_mode_choices.cc
        codegen/test_library_mode_strings.cc
        codegen/test_library_mode_bitfields.cc
        codegen/test_library_mode_subtypes.cc
        codegen/test_library_mode_parameterized.cc
        codegen/test_library_mode_endianness.cc
        codegen/test_library_mode_labels_alignment.cc
        codegen/test_library_mode_conditionals.cc
        codegen/test_library_mode_inline_discrim.cc
        codegen/test_choice_discriminator_e2e.cc
        codegen/test_choice_default_case_bug.cc
        codegen/test_inline_discriminator_peek.cc
        bugreport/test_bugreport_fixes.cc
)

# Make unittest depend on generated headers
add_dependencies(datascript_unittest generate_test_headers generate_library_mode_headers)

target_link_libraries(datascript_unittest PRIVATE datascript doctest::doctest)

# Make parser internals and generated headers accessible for testing
target_include_directories(datascript_unittest PRIVATE
        ${CMAKE_SOURCE_DIR}/lib/src
        ${CODEGEN_OUTPUT_DIR}
        ${LIBRARY_MODE_OUTPUT_DIR}
)

target_compile_definitions(datascript_unittest PRIVATE UNITTEST_HOME="${CMAKE_CURRENT_SOURCE_DIR}")
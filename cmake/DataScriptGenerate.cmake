#[=======================================================================[.rst:
DataScriptGenerate
------------------

Functions for generating C++ code from DataScript schemas.

.. command:: datascript_generate

  Generate C++ headers from DataScript schema files::

    datascript_generate(
        TARGET <target>
        SCHEMAS <schema1.ds> [schema2.ds ...]
        [OUTPUT_DIR <dir>]
        [INCLUDE_DIR <dir>]
        [PRESERVE_PACKAGE_DIRS <ON|OFF>]
        [IMPORT_DIRS <dir1> [dir2 ...]]
        [LANGUAGE <cpp>]
        [OPTIONS <opt1> [opt2 ...]]
    )

  Creates an INTERFACE library ``<target>`` that other targets can link
  against to use the generated headers.

  Arguments:
    TARGET              - Name of the interface library to create
    SCHEMAS             - List of .ds schema files (absolute or relative paths)
    OUTPUT_DIR          - Directory for generated files
                          (default: ${CMAKE_CURRENT_BINARY_DIR}/datascript_gen)
    INCLUDE_DIR         - Directory added to target_include_directories
                          (default: same as OUTPUT_DIR)
    PRESERVE_PACKAGE_DIRS - Whether to create package-based subdirectories
                          ON (default): formats.mz -> formats/mz/file.hh
                          OFF: formats.mz -> file.hh (flat structure)
    IMPORT_DIRS         - Additional import search paths
    LANGUAGE            - Target language (default: cpp)
    OPTIONS             - Additional ds compiler options (e.g., --cpp-mode=library)

Example usage::

    # Basic usage (package dirs preserved)
    datascript_generate(
        TARGET exe_parsers
        SCHEMAS
            ${CMAKE_CURRENT_SOURCE_DIR}/schemas/mz.ds
            ${CMAKE_CURRENT_SOURCE_DIR}/schemas/pe/pe_header.ds
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/schemas
    )
    # Include: #include <formats/mz/image_dos_header.hh>

    # Flat output (no package directories)
    datascript_generate(
        TARGET flat_parsers
        SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/mz.ds
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/schemas
        PRESERVE_PACKAGE_DIRS OFF
    )
    # Include: #include <image_dos_header.hh>

    # Custom include prefix
    datascript_generate(
        TARGET prefixed_parsers
        SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/mz.ds
        OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen/myproject
        INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
        IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/schemas
    )
    # Include: #include <myproject/formats/mz/image_dos_header.hh>

    add_executable(my_app main.cpp)
    target_link_libraries(my_app PRIVATE exe_parsers)

#]=======================================================================]

function(datascript_generate)
    cmake_parse_arguments(DS
        ""                                              # Options (flags)
        "TARGET;OUTPUT_DIR;INCLUDE_DIR;PRESERVE_PACKAGE_DIRS;LANGUAGE" # Single-value args
        "SCHEMAS;IMPORT_DIRS;OPTIONS"                   # Multi-value args
        ${ARGN}
    )

    # Validate required arguments
    if(NOT DS_TARGET)
        message(FATAL_ERROR "datascript_generate: TARGET is required")
    endif()
    if(NOT DS_SCHEMAS)
        message(FATAL_ERROR "datascript_generate: SCHEMAS is required")
    endif()

    # Set defaults
    if(NOT DS_OUTPUT_DIR)
        set(DS_OUTPUT_DIR "${CMAKE_CURRENT_BINARY_DIR}/datascript_gen")
    endif()
    if(NOT DS_INCLUDE_DIR)
        set(DS_INCLUDE_DIR "${DS_OUTPUT_DIR}")
    endif()
    if(NOT DEFINED DS_PRESERVE_PACKAGE_DIRS)
        set(DS_PRESERVE_PACKAGE_DIRS ON)
    endif()
    if(NOT DS_LANGUAGE)
        set(DS_LANGUAGE "cpp")
    endif()

    # Build import directory arguments
    set(import_args "")
    foreach(dir IN LISTS DS_IMPORT_DIRS)
        list(APPEND import_args "-I${dir}")
    endforeach()

    # Build flat output flag if needed
    set(flat_output_arg "")
    if(NOT DS_PRESERVE_PACKAGE_DIRS)
        set(flat_output_arg "--flat-output")
    endif()

    # Process each schema file
    set(all_generated_files "")
    set(all_schema_deps "")

    foreach(schema IN LISTS DS_SCHEMAS)
        # Convert to absolute path if relative
        if(NOT IS_ABSOLUTE "${schema}")
            set(schema "${CMAKE_CURRENT_SOURCE_DIR}/${schema}")
        endif()

        # Get import dependencies using ds --print-imports
        execute_process(
            COMMAND $<TARGET_FILE:ds> --print-imports ${import_args} "${schema}"
            OUTPUT_VARIABLE imports_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE imports_result
            ERROR_QUIET
        )

        # Get output files using ds --print-outputs
        execute_process(
            COMMAND $<TARGET_FILE:ds> --print-outputs
                    -t ${DS_LANGUAGE} ${flat_output_arg} ${import_args} ${DS_OPTIONS} "${schema}"
            OUTPUT_VARIABLE outputs_output
            OUTPUT_STRIP_TRAILING_WHITESPACE
            RESULT_VARIABLE outputs_result
            ERROR_QUIET
        )

        # Parse output files (one per line)
        string(REPLACE "\n" ";" output_files "${outputs_output}")

        # Build list of generated files with full paths
        # Output files are relative paths like "formats/mz/image_dos_header.hh"
        set(schema_outputs "")
        foreach(output_file IN LISTS output_files)
            if(output_file)
                list(APPEND schema_outputs "${DS_OUTPUT_DIR}/${output_file}")
            endif()
        endforeach()

        # Collect directories that need to be created
        set(output_dirs "")
        foreach(output_file IN LISTS schema_outputs)
            get_filename_component(output_dir "${output_file}" DIRECTORY)
            list(APPEND output_dirs "${output_dir}")
        endforeach()
        list(REMOVE_DUPLICATES output_dirs)

        # Resolve import dependencies to file paths
        set(import_deps "${schema}")  # Always depend on the schema itself
        string(REPLACE "\n" ";" import_list "${imports_output}")
        foreach(import_pkg IN LISTS import_list)
            if(import_pkg)
                # Convert package name to path: foo.bar -> foo/bar.ds
                string(REPLACE "." "/" import_path "${import_pkg}")
                # Search in import dirs
                foreach(dir IN LISTS DS_IMPORT_DIRS)
                    if(EXISTS "${dir}/${import_path}.ds")
                        list(APPEND import_deps "${dir}/${import_path}.ds")
                        break()
                    endif()
                endforeach()
            endif()
        endforeach()

        # Build mkdir commands for all needed directories
        set(mkdir_commands "")
        foreach(dir IN LISTS output_dirs)
            list(APPEND mkdir_commands COMMAND ${CMAKE_COMMAND} -E make_directory "${dir}")
        endforeach()

        # Add custom command for this schema
        add_custom_command(
            OUTPUT ${schema_outputs}
            ${mkdir_commands}
            COMMAND $<TARGET_FILE:ds>
                    -t ${DS_LANGUAGE}
                    -o "${DS_OUTPUT_DIR}"
                    ${flat_output_arg}
                    ${import_args}
                    ${DS_OPTIONS}
                    "${schema}"
            DEPENDS ds ${import_deps}
            COMMENT "DataScript: Generating from ${schema}"
            VERBATIM
        )

        list(APPEND all_generated_files ${schema_outputs})
        list(APPEND all_schema_deps ${import_deps})
    endforeach()

    # Create interface library target
    add_library(${DS_TARGET} INTERFACE)
    target_sources(${DS_TARGET} INTERFACE ${all_generated_files})
    target_include_directories(${DS_TARGET} INTERFACE "${DS_INCLUDE_DIR}")

    # Create a custom target to trigger generation
    add_custom_target(${DS_TARGET}_generate DEPENDS ${all_generated_files})
    add_dependencies(${DS_TARGET} ${DS_TARGET}_generate)
endfunction()

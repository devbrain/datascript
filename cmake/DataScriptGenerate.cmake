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

#[=======================================================================[
Helper function: Extract package name from a DataScript schema file.

Reads the file and looks for a "package" declaration to determine
the package path. Returns empty string if no package declaration found.
#]=======================================================================]
function(_datascript_extract_package schema_file out_package)
    # Read the schema file
    file(READ "${schema_file}" schema_content)

    # Look for package declaration: package foo.bar.baz;
    # Match: "package" followed by whitespace, then identifier(s) separated by dots,
    # then optional whitespace, then semicolon.
    # NOTE: Only whitespace is allowed between identifier and semicolon to avoid
    # matching "package" in comments like "This package defines..."
    string(REGEX MATCH "package[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_.]*)[ \t\r\n]*;" package_match "${schema_content}")

    if(package_match)
        string(REGEX REPLACE "package[ \t\r\n]+([a-zA-Z_][a-zA-Z0-9_.]*)[ \t\r\n]*;" "\\1" package_name "${package_match}")
        set(${out_package} "${package_name}" PARENT_SCOPE)
    else()
        set(${out_package} "" PARENT_SCOPE)
    endif()
endfunction()

#[=======================================================================[
Helper function: Extract imports from a DataScript schema file.

Reads the file and looks for "import" declarations.
Returns a list of package names that are imported.
#]=======================================================================]
function(_datascript_extract_imports schema_file out_imports)
    # Read the schema file
    file(READ "${schema_file}" schema_content)

    # Find all import declarations: import foo.bar;
    # Use a simpler approach: find all matches
    set(imports_list "")

    # Split content into lines for easier processing
    string(REPLACE "\n" ";" schema_lines "${schema_content}")

    foreach(line IN LISTS schema_lines)
        # Match import declarations (not wildcard imports for now)
        if(line MATCHES "^[ \t]*import[ \t]+([a-zA-Z_][a-zA-Z0-9_.]*);")
            string(REGEX REPLACE "^[ \t]*import[ \t]+([a-zA-Z_][a-zA-Z0-9_.]*);.*" "\\1" import_name "${line}")
            list(APPEND imports_list "${import_name}")
        endif()
    endforeach()

    set(${out_imports} "${imports_list}" PARENT_SCOPE)
endfunction()

#[=======================================================================[
Helper function: Compute output filenames for a DataScript schema.

Based on the schema filename, package declaration, and options,
computes the expected output filenames without running the ds compiler.

For C++ (default):
  - Single-header mode: {schema_basename}.hh
  - Library mode: {schema_basename}.h, {schema_basename}_impl.h, {schema_basename}_runtime.h

The package path prefix is added if PRESERVE_PACKAGE_DIRS is ON.
#]=======================================================================]
function(_datascript_compute_outputs schema_file package_name options preserve_package_dirs language out_files out_subdir)
    # Get the base name from schema filename (without extension)
    get_filename_component(schema_basename "${schema_file}" NAME_WE)

    # Compute package-based subdirectory
    set(pkg_subdir "")
    if(preserve_package_dirs AND package_name)
        # Convert package.name to package/name
        string(REPLACE "." "/" pkg_subdir "${package_name}")
    endif()

    # Determine output mode from options
    set(is_library_mode FALSE)
    foreach(opt IN LISTS options)
        if(opt MATCHES "--cpp-mode=library")
            set(is_library_mode TRUE)
        endif()
    endforeach()

    # Generate output filenames based on language and mode
    # With --use-input-name, the output base is always the schema basename
    set(output_files "")

    if(language STREQUAL "cpp")
        if(is_library_mode)
            # Library mode: three header files (uses .h extension)
            list(APPEND output_files "${schema_basename}.h")
            list(APPEND output_files "${schema_basename}_impl.h")
            list(APPEND output_files "${schema_basename}_runtime.h")
        else()
            # Single-header mode (default, uses .hh extension)
            list(APPEND output_files "${schema_basename}.hh")
        endif()
    else()
        # Unknown language - use schema basename with .h extension
        list(APPEND output_files "${schema_basename}.h")
    endif()

    set(${out_files} "${output_files}" PARENT_SCOPE)
    set(${out_subdir} "${pkg_subdir}" PARENT_SCOPE)
endfunction()

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

    # Build import directory arguments for ds command
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

        # Verify schema file exists
        if(NOT EXISTS "${schema}")
            message(FATAL_ERROR "datascript_generate: Schema file not found: ${schema}")
        endif()

        # Extract package declaration from schema file
        _datascript_extract_package("${schema}" schema_package)

        # Extract imports from schema file
        _datascript_extract_imports("${schema}" schema_imports)

        # Compute output filenames based on schema, package, and options
        _datascript_compute_outputs(
            "${schema}"
            "${schema_package}"
            "${DS_OPTIONS}"
            "${DS_PRESERVE_PACKAGE_DIRS}"
            "${DS_LANGUAGE}"
            output_basenames
            pkg_subdir
        )

        # Build full output paths
        set(schema_outputs "")
        if(pkg_subdir)
            set(output_subdir "${DS_OUTPUT_DIR}/${pkg_subdir}")
        else()
            set(output_subdir "${DS_OUTPUT_DIR}")
        endif()

        foreach(output_basename IN LISTS output_basenames)
            list(APPEND schema_outputs "${output_subdir}/${output_basename}")
        endforeach()

        # Resolve import dependencies to file paths
        set(import_deps "${schema}")  # Always depend on the schema itself
        foreach(import_pkg IN LISTS schema_imports)
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

        # Add custom command for this schema
        # Note: We use $<TARGET_FILE:ds> here which is valid in add_custom_command
        # (evaluated at build time, not configure time)
        # --use-input-name ensures output filename matches schema filename (foo.ds -> foo.hh)
        add_custom_command(
            OUTPUT ${schema_outputs}
            COMMAND ${CMAKE_COMMAND} -E make_directory "${output_subdir}"
            COMMAND $<TARGET_FILE:ds>
                    -t ${DS_LANGUAGE}
                    -o "${DS_OUTPUT_DIR}"
                    --use-input-name
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

# CMake Integration Guide

This guide explains how to integrate DataScript into CMake-based projects to automatically generate C++ parsers from `.ds` schema files.

## Table of Contents

- [Quick Start](#quick-start)
- [Using FetchContent](#using-fetchcontent)
- [The datascript_generate Function](#the-datascript_generate-function)
- [Parameters Reference](#parameters-reference)
- [Output Directory Structure](#output-directory-structure)
- [Include Path Configuration](#include-path-configuration)
- [Dependency Tracking](#dependency-tracking)
- [Advanced Usage](#advanced-usage)
- [Troubleshooting](#troubleshooting)

## Quick Start

The simplest way to use DataScript in your CMake project:

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyProject)

# Fetch DataScript
include(FetchContent)
FetchContent_Declare(
    DataScript
    GIT_REPOSITORY https://github.com/user/DataScript.git
    GIT_TAG main
)
FetchContent_MakeAvailable(DataScript)

# Generate parsers from schema files
datascript_generate(
    TARGET my_parsers
    SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/protocol.ds
    IMPORT_DIRS ${CMAKE_CURRENT_SOURCE_DIR}/schemas
)

# Link to your executable
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE my_parsers)
```

In your C++ code:

```cpp
#include <protocol/message.hh>  // Generated header

int main() {
    std::vector<uint8_t> data = load_binary_data();
    auto msg = generated::Message::read(data.data(), data.data() + data.size());
    // Use parsed message...
}
```

## Using FetchContent

DataScript is designed to work seamlessly with CMake's `FetchContent` module. When you call `FetchContent_MakeAvailable(DataScript)`, the following happens:

1. DataScript source is downloaded/updated
2. The `ds` compiler is built as part of your project
3. The `datascript_generate()` function becomes available

```cmake
include(FetchContent)

FetchContent_Declare(
    DataScript
    GIT_REPOSITORY https://github.com/user/DataScript.git
    GIT_TAG v1.0.0  # Use a specific version for reproducibility
)

# Optional: Disable DataScript's own tests to speed up build
set(DATASCRIPT_BUILD_TESTS OFF CACHE BOOL "" FORCE)

FetchContent_MakeAvailable(DataScript)

# Now datascript_generate() is available
```

## The datascript_generate Function

The `datascript_generate()` function creates an INTERFACE library target that:

- Generates C++ headers from DataScript schema files
- Automatically rebuilds when schema files change
- Provides proper include directories to consuming targets

### Basic Syntax

```cmake
datascript_generate(
    TARGET <target_name>
    SCHEMAS <schema1.ds> [schema2.ds ...]
    [OUTPUT_DIR <directory>]
    [INCLUDE_DIR <directory>]
    [PRESERVE_PACKAGE_DIRS <ON|OFF>]
    [IMPORT_DIRS <dir1> [dir2 ...]]
    [LANGUAGE <cpp>]
    [OPTIONS <opt1> [opt2 ...]]
)
```

## Parameters Reference

### TARGET (required)

Name of the INTERFACE library target to create. Other targets can link against this to use the generated headers.

```cmake
datascript_generate(
    TARGET protocol_parsers
    ...
)

add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE protocol_parsers)
```

### SCHEMAS (required)

List of DataScript schema files to process. Paths can be absolute or relative to `CMAKE_CURRENT_SOURCE_DIR`.

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas/mz.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas/pe/pe_header.ds
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas/resources/dialogs.ds
    ...
)
```

### OUTPUT_DIR

Directory where generated files are written.

**Default:** `${CMAKE_CURRENT_BINARY_DIR}/datascript_gen`

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS ...
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/generated
)
```

### INCLUDE_DIR

Directory added to `target_include_directories()` for the generated INTERFACE library. This controls how you `#include` the generated headers.

**Default:** Same as `OUTPUT_DIR`

```cmake
# Custom include prefix
datascript_generate(
    TARGET parsers
    SCHEMAS ...
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen/myproject
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
)
# Include as: #include <myproject/formats/mz/header.hh>
```

### PRESERVE_PACKAGE_DIRS

Controls whether package-based subdirectories are created.

**Default:** `ON`

| Value | Behavior | Include Path |
|-------|----------|--------------|
| `ON` | `package formats.mz;` → `formats/mz/header.hh` | `#include <formats/mz/header.hh>` |
| `OFF` | `package formats.mz;` → `header.hh` | `#include <header.hh>` |

```cmake
# Flat output structure
datascript_generate(
    TARGET flat_parsers
    SCHEMAS ...
    PRESERVE_PACKAGE_DIRS OFF
)
```

### IMPORT_DIRS

Search paths for resolving `import` statements in schema files. Similar to `-I` in C/C++ compilers.

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/pe/pe_header.ds
    IMPORT_DIRS
        ${CMAKE_CURRENT_SOURCE_DIR}/schemas
        ${CMAKE_CURRENT_SOURCE_DIR}/common
)
```

### LANGUAGE

Target language for code generation.

**Default:** `cpp`

Currently only `cpp` is supported.

### OPTIONS

Additional options passed to the `ds` compiler. Useful for generator-specific settings.

```cmake
# Generate library mode headers (three files per schema)
datascript_generate(
    TARGET lib_parsers
    SCHEMAS ...
    OPTIONS --cpp-mode=library
)

# Disable exceptions
datascript_generate(
    TARGET safe_parsers
    SCHEMAS ...
    OPTIONS --cpp-exceptions=false
)

# Multiple options
datascript_generate(
    TARGET custom_parsers
    SCHEMAS ...
    OPTIONS
        --cpp-mode=library
        --cpp-enum-to-string=true
)
```

## Output Directory Structure

### With PRESERVE_PACKAGE_DIRS ON (default)

Given a schema file with `package formats.mz;`:

```
${OUTPUT_DIR}/
└── formats/
    └── mz/
        └── image_dos_header.hh
```

Include as: `#include <formats/mz/image_dos_header.hh>`

### With PRESERVE_PACKAGE_DIRS OFF

```
${OUTPUT_DIR}/
└── image_dos_header.hh
```

Include as: `#include <image_dos_header.hh>`

### Library Mode (--cpp-mode=library)

Library mode generates three files per schema:

```
${OUTPUT_DIR}/
└── formats/
    └── mz/
        ├── formats_mz_runtime.h   # Format-specific exceptions and helpers
        ├── formats_mz.h           # Public API (forward declarations, enums)
        └── formats_mz_impl.h      # Full implementation with read() methods
```

## Include Path Configuration

### Basic Configuration

By default, `INCLUDE_DIR` equals `OUTPUT_DIR`:

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS schemas/formats/mz.ds
    IMPORT_DIRS schemas
)
# Files at: ${CMAKE_CURRENT_BINARY_DIR}/datascript_gen/formats/mz/...
# Include:  #include <formats/mz/image_dos_header.hh>
```

### Custom Include Prefix

To add a project-specific prefix to includes:

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS schemas/formats/mz.ds
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen/myproject
    INCLUDE_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen
    IMPORT_DIRS schemas
)
# Files at: ${CMAKE_CURRENT_BINARY_DIR}/gen/myproject/formats/mz/...
# Include:  #include <myproject/formats/mz/image_dos_header.hh>
```

### Flat Includes

For simple projects without package namespacing:

```cmake
datascript_generate(
    TARGET parsers
    SCHEMAS schemas/protocol.ds
    PRESERVE_PACKAGE_DIRS OFF
)
# Files at: ${CMAKE_CURRENT_BINARY_DIR}/datascript_gen/message.hh
# Include:  #include <message.hh>
```

## Dependency Tracking

The CMake integration automatically tracks dependencies:

### Schema File Dependencies

Changes to any `.ds` file listed in `SCHEMAS` trigger regeneration.

### Import Dependencies

The `ds --print-imports` command is used at configure time to discover import dependencies. If schema A imports schema B, changes to B will trigger regeneration of A.

```
# schemas/protocol.ds
package protocol;
import common.types;  # Dependency tracked automatically
```

### Rebuild Behavior

- **Configure time:** Import dependencies are discovered
- **Build time:** Files are regenerated only when dependencies change
- **Clean build:** All schema files are processed

## Advanced Usage

### Multiple Generator Targets

Create separate targets for different configurations:

```cmake
# Main parsers with exceptions
datascript_generate(
    TARGET parsers
    SCHEMAS ${SCHEMA_FILES}
    IMPORT_DIRS ${SCHEMA_DIR}
)

# Safe parsers for embedded use
datascript_generate(
    TARGET parsers_safe
    SCHEMAS ${SCHEMA_FILES}
    IMPORT_DIRS ${SCHEMA_DIR}
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_safe
    OPTIONS --cpp-exceptions=false
)

# Library mode for introspection
datascript_generate(
    TARGET parsers_lib
    SCHEMAS ${SCHEMA_FILES}
    IMPORT_DIRS ${SCHEMA_DIR}
    OUTPUT_DIR ${CMAKE_CURRENT_BINARY_DIR}/gen_lib
    OPTIONS --cpp-mode=library
)
```

### Conditional Generation

```cmake
option(USE_LIBRARY_MODE "Generate library mode headers" OFF)

set(DS_OPTIONS "")
if(USE_LIBRARY_MODE)
    list(APPEND DS_OPTIONS --cpp-mode=library)
endif()

datascript_generate(
    TARGET parsers
    SCHEMAS ${SCHEMA_FILES}
    IMPORT_DIRS ${SCHEMA_DIR}
    OPTIONS ${DS_OPTIONS}
)
```

### Integration with External Projects

If DataScript is installed system-wide or in a custom location:

```cmake
# Find installed DataScript
find_program(DS_COMPILER ds REQUIRED)

# Manual generation (without datascript_generate)
add_custom_command(
    OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/gen/message.hh
    COMMAND ${DS_COMPILER}
            -t cpp
            -o ${CMAKE_CURRENT_BINARY_DIR}/gen
            -I ${CMAKE_CURRENT_SOURCE_DIR}/schemas
            ${CMAKE_CURRENT_SOURCE_DIR}/schemas/protocol.ds
    DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/schemas/protocol.ds
    COMMENT "Generating protocol parser"
)

add_custom_target(generate_parsers
    DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/gen/message.hh
)
```

### Cross-Compilation

When cross-compiling, you need a host `ds` compiler:

```cmake
if(CMAKE_CROSSCOMPILING)
    # Use pre-built host compiler
    find_program(DS_COMPILER ds REQUIRED
        PATHS /path/to/host/tools
        NO_DEFAULT_PATH
    )
else()
    # Build ds as part of the project
    FetchContent_MakeAvailable(DataScript)
endif()
```

## Troubleshooting

### "datascript_generate is not a command"

Ensure `FetchContent_MakeAvailable(DataScript)` is called before using `datascript_generate()`.

### Generated files not found

Check that:
1. The `IMPORT_DIRS` include all directories containing imported schemas
2. Schema file paths are correct (absolute or relative to `CMAKE_CURRENT_SOURCE_DIR`)
3. Package declarations match file paths

### Rebuild not triggered

If changes to imported schemas don't trigger rebuilds:
1. Re-run CMake configure to update dependencies
2. Check that import paths resolve correctly in `IMPORT_DIRS`

### Include path issues

Verify your include configuration:

```cmake
# Debug: Print the include directory
datascript_generate(TARGET parsers SCHEMAS ...)
get_target_property(INC_DIR parsers INTERFACE_INCLUDE_DIRECTORIES)
message(STATUS "Include dir: ${INC_DIR}")
```

### Build errors in generated code

1. Ensure you're using C++20 or later
2. Check that all required headers are available (`<variant>`, `<optional>`, etc.)
3. Verify schema syntax with `ds --verbose schema.ds`

## CLI Reference

The `ds` compiler supports several flags useful for CMake integration:

```bash
# Print import dependencies (one per line)
ds --print-imports -I schemas schema.ds

# Print output files (relative paths, one per line)
ds --print-outputs -t cpp -I schemas schema.ds

# Flat output (no package subdirectories)
ds --flat-output -t cpp -o outdir schema.ds

# Combine flags
ds --print-outputs --flat-output -t cpp --cpp-mode=library schema.ds
```

## See Also

- [C++ Code Generation Reference](CPP_CODE_GENERATION.md) - Details on generated C++ code
- [Language Guide](LANGUAGE_GUIDE.md) - DataScript language syntax
- [ABNF Grammar](datascript.abnf) - Formal grammar specification

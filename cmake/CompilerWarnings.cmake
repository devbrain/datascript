#
# Strict Compiler Warning Flags
#
# Provides strict warning configurations for MSVC, Clang, and GCC.
# These flags are applied as PRIVATE to avoid propagating to consumers.
#

# Function to apply strict warning flags to a target
function(target_set_strict_warnings target)
    if(MSVC)
        # MSVC strict warnings
        target_compile_options(${target} PRIVATE
            /W4                 # Warning level 4 (nearly all warnings)
            /WX                 # Treat warnings as errors
            /permissive-        # Strict C++ conformance
            /w14242             # Conversion from 'type1' to 'type2', possible loss of data
            /w14254             # Operator conversion, possible loss of data
            /w14263             # Member function does not override any base class virtual function
            /w14265             # Class has virtual functions but destructor is not virtual
            /w14287             # Unsigned/negative constant mismatch
            /w14289             # Loop control variable used outside loop
            /w14296             # Expression is always true/false
            /w14311             # Pointer truncation from 'type' to 'type'
            /w14545             # Expression before comma evaluates to a function
            /w14546             # Function call before comma missing argument list
            /w14547             # Operator before comma has no effect
            /w14549             # Operator before comma has no effect
            /w14555             # Expression has no effect
            /w14619             # Pragma warning: warning number does not exist
            /w14640             # Thread unsafe static member initialization
            /w14826             # Conversion is sign-extended
            /w14905             # Wide string literal cast to LPSTR
            /w14906             # String literal cast to LPWSTR
            /w14928             # Illegal copy-initialization
            /wd4201             # Disable: nonstandard extension used: nameless struct/union
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        # Clang strict warnings
        target_compile_options(${target} PRIVATE
            -Wall                       # Basic warnings
            -Wextra                     # Extra warnings
            -Wpedantic                  # Strict ISO C++ compliance
            -Werror                     # Treat warnings as errors
            -Wcast-align                # Pointer cast alignment issues
            -Wcast-qual                 # Cast removes qualifiers
            -Wconversion                # Implicit conversions that may alter value
            -Wdouble-promotion          # Float promoted to double
            -Wextra-semi                # Extra semicolons
            -Wformat=2                  # Printf format string issues
            -Wimplicit-fallthrough      # Missing break in switch
            -Wnon-virtual-dtor          # Non-virtual destructor in polymorphic class
            -Wnull-dereference          # Null pointer dereference
            -Wold-style-cast            # C-style casts
            -Woverloaded-virtual        # Overloaded virtual function
            -Wpointer-arith             # Pointer arithmetic issues
            -Wshadow                    # Variable shadowing
            -Wsign-conversion           # Sign conversion warnings
            -Wundef                     # Undefined macro in #if
            -Wunreachable-code          # Unreachable code
            -Wunused                    # Unused entities
            -Wzero-as-null-pointer-constant  # Zero used as null pointer
            -Wno-c++98-compat           # Disable C++98 compatibility warnings
            -Wno-c++98-compat-pedantic  # Disable C++98 pedantic compatibility
            -Wno-padded                 # Disable struct padding warnings
            -Wno-missing-field-initializers  # Allow partial struct initialization
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        # GCC strict warnings - common to C and C++
        target_compile_options(${target} PRIVATE
            -Wall                       # Basic warnings
            -Wextra                     # Extra warnings
            -Wpedantic                  # Strict ISO C++ compliance
            -Werror                     # Treat warnings as errors
            -Walloc-zero                # Zero-size allocation
            -Wcast-align                # Pointer cast alignment issues
            -Wcast-qual                 # Cast removes qualifiers
            -Wconversion                # Implicit conversions that may alter value
            -Wdouble-promotion          # Float promoted to double
            -Wduplicated-branches       # Duplicated branches in if-else
            -Wduplicated-cond           # Duplicated conditions
            -Wformat=2                  # Printf format string issues
            -Wformat-overflow=2         # Format string overflow
            -Wformat-signedness         # Format string sign mismatch
            -Wformat-truncation=2       # Format string truncation
            -Wimplicit-fallthrough=5    # Missing break in switch (strictest)
            -Wlogical-op                # Suspicious logical operations
            -Wnull-dereference          # Null pointer dereference
            -Wpointer-arith             # Pointer arithmetic issues
            -Wshadow                    # Variable shadowing
            -Wsign-conversion           # Sign conversion warnings
            -Wundef                     # Undefined macro in #if
            -Wunreachable-code          # Unreachable code
            -Wunused                    # Unused entities
            -Wno-missing-field-initializers  # Allow partial struct initialization
        )
        # GCC C++ only warnings (use generator expression)
        target_compile_options(${target} PRIVATE
            $<$<COMPILE_LANGUAGE:CXX>:-Wextra-semi>              # Extra semicolons
            $<$<COMPILE_LANGUAGE:CXX>:-Wnon-virtual-dtor>        # Non-virtual destructor
            $<$<COMPILE_LANGUAGE:CXX>:-Wold-style-cast>          # C-style casts
            $<$<COMPILE_LANGUAGE:CXX>:-Woverloaded-virtual>      # Overloaded virtual
            $<$<COMPILE_LANGUAGE:CXX>:-Wuseless-cast>            # Useless casts
            $<$<COMPILE_LANGUAGE:CXX>:-Wzero-as-null-pointer-constant>  # Zero as null
        )
    endif()
endfunction()

# Function to apply warnings to a target but without treating them as errors
# Useful for third-party code or generated code
function(target_set_warnings_no_error target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
            /WX-            # Explicitly disable warnings-as-errors
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-error      # Explicitly disable warnings-as-errors
        )
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wno-error      # Explicitly disable warnings-as-errors
        )
    endif()
endfunction()

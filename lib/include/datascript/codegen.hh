//
// Code Generation API
//
// Generate code from IR in various target languages
//

#pragma once

#include "ir.hh"
#include <string>
#include <stdexcept>

namespace datascript::codegen {

// ============================================================================
// Code Generation Exceptions
// ============================================================================

/**
 * Base exception for code generation errors.
 */
class codegen_error : public std::logic_error {
public:
    using std::logic_error::logic_error;
};

/**
 * Exception thrown when renderer encounters invalid/null IR data.
 *
 * This indicates a bug in either:
 * - IR construction (semantic analysis should catch this)
 * - IR validation (should validate before rendering)
 * - Renderer implementation (passing null pointers)
 */
class invalid_ir_error : public codegen_error {
public:
    invalid_ir_error(const std::string& what_arg)
        : codegen_error("Invalid IR passed to renderer: " + what_arg) {}
};

// ============================================================================
// C++ Code Generation Options
// ============================================================================

struct cpp_options {
    /// Error handling style
    enum error_style {
        exceptions_only,     ///< Generate only read() with exceptions
        results_only,        ///< Generate only read_safe() with results
        both                 ///< Generate both methods (default)
    };
    error_style error_handling = both;

    /// Code organization
    bool inline_all = true;              ///< Mark all functions inline
    bool generate_helpers = true;        ///< Generate helper functions
    std::string namespace_name = "generated";

    /// Features (Phase 2)
    bool generate_writer = false;        ///< Generate write() methods
    bool generate_size_calc = false;     ///< Generate size() methods

    /// Documentation
    bool include_source_refs = true;     ///< Include source location comments
    bool include_field_docs = false;     ///< Include field documentation comments
};

// ============================================================================
// C++ Code Generation
// ============================================================================

/// Generate self-contained C++ header from IR
/// @param ir IR module to generate from
/// @param opts Code generation options
/// @return C++ header file content
std::string generate_cpp_header(const ir::bundle& ir, const cpp_options& opts = {});

/// Generate C++ code for a single struct using command stream architecture
/// @param struct_def The struct definition from IR
/// @param use_exceptions If true, generate throwing code; if false, return errors
/// @return C++ code for the struct with read method
std::string generate_cpp_struct(const ir::struct_def& struct_def, bool use_exceptions = true);

} // namespace datascript::codegen

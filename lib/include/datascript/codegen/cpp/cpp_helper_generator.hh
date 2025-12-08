//
// C++ Helper Function Generator
//
// Generates runtime helper functions for binary parsing:
// - Exception classes (ConstraintViolation)
// - Binary reading helpers (read_uint8, read_uint16_le/be, etc.)
// - String reading helpers (exception and safe modes)
// - ReadResult template (for safe mode)
//

#pragma once

#include <datascript/codegen/cpp/cpp_writer_context.hh>
#include <datascript/codegen.hh>  // For cpp_options

namespace datascript::codegen {

/**
 * Generates C++ runtime helper functions for binary data parsing.
 *
 * This class encapsulates the generation of all runtime support functions
 * needed by generated DataScript parsers, including:
 * - Binary reading functions (uint8/16/32/64 with endianness support)
 * - Bounds checking for all reads
 * - String parsing (null-terminated)
 * - Safe-mode return types (ReadResult<T>)
 * - Exception classes for error reporting
 */
class CppHelperGenerator {
public:
    /**
     * Constructor.
     *
     * @param ctx Writer context for code generation
     * @param error_handling Error handling mode (exceptions/results/both)
     */
    explicit CppHelperGenerator(CppWriterContext& ctx, cpp_options::error_style error_handling);

    /**
     * Generate all helper functions in the proper order.
     *
     * Output order:
     * 1. Exception classes (if exceptions enabled)
     * 2. Binary reading helpers (always)
     * 3. Peek helpers (non-consuming reads, always)
     * 4. String reading helpers (mode-dependent)
     * 5. ReadResult template (if safe mode enabled)
     */
    void generate_all();

private:
    CppWriterContext& ctx_;
    cpp_options::error_style error_handling_;

    // Generation methods for each section
    void generate_exception_classes();
    void generate_binary_readers();
    void generate_peek_helpers();
    void generate_string_readers();
};

}  // namespace datascript::codegen

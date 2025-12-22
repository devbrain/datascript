#pragma once

#include <datascript/codegen/cpp/cpp_code_writer.hh>
#include <string>
#include <vector>
#include <variant>
#include <cstdint>
#include <type_traits>

namespace datascript::codegen {

/**
 * CppWriterContext - High-level code generation context
 *
 * Wraps CppCodeWriter and manages RAII block lifecycles across
 * command-stream architecture where start/end calls are separate.
 *
 * Usage:
 *   CppWriterContext ctx(output_stream);
 *   ctx.start_struct("Point", "A 2D point");
 *   ctx << "int x;" << endl;
 *   ctx << "int y;" << endl;
 *   ctx.end_struct();
 */
class CppWriterContext {
public:
    explicit CppWriterContext(std::ostream& output);
    ~CppWriterContext() = default;

    // ========================================================================
    // Streaming Operators (delegate to writer)
    // ========================================================================

    CppWriterContext& operator<<(const std::string& text);
    CppWriterContext& operator<<(const char* text);
    CppWriterContext& operator<<(char c);
    CppWriterContext& operator<<(int value);
    CppWriterContext& operator<<(size_t value);
    // Only enable uint64_t/int64_t overloads when they differ from size_t/int (macOS)
    template<typename T, std::enable_if_t<
        (std::is_same_v<T, uint64_t> && !std::is_same_v<uint64_t, size_t>) ||
        (std::is_same_v<T, int64_t> && !std::is_same_v<int64_t, int> && !std::is_same_v<int64_t, long>), int> = 0>
    CppWriterContext& operator<<(T value) {
        writer_ << value;
        return *this;
    }
    CppWriterContext& operator<<(CodeWriter& (*manip)(CodeWriter&));

    // ========================================================================
    // Struct/Class Management
    // ========================================================================

    void start_struct(const std::string& name, const std::string& doc_comment = "");
    void end_struct();

    void start_class(const std::string& name, const std::string& doc_comment = "");
    void end_class();

    // ========================================================================
    // Enum Management
    // ========================================================================

    void start_enum(const std::string& name, const std::string& base_type, const std::string& doc_comment = "");
    void end_enum();

    // ========================================================================
    // Namespace Management
    // ========================================================================

    void start_namespace(const std::string& name);
    void end_namespace();

    // ========================================================================
    // Method/Function Management
    // ========================================================================

    void start_function(const std::string& return_type,
                       const std::string& name,
                       const std::string& params);
    void end_function();

    void start_inline_function(const std::string& return_type,
                               const std::string& name,
                               const std::string& params);
    void end_inline_function();

    // ========================================================================
    // Control Flow - If/Else
    // ========================================================================

    void start_if(const std::string& condition);
    void start_else_if(const std::string& condition);
    void start_else();
    void end_if();  // Ends the entire if/else-if/else chain

    // ========================================================================
    // Control Flow - Loops
    // ========================================================================

    void start_for(const std::string& init,
                   const std::string& condition,
                   const std::string& increment);
    void end_for();

    void start_while(const std::string& condition);
    void end_while();

    // ========================================================================
    // Scope Management
    // ========================================================================

    void start_scope();
    void end_scope();

    // ========================================================================
    // Try/Catch Management
    // ========================================================================

    void start_try();
    void start_catch(const std::string& exception_type,
                     const std::string& var_name = "e");
    void end_try();  // Ends the entire try/catch chain

    // ========================================================================
    // Helper Methods
    // ========================================================================

    void write_blank_line();
    void write_comment(const std::string& comment);
    void write_block_comment(const std::vector<std::string>& lines);
    void write_pragma_once();
    void write_include(const std::string& header, bool system = false);

    // Get underlying writer for advanced operations
    CppCodeWriter& writer() { return writer_; }
    const CppCodeWriter& writer() const { return writer_; }

private:
    // ========================================================================
    // Block variant type - all possible RAII blocks
    // ========================================================================
    using Block = std::variant<
        StructBlock,
        ClassBlock,
        EnumBlock,
        NamespaceBlock,
        FunctionBlock,
        InlineFunctionBlock,
        IfBlock,
        ElseBlock,
        ForBlock,
        WhileBlock,
        ScopeBlock,
        TryBlock,
        CatchBlock
    >;

    CppCodeWriter writer_;

    // Stack of active RAII blocks (for nested structures)
    std::vector<Block> block_stack_;
};

}  // namespace datascript::codegen

//
// C++ Writer Context Implementation
//
// High-level code generation context that manages RAII block lifecycles.
//

#include <datascript/codegen/cpp/cpp_writer_context.hh>

namespace datascript::codegen {

// Bring manipulators into scope
using codegen::endl;
using codegen::blank;

// ============================================================================
// Construction
// ============================================================================

CppWriterContext::CppWriterContext(std::ostream& output)
    : writer_(output)
{
}

// ============================================================================
// Streaming Operators (delegate to writer)
// ============================================================================

CppWriterContext& CppWriterContext::operator<<(const std::string& text) {
    writer_ << text;
    return *this;
}

CppWriterContext& CppWriterContext::operator<<(const char* text) {
    writer_ << text;
    return *this;
}

CppWriterContext& CppWriterContext::operator<<(char c) {
    writer_ << c;
    return *this;
}

CppWriterContext& CppWriterContext::operator<<(int value) {
    writer_ << value;
    return *this;
}

CppWriterContext& CppWriterContext::operator<<(size_t value) {
    writer_ << value;
    return *this;
}

CppWriterContext& CppWriterContext::operator<<(CodeWriter& (*manip)(CodeWriter&)) {
    writer_ << manip;
    return *this;
}

// ============================================================================
// Struct/Class Management
// ============================================================================

void CppWriterContext::start_struct(const std::string& name, const std::string& doc_comment) {
    if (!doc_comment.empty()) {
        writer_ << "/**" << endl;
        writer_ << " * " << doc_comment << endl;
        writer_ << " */" << endl;
    }
    block_stack_.emplace_back(StructBlock(&writer_, name));
}

void CppWriterContext::end_struct() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor automatically writes "};"
    }
    writer_ << blank;
}

void CppWriterContext::start_class(const std::string& name, const std::string& doc_comment) {
    if (!doc_comment.empty()) {
        writer_ << "/**" << endl;
        writer_ << " * " << doc_comment << endl;
        writer_ << " */" << endl;
    }
    block_stack_.emplace_back(ClassBlock(&writer_, name));
}

void CppWriterContext::end_class() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor automatically writes "};"
    }
    writer_ << blank;
}

// ============================================================================
// Enum Management
// ============================================================================

void CppWriterContext::start_enum(const std::string& name, const std::string& base_type, const std::string& doc_comment) {
    if (!doc_comment.empty()) {
        writer_ << "/**" << endl;
        writer_ << " * " << doc_comment << endl;
        writer_ << " */" << endl;
    }
    block_stack_.emplace_back(EnumBlock(&writer_, name, base_type));
}

void CppWriterContext::end_enum() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor automatically writes "};"
    }
    writer_ << blank;
}

// ============================================================================
// Namespace Management
// ============================================================================

void CppWriterContext::start_namespace(const std::string& name) {
    block_stack_.emplace_back(NamespaceBlock(&writer_, name));
}

void CppWriterContext::end_namespace() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor writes closing with comment
    }
}

// ============================================================================
// Method/Function Management
// ============================================================================

void CppWriterContext::start_function(const std::string& return_type,
                                      const std::string& name,
                                      const std::string& params) {
    block_stack_.emplace_back(FunctionBlock(&writer_, return_type, name, params));
}

void CppWriterContext::end_function() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor writes closing "}"
    }
}

void CppWriterContext::start_inline_function(const std::string& return_type,
                                             const std::string& name,
                                             const std::string& params) {
    block_stack_.emplace_back(InlineFunctionBlock(&writer_, return_type, name, params));
}

void CppWriterContext::end_inline_function() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();  // Destructor writes closing "}"
    }
}

// ============================================================================
// Control Flow - If/Else
// ============================================================================

void CppWriterContext::start_if(const std::string& condition) {
    block_stack_.emplace_back(IfBlock(&writer_, condition));
}

void CppWriterContext::start_else_if(const std::string& condition) {
    if (!block_stack_.empty() && std::holds_alternative<IfBlock>(block_stack_.back())) {
        // Get the current IfBlock, call write_else_if, replace with new IfBlock
        IfBlock new_if = std::get<IfBlock>(block_stack_.back()).write_else_if(condition);
        block_stack_.pop_back();
        block_stack_.emplace_back(std::move(new_if));
    }
}

void CppWriterContext::start_else() {
    if (!block_stack_.empty() && std::holds_alternative<IfBlock>(block_stack_.back())) {
        // Get the current IfBlock, call write_else, replace with ElseBlock
        ElseBlock else_block = std::get<IfBlock>(block_stack_.back()).write_else();
        block_stack_.pop_back();
        block_stack_.emplace_back(std::move(else_block));
    }
}

void CppWriterContext::end_if() {
    // Pop the if or else block
    if (!block_stack_.empty()) {
        block_stack_.pop_back();
    }
}

// ============================================================================
// Control Flow - Loops
// ============================================================================

void CppWriterContext::start_for(const std::string& init,
                                 const std::string& condition,
                                 const std::string& increment) {
    block_stack_.emplace_back(ForBlock(&writer_, init, condition, increment));
}

void CppWriterContext::end_for() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();
    }
}

void CppWriterContext::start_while(const std::string& condition) {
    block_stack_.emplace_back(WhileBlock(&writer_, condition));
}

void CppWriterContext::end_while() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();
    }
}

// ============================================================================
// Scope Management
// ============================================================================

void CppWriterContext::start_scope() {
    block_stack_.emplace_back(ScopeBlock(&writer_));
}

void CppWriterContext::end_scope() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();
    }
}

// ============================================================================
// Try/Catch Management
// ============================================================================

void CppWriterContext::start_try() {
    block_stack_.emplace_back(TryBlock(&writer_));
}

void CppWriterContext::start_catch(const std::string& exception_type,
                                   const std::string& var_name) {
    if (!block_stack_.empty() && std::holds_alternative<TryBlock>(block_stack_.back())) {
        // Get TryBlock, call write_catch, replace with CatchBlock
        CatchBlock catch_block = std::get<TryBlock>(block_stack_.back()).write_catch(exception_type, var_name);
        block_stack_.pop_back();
        block_stack_.emplace_back(std::move(catch_block));
    }
}

void CppWriterContext::end_try() {
    if (!block_stack_.empty()) {
        block_stack_.pop_back();
    }
}

// ============================================================================
// Helper Methods
// ============================================================================

void CppWriterContext::write_blank_line() {
    writer_ << blank;
}

void CppWriterContext::write_comment(const std::string& comment) {
    writer_.write_comment(comment);
}

void CppWriterContext::write_block_comment(const std::vector<std::string>& lines) {
    writer_.write_block_comment(lines);
}

void CppWriterContext::write_pragma_once() {
    writer_.write_pragma_once();
}

void CppWriterContext::write_include(const std::string& header, bool system) {
    writer_.write_include(header, system);
}

}  // namespace datascript::codegen

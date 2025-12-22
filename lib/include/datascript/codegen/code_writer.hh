//
// Generic Code Writer - Language-Agnostic RAII-Based Code Generation
//
// This module provides a base class for generating code with automatic
// indentation management using RAII (Resource Acquisition Is Initialization).
// RAII blocks ensure proper closing of braces/blocks and prevent common bugs
// like mismatched indentation or forgotten closing braces.
//

#pragma once

#include <ostream>
#include <string>
#include <memory>
#include <cstdint>
#include <type_traits>

namespace datascript::codegen {

// Forward declarations for RAII block classes
class IfBlock;
class ElseBlock;
class ForBlock;
class WhileBlock;
class TryBlock;
class CatchBlock;
class ScopeBlock;

// ============================================================================
// CodeWriter - Base class for language-agnostic code generation
// ============================================================================

class CodeWriter {
public:
    explicit CodeWriter(std::ostream& output);
    virtual ~CodeWriter() = default;

    // Non-copyable (output stream reference)
    CodeWriter(const CodeWriter&) = delete;
    CodeWriter& operator=(const CodeWriter&) = delete;

    // ========================================================================
    // Basic Output Methods
    // ========================================================================

    // Write a line with current indentation
    virtual void write_line(const std::string& line);

    // Write raw text without indentation or newline
    virtual void write_raw(const std::string& text);

    // Write a blank line
    virtual void write_blank_line();

    // ========================================================================
    // RAII Block Generators - Return guard objects
    // ========================================================================

    // Create an if-block with automatic brace/indent management
    IfBlock write_if(const std::string& condition);

    // Create a for-loop block
    ForBlock write_for(const std::string& init,
                       const std::string& condition,
                       const std::string& increment);

    // Create a while-loop block
    WhileBlock write_while(const std::string& condition);

    // Create a try-block
    TryBlock write_try();

    // Create a generic scope block (just braces)
    ScopeBlock write_scope();

    // ========================================================================
    // Indentation Management (for RAII blocks)
    // ========================================================================

    void indent();
    void unindent();
    size_t current_indent_level() const { return indent_level_; }

    // ========================================================================
    // Streaming Operators (idiomatic C++ output)
    // ========================================================================

    // Stream text (accumulated until endl or write_line)
    CodeWriter& operator<<(const std::string& text);
    CodeWriter& operator<<(const char* text);
    CodeWriter& operator<<(char c);
    CodeWriter& operator<<(int value);
    CodeWriter& operator<<(size_t value);
    // Only enable uint64_t/int64_t overloads when they differ from size_t/int (macOS)
    template<typename T, std::enable_if_t<
        (std::is_same_v<T, uint64_t> && !std::is_same_v<uint64_t, size_t>) ||
        (std::is_same_v<T, int64_t> && !std::is_same_v<int64_t, int> && !std::is_same_v<int64_t, long>), int> = 0>
    CodeWriter& operator<<(T value) {
        return *this << std::to_string(value);
    }

    // Stream manipulator support (for custom endl, indent, etc.)
    CodeWriter& operator<<(CodeWriter& (*manip)(CodeWriter&));

    // ========================================================================
    // Configuration
    // ========================================================================

    void set_indent_string(const std::string& indent);
    const std::string& get_indent_string() const { return indent_string_; }

    // ========================================================================
    // Friend Functions (for manipulator access)
    // ========================================================================

    friend CodeWriter& endl(CodeWriter& writer);
    friend CodeWriter& indent(CodeWriter& writer);
    friend CodeWriter& unindent(CodeWriter& writer);
    friend CodeWriter& blank(CodeWriter& writer);

protected:
    // Access for derived classes
    std::ostream& output_;

    // Indentation state
    size_t indent_level_;
    std::string indent_string_;  // e.g., "    " (4 spaces) or "\t"
    std::string cached_indent_;  // Cached string of current indentation

    // Line buffer for streaming operator
    std::string line_buffer_;

    // Update cached indentation string
    void update_cached_indent();

    // Write the opening and closing syntax for blocks
    // Override in derived classes for language-specific syntax
    virtual void write_block_open(const std::string& header);
    virtual void write_block_close();
};

// ============================================================================
// Custom Stream Manipulators
// ============================================================================

// Write buffered line with indentation and newline (like std::endl)
CodeWriter& endl(CodeWriter& writer);

// Increase indentation level
CodeWriter& indent(CodeWriter& writer);

// Decrease indentation level
CodeWriter& unindent(CodeWriter& writer);

// Write a blank line
CodeWriter& blank(CodeWriter& writer);

// ============================================================================
// StreamableBlock - CRTP base for blocks with streaming operators
// ============================================================================

template<typename Derived>
class StreamableBlock {
public:
    // Streaming operators - delegate to writer and return derived type
    Derived& operator<<(const std::string& text) {
        writer_->operator<<(text);
        return static_cast<Derived&>(*this);
    }

    Derived& operator<<(const char* text) {
        writer_->operator<<(text);
        return static_cast<Derived&>(*this);
    }

    Derived& operator<<(char c) {
        writer_->operator<<(c);
        return static_cast<Derived&>(*this);
    }

    Derived& operator<<(int value) {
        writer_->operator<<(value);
        return static_cast<Derived&>(*this);
    }

    Derived& operator<<(size_t value) {
        writer_->operator<<(value);
        return static_cast<Derived&>(*this);
    }

    Derived& operator<<(CodeWriter& (*manip)(CodeWriter&)) {
        writer_->operator<<(manip);
        return static_cast<Derived&>(*this);
    }

protected:
    CodeWriter* writer_;

    explicit StreamableBlock(CodeWriter* writer) : writer_(writer) {}
};

// ============================================================================
// IfBlock - RAII guard for if-statements
// ============================================================================

class IfBlock : public StreamableBlock<IfBlock> {
public:
    IfBlock(CodeWriter* writer, const std::string& condition);
    ~IfBlock();

    // Non-copyable, movable
    IfBlock(const IfBlock&) = delete;
    IfBlock& operator=(const IfBlock&) = delete;
    IfBlock(IfBlock&& other) noexcept;
    IfBlock& operator=(IfBlock&& other) noexcept;

    // Chain an else-if block
    IfBlock write_else_if(const std::string& condition);

    // Chain an else block
    ElseBlock write_else();

private:
    friend class CodeWriter;

    // Private constructor for else-if (already opened)
    IfBlock(CodeWriter* writer, bool already_opened);

    bool has_else_;
    bool already_closed_;
};

// ============================================================================
// ElseBlock - RAII guard for else-statements
// ============================================================================

class ElseBlock : public StreamableBlock<ElseBlock> {
public:
    explicit ElseBlock(CodeWriter* writer);
    ~ElseBlock();

    // Non-copyable, movable
    ElseBlock(const ElseBlock&) = delete;
    ElseBlock& operator=(const ElseBlock&) = delete;
    ElseBlock(ElseBlock&& other) noexcept;
    ElseBlock& operator=(ElseBlock&& other) noexcept;

private:
    bool already_closed_;
};

// ============================================================================
// ForBlock - RAII guard for for-loops
// ============================================================================

class ForBlock : public StreamableBlock<ForBlock> {
public:
    ForBlock(CodeWriter* writer,
             const std::string& init,
             const std::string& condition,
             const std::string& increment);
    ~ForBlock();

    // Non-copyable, movable
    ForBlock(const ForBlock&) = delete;
    ForBlock& operator=(const ForBlock&) = delete;
    ForBlock(ForBlock&& other) noexcept;
    ForBlock& operator=(ForBlock&& other) noexcept;

private:
    bool already_closed_;
};

// ============================================================================
// WhileBlock - RAII guard for while-loops
// ============================================================================

class WhileBlock : public StreamableBlock<WhileBlock> {
public:
    WhileBlock(CodeWriter* writer, const std::string& condition);
    ~WhileBlock();

    // Non-copyable, movable
    WhileBlock(const WhileBlock&) = delete;
    WhileBlock& operator=(const WhileBlock&) = delete;
    WhileBlock(WhileBlock&& other) noexcept;
    WhileBlock& operator=(WhileBlock&& other) noexcept;

private:
    bool already_closed_;
};

// ============================================================================
// TryBlock - RAII guard for try-catch blocks
// ============================================================================

class TryBlock : public StreamableBlock<TryBlock> {
public:
    explicit TryBlock(CodeWriter* writer);
    ~TryBlock();

    // Non-copyable, movable
    TryBlock(const TryBlock&) = delete;
    TryBlock& operator=(const TryBlock&) = delete;
    TryBlock(TryBlock&& other) noexcept;
    TryBlock& operator=(TryBlock&& other) noexcept;

    // Add a catch block
    CatchBlock write_catch(const std::string& exception_type,
                          const std::string& var_name = "e");

private:
    bool has_catch_;
    bool already_closed_;
};

// ============================================================================
// CatchBlock - RAII guard for catch statements
// ============================================================================

class CatchBlock : public StreamableBlock<CatchBlock> {
public:
    CatchBlock(CodeWriter* writer);
    ~CatchBlock();

    // Non-copyable, movable
    CatchBlock(const CatchBlock&) = delete;
    CatchBlock& operator=(const CatchBlock&) = delete;
    CatchBlock(CatchBlock&& other) noexcept;
    CatchBlock& operator=(CatchBlock&& other) noexcept;

    // Chain another catch block
    CatchBlock write_catch(const std::string& exception_type,
                          const std::string& var_name = "e");

private:
    bool already_closed_;
};

// ============================================================================
// ScopeBlock - RAII guard for generic braced scopes
// ============================================================================

class ScopeBlock : public StreamableBlock<ScopeBlock> {
public:
    explicit ScopeBlock(CodeWriter* writer);
    ~ScopeBlock();

    // Non-copyable, movable
    ScopeBlock(const ScopeBlock&) = delete;
    ScopeBlock& operator=(const ScopeBlock&) = delete;
    ScopeBlock(ScopeBlock&& other) noexcept;
    ScopeBlock& operator=(ScopeBlock&& other) noexcept;

private:
    bool already_closed_;
};

}  // namespace datascript::codegen

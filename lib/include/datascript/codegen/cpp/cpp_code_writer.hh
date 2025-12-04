//
// C++ Code Writer - C++-Specific Code Generation
//
// Extends the generic CodeWriter with C++-specific features:
// - Struct/class definitions with access modifiers
// - Function definitions
// - Namespace blocks
// - Include directives
// - Pragma once
//

#pragma once

#include <datascript/codegen/code_writer.hh>
#include <string>
#include <vector>

namespace datascript::codegen {

// Forward declarations
class StructBlock;
class ClassBlock;
class FunctionBlock;
class InlineFunctionBlock;
class NamespaceBlock;

// ============================================================================
// CppCodeWriter - C++-Specific Code Generation
// ============================================================================

class CppCodeWriter : public CodeWriter {
public:
    explicit CppCodeWriter(std::ostream& output);
    virtual ~CppCodeWriter() = default;

    // ========================================================================
    // C++-Specific Blocks
    // ========================================================================

    // Create a struct definition block
    StructBlock write_struct(const std::string& name);

    // Create a class definition block
    ClassBlock write_class(const std::string& name);

    // Create a function definition block
    FunctionBlock write_function(const std::string& return_type,
                                  const std::string& name,
                                  const std::string& params);

    // Create an inline function definition block
    InlineFunctionBlock write_inline_function(const std::string& return_type,
                                               const std::string& name,
                                               const std::string& params);

    // Create a namespace block
    NamespaceBlock write_namespace(const std::string& name);

    // ========================================================================
    // C++-Specific Output Helpers
    // ========================================================================

    // Write #pragma once
    void write_pragma_once();

    // Write #include directive
    void write_include(const std::string& header, bool system = false);

    // Write multiple includes
    void write_includes(const std::vector<std::string>& headers, bool system = false);

    // Write a comment line
    void write_comment(const std::string& comment);

    // Write a multi-line block comment
    void write_block_comment(const std::vector<std::string>& lines);

    // Write a using statement
    void write_using(const std::string& alias, const std::string& type);

    // Write a typedef
    void write_typedef(const std::string& existing_type, const std::string& new_name);
};

// ============================================================================
// StructBlock - RAII guard for struct definitions
// ============================================================================

class StructBlock : public StreamableBlock<StructBlock> {
public:
    StructBlock(CppCodeWriter* writer, const std::string& name);
    ~StructBlock();

    // Non-copyable, movable
    StructBlock(const StructBlock&) = delete;
    StructBlock& operator=(const StructBlock&) = delete;
    StructBlock(StructBlock&& other) noexcept;
    StructBlock& operator=(StructBlock&& other) noexcept;

    // Access modifiers
    void write_public();
    void write_private();
    void write_protected();

private:
    CppCodeWriter* cpp_writer_;  // Specific writer for access modifiers
    std::string name_;
    bool already_closed_;
};

// ============================================================================
// EnumBlock - RAII guard for enum definitions
// ============================================================================

class EnumBlock : public StreamableBlock<EnumBlock> {
public:
    EnumBlock(CppCodeWriter* writer, const std::string& name, const std::string& base_type);
    ~EnumBlock();

    // Non-copyable, movable
    EnumBlock(const EnumBlock&) = delete;
    EnumBlock& operator=(const EnumBlock&) = delete;
    EnumBlock(EnumBlock&& other) noexcept;
    EnumBlock& operator=(EnumBlock&& other) noexcept;

private:
    CppCodeWriter* cpp_writer_;
    std::string name_;
    std::string base_type_;
    bool already_closed_;
};

// ============================================================================
// ClassBlock - RAII guard for class definitions
// ============================================================================

class ClassBlock : public StreamableBlock<ClassBlock> {
public:
    ClassBlock(CppCodeWriter* writer, const std::string& name);
    ~ClassBlock();

    // Non-copyable, movable
    ClassBlock(const ClassBlock&) = delete;
    ClassBlock& operator=(const ClassBlock&) = delete;
    ClassBlock(ClassBlock&& other) noexcept;
    ClassBlock& operator=(ClassBlock&& other) noexcept;

    // Access modifiers
    void write_public();
    void write_private();
    void write_protected();

private:
    CppCodeWriter* cpp_writer_;  // Specific writer for access modifiers
    std::string name_;
    bool already_closed_;
};

// ============================================================================
// FunctionBlock - RAII guard for function definitions
// ============================================================================

class FunctionBlock : public StreamableBlock<FunctionBlock> {
public:
    FunctionBlock(CppCodeWriter* writer,
                  const std::string& return_type,
                  const std::string& name,
                  const std::string& params);
    ~FunctionBlock();

    // Non-copyable, movable
    FunctionBlock(const FunctionBlock&) = delete;
    FunctionBlock& operator=(const FunctionBlock&) = delete;
    FunctionBlock(FunctionBlock&& other) noexcept;
    FunctionBlock& operator=(FunctionBlock&& other) noexcept;

private:
    bool already_closed_;
};

// ============================================================================
// InlineFunctionBlock - RAII guard for inline function definitions
// ============================================================================

class InlineFunctionBlock : public StreamableBlock<InlineFunctionBlock> {
public:
    InlineFunctionBlock(CppCodeWriter* writer,
                       const std::string& return_type,
                       const std::string& name,
                       const std::string& params);
    ~InlineFunctionBlock();

    // Non-copyable, movable
    InlineFunctionBlock(const InlineFunctionBlock&) = delete;
    InlineFunctionBlock& operator=(const InlineFunctionBlock&) = delete;
    InlineFunctionBlock(InlineFunctionBlock&& other) noexcept;
    InlineFunctionBlock& operator=(InlineFunctionBlock&& other) noexcept;

private:
    bool already_closed_;
};

// ============================================================================
// NamespaceBlock - RAII guard for namespace definitions
// ============================================================================

class NamespaceBlock : public StreamableBlock<NamespaceBlock> {
public:
    NamespaceBlock(CppCodeWriter* writer, const std::string& name);
    ~NamespaceBlock();

    // Non-copyable, movable
    NamespaceBlock(const NamespaceBlock&) = delete;
    NamespaceBlock& operator=(const NamespaceBlock&) = delete;
    NamespaceBlock(NamespaceBlock&& other) noexcept;
    NamespaceBlock& operator=(NamespaceBlock&& other) noexcept;

private:
    std::string name_;
    bool already_closed_;
};

}  // namespace datascript::codegen

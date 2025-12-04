//
// C++ Code Writer Implementation
//

#include <datascript/codegen/cpp/cpp_code_writer.hh>

namespace datascript::codegen {

// ============================================================================
// CppCodeWriter Implementation
// ============================================================================

CppCodeWriter::CppCodeWriter(std::ostream& output)
    : CodeWriter(output)
{
}

// C++-Specific Block Factory Methods
StructBlock CppCodeWriter::write_struct(const std::string& name) {
    return StructBlock(this, name);
}

ClassBlock CppCodeWriter::write_class(const std::string& name) {
    return ClassBlock(this, name);
}

FunctionBlock CppCodeWriter::write_function(const std::string& return_type,
                                             const std::string& name,
                                             const std::string& params) {
    return FunctionBlock(this, return_type, name, params);
}

InlineFunctionBlock CppCodeWriter::write_inline_function(const std::string& return_type,
                                                          const std::string& name,
                                                          const std::string& params) {
    return InlineFunctionBlock(this, return_type, name, params);
}

NamespaceBlock CppCodeWriter::write_namespace(const std::string& name) {
    return NamespaceBlock(this, name);
}

// C++-Specific Output Helpers
void CppCodeWriter::write_pragma_once() {
    write_line("#pragma once");
}

void CppCodeWriter::write_include(const std::string& header, bool system) {
    if (system) {
        write_line("#include <" + header + ">");
    } else {
        write_line("#include \"" + header + "\"");
    }
}

void CppCodeWriter::write_includes(const std::vector<std::string>& headers, bool system) {
    for (const auto& header : headers) {
        write_include(header, system);
    }
}

void CppCodeWriter::write_comment(const std::string& comment) {
    write_line("// " + comment);
}

void CppCodeWriter::write_block_comment(const std::vector<std::string>& lines) {
    write_line("/*");
    for (const auto& line : lines) {
        write_line(" * " + line);
    }
    write_line(" */");
}

void CppCodeWriter::write_using(const std::string& alias, const std::string& type) {
    write_line("using " + alias + " = " + type + ";");
}

void CppCodeWriter::write_typedef(const std::string& existing_type, const std::string& new_name) {
    write_line("typedef " + existing_type + " " + new_name + ";");
}

// ============================================================================
// StructBlock Implementation
// ============================================================================

StructBlock::StructBlock(CppCodeWriter* writer, const std::string& name)
    : StreamableBlock<StructBlock>(writer),
      cpp_writer_(writer),
      name_(name),
      already_closed_(false)
{
    cpp_writer_->write_line("struct " + name + " {");
    cpp_writer_->indent();
}

StructBlock::~StructBlock() {
    if (writer_ && !already_closed_) {
        cpp_writer_->unindent();
        cpp_writer_->write_line("};");  // Struct ends with semicolon
    }
}

StructBlock::StructBlock(StructBlock&& other) noexcept
    : StreamableBlock<StructBlock>(other.writer_),
      cpp_writer_(other.cpp_writer_),
      name_(std::move(other.name_)),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
    other.cpp_writer_ = nullptr;
}

StructBlock& StructBlock::operator=(StructBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            cpp_writer_->unindent();
            cpp_writer_->write_line("};");
        }
        writer_ = other.writer_;
        cpp_writer_ = other.cpp_writer_;
        name_ = std::move(other.name_);
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
        other.cpp_writer_ = nullptr;
    }
    return *this;
}

void StructBlock::write_public() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("public:");
    cpp_writer_->indent();
}

void StructBlock::write_private() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("private:");
    cpp_writer_->indent();
}

void StructBlock::write_protected() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("protected:");
    cpp_writer_->indent();
}

// ============================================================================
// EnumBlock Implementation
// ============================================================================

EnumBlock::EnumBlock(CppCodeWriter* writer, const std::string& name, const std::string& base_type)
    : StreamableBlock<EnumBlock>(writer),
      cpp_writer_(writer),
      name_(name),
      base_type_(base_type),
      already_closed_(false)
{
    cpp_writer_->write_line("enum class " + name + " : " + base_type + " {");
    cpp_writer_->indent();
}

EnumBlock::~EnumBlock() {
    if (writer_ && !already_closed_) {
        cpp_writer_->unindent();
        cpp_writer_->write_line("};");  // Enum ends with semicolon
    }
}

EnumBlock::EnumBlock(EnumBlock&& other) noexcept
    : StreamableBlock<EnumBlock>(other.writer_),
      cpp_writer_(other.cpp_writer_),
      name_(std::move(other.name_)),
      base_type_(std::move(other.base_type_)),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
    other.cpp_writer_ = nullptr;
}

EnumBlock& EnumBlock::operator=(EnumBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            cpp_writer_->unindent();
            cpp_writer_->write_line("};");
        }
        writer_ = other.writer_;
        cpp_writer_ = other.cpp_writer_;
        name_ = std::move(other.name_);
        base_type_ = std::move(other.base_type_);
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
        other.cpp_writer_ = nullptr;
    }
    return *this;
}

// ============================================================================
// ClassBlock Implementation
// ============================================================================

ClassBlock::ClassBlock(CppCodeWriter* writer, const std::string& name)
    : StreamableBlock<ClassBlock>(writer),
      cpp_writer_(writer),
      name_(name),
      already_closed_(false)
{
    cpp_writer_->write_line("class " + name + " {");
    cpp_writer_->indent();
}

ClassBlock::~ClassBlock() {
    if (writer_ && !already_closed_) {
        cpp_writer_->unindent();
        cpp_writer_->write_line("};");  // Class ends with semicolon
    }
}

ClassBlock::ClassBlock(ClassBlock&& other) noexcept
    : StreamableBlock<ClassBlock>(other.writer_),
      cpp_writer_(other.cpp_writer_),
      name_(std::move(other.name_)),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
    other.cpp_writer_ = nullptr;
}

ClassBlock& ClassBlock::operator=(ClassBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            cpp_writer_->unindent();
            cpp_writer_->write_line("};");
        }
        writer_ = other.writer_;
        cpp_writer_ = other.cpp_writer_;
        name_ = std::move(other.name_);
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
        other.cpp_writer_ = nullptr;
    }
    return *this;
}

void ClassBlock::write_public() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("public:");
    cpp_writer_->indent();
}

void ClassBlock::write_private() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("private:");
    cpp_writer_->indent();
}

void ClassBlock::write_protected() {
    cpp_writer_->unindent();
    cpp_writer_->write_line("protected:");
    cpp_writer_->indent();
}

// ============================================================================
// FunctionBlock Implementation
// ============================================================================

FunctionBlock::FunctionBlock(CppCodeWriter* writer,
                             const std::string& return_type,
                             const std::string& name,
                             const std::string& params)
    : StreamableBlock<FunctionBlock>(writer),
      already_closed_(false)
{
    writer_->write_line(return_type + " " + name + "(" + params + ") {");
    writer_->indent();
}

FunctionBlock::~FunctionBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");  // No semicolon for functions
    }
}

FunctionBlock::FunctionBlock(FunctionBlock&& other) noexcept
    : StreamableBlock<FunctionBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

FunctionBlock& FunctionBlock::operator=(FunctionBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            writer_->unindent();
            writer_->write_line("}");
        }
        writer_ = other.writer_;
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
    }
    return *this;
}

// ============================================================================
// InlineFunctionBlock Implementation
// ============================================================================

InlineFunctionBlock::InlineFunctionBlock(CppCodeWriter* writer,
                                         const std::string& return_type,
                                         const std::string& name,
                                         const std::string& params)
    : StreamableBlock<InlineFunctionBlock>(writer),
      already_closed_(false)
{
    writer_->write_line("inline " + return_type + " " + name + "(" + params + ") {");
    writer_->indent();
}

InlineFunctionBlock::~InlineFunctionBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");  // No semicolon for functions
    }
}

InlineFunctionBlock::InlineFunctionBlock(InlineFunctionBlock&& other) noexcept
    : StreamableBlock<InlineFunctionBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

InlineFunctionBlock& InlineFunctionBlock::operator=(InlineFunctionBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            writer_->unindent();
            writer_->write_line("}");
        }
        writer_ = other.writer_;
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
    }
    return *this;
}

// ============================================================================
// NamespaceBlock Implementation
// ============================================================================

NamespaceBlock::NamespaceBlock(CppCodeWriter* writer, const std::string& name)
    : StreamableBlock<NamespaceBlock>(writer),
      name_(name),
      already_closed_(false)
{
    writer_->write_line("namespace " + name + " {");
    writer_->write_blank_line();
    writer_->indent();
}

NamespaceBlock::~NamespaceBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_blank_line();
        writer_->write_line("}  // namespace " + name_);
    }
}

NamespaceBlock::NamespaceBlock(NamespaceBlock&& other) noexcept
    : StreamableBlock<NamespaceBlock>(other.writer_),
      name_(std::move(other.name_)),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

NamespaceBlock& NamespaceBlock::operator=(NamespaceBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_) {
            writer_->unindent();
            writer_->write_blank_line();
            writer_->write_line("}  // namespace " + name_);
        }
        writer_ = other.writer_;
        name_ = std::move(other.name_);
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
    }
    return *this;
}

}  // namespace datascript::codegen

//
// Generic Code Writer Implementation
//

#include <datascript/codegen/code_writer.hh>

namespace datascript::codegen {

// ============================================================================
// CodeWriter Implementation
// ============================================================================

CodeWriter::CodeWriter(std::ostream& output)
    : output_(output),
      indent_level_(0),
      indent_string_("    "),  // 4 spaces default
      cached_indent_(),
      line_buffer_()
{
}

void CodeWriter::write_line(const std::string& line) {
    if (!line.empty()) {
        output_ << cached_indent_ << line;
    }
    output_ << '\n';
}

void CodeWriter::write_raw(const std::string& text) {
    output_ << text;
}

void CodeWriter::write_blank_line() {
    output_ << '\n';
}

void CodeWriter::indent() {
    indent_level_++;
    update_cached_indent();
}

void CodeWriter::unindent() {
    if (indent_level_ > 0) {
        indent_level_--;
        update_cached_indent();
    }
}

void CodeWriter::set_indent_string(const std::string& indent) {
    indent_string_ = indent;
    update_cached_indent();
}

void CodeWriter::update_cached_indent() {
    cached_indent_.clear();
    for (size_t i = 0; i < indent_level_; ++i) {
        cached_indent_ += indent_string_;
    }
}

void CodeWriter::write_block_open(const std::string& header) {
    write_line(header + " {");
}

void CodeWriter::write_block_close() {
    write_line("}");
}

// RAII Block factory methods
IfBlock CodeWriter::write_if(const std::string& condition) {
    return IfBlock(this, condition);
}

ForBlock CodeWriter::write_for(const std::string& init,
                                const std::string& condition,
                                const std::string& increment) {
    return ForBlock(this, init, condition, increment);
}

WhileBlock CodeWriter::write_while(const std::string& condition) {
    return WhileBlock(this, condition);
}

TryBlock CodeWriter::write_try() {
    return TryBlock(this);
}

ScopeBlock CodeWriter::write_scope() {
    return ScopeBlock(this);
}

// ============================================================================
// Streaming Operators Implementation
// ============================================================================

CodeWriter& CodeWriter::operator<<(const std::string& text) {
    line_buffer_ += text;
    return *this;
}

CodeWriter& CodeWriter::operator<<(const char* text) {
    if (text) {
        line_buffer_ += text;
    }
    return *this;
}

CodeWriter& CodeWriter::operator<<(char c) {
    line_buffer_ += c;
    return *this;
}

CodeWriter& CodeWriter::operator<<(int value) {
    line_buffer_ += std::to_string(value);
    return *this;
}

CodeWriter& CodeWriter::operator<<(size_t value) {
    line_buffer_ += std::to_string(value);
    return *this;
}

CodeWriter& CodeWriter::operator<<(CodeWriter& (*manip)(CodeWriter&)) {
    return manip(*this);
}

// ============================================================================
// Custom Manipulators Implementation
// ============================================================================

CodeWriter& endl(CodeWriter& writer) {
    writer.write_line(writer.line_buffer_);
    writer.line_buffer_.clear();
    return writer;
}

CodeWriter& indent(CodeWriter& writer) {
    writer.indent();
    return writer;
}

CodeWriter& unindent(CodeWriter& writer) {
    writer.unindent();
    return writer;
}

CodeWriter& blank(CodeWriter& writer) {
    writer.write_blank_line();
    return writer;
}

// ============================================================================
// IfBlock Implementation
// ============================================================================

IfBlock::IfBlock(CodeWriter* writer, const std::string& condition)
    : StreamableBlock<IfBlock>(writer),
      has_else_(false),
      already_closed_(false)
{
    writer_->write_line("if (" + condition + ") {");
    writer_->indent();
}

IfBlock::IfBlock(CodeWriter* writer, bool already_opened)
    : StreamableBlock<IfBlock>(writer),
      has_else_(false),
      already_closed_(false)
{
    // Used by write_else_if - block already opened
    (void)already_opened;
}

IfBlock::~IfBlock() {
    if (writer_ && !already_closed_ && !has_else_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

IfBlock::IfBlock(IfBlock&& other) noexcept
    : StreamableBlock<IfBlock>(other.writer_),
      has_else_(other.has_else_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

IfBlock& IfBlock::operator=(IfBlock&& other) noexcept {
    if (this != &other) {
        // Close current block if needed
        if (writer_ && !already_closed_ && !has_else_) {
            writer_->unindent();
            writer_->write_line("}");
        }
        writer_ = other.writer_;
        has_else_ = other.has_else_;
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
    }
    return *this;
}

IfBlock IfBlock::write_else_if(const std::string& condition) {
    writer_->unindent();
    writer_->write_line("} else if (" + condition + ") {");
    writer_->indent();
    has_else_ = true;
    return IfBlock(writer_, true);
}

ElseBlock IfBlock::write_else() {
    writer_->unindent();
    writer_->write_line("} else {");
    writer_->indent();
    has_else_ = true;
    return ElseBlock(writer_);
}

// ============================================================================
// ElseBlock Implementation
// ============================================================================

ElseBlock::ElseBlock(CodeWriter* writer)
    : StreamableBlock<ElseBlock>(writer),
      already_closed_(false)
{
}

ElseBlock::~ElseBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

ElseBlock::ElseBlock(ElseBlock&& other) noexcept
    : StreamableBlock<ElseBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

ElseBlock& ElseBlock::operator=(ElseBlock&& other) noexcept {
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
// ForBlock Implementation
// ============================================================================

ForBlock::ForBlock(CodeWriter* writer,
                   const std::string& init,
                   const std::string& condition,
                   const std::string& increment)
    : StreamableBlock<ForBlock>(writer),
      already_closed_(false)
{
    writer_->write_line("for (" + init + "; " + condition + "; " + increment + ") {");
    writer_->indent();
}

ForBlock::~ForBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

ForBlock::ForBlock(ForBlock&& other) noexcept
    : StreamableBlock<ForBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

ForBlock& ForBlock::operator=(ForBlock&& other) noexcept {
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
// WhileBlock Implementation
// ============================================================================

WhileBlock::WhileBlock(CodeWriter* writer, const std::string& condition)
    : StreamableBlock<WhileBlock>(writer),
      already_closed_(false)
{
    writer_->write_line("while (" + condition + ") {");
    writer_->indent();
}

WhileBlock::~WhileBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

WhileBlock::WhileBlock(WhileBlock&& other) noexcept
    : StreamableBlock<WhileBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

WhileBlock& WhileBlock::operator=(WhileBlock&& other) noexcept {
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
// TryBlock Implementation
// ============================================================================

TryBlock::TryBlock(CodeWriter* writer)
    : StreamableBlock<TryBlock>(writer),
      has_catch_(false),
      already_closed_(false)
{
    writer_->write_line("try {");
    writer_->indent();
}

TryBlock::~TryBlock() {
    if (writer_ && !already_closed_ && !has_catch_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

TryBlock::TryBlock(TryBlock&& other) noexcept
    : StreamableBlock<TryBlock>(other.writer_),
      has_catch_(other.has_catch_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

TryBlock& TryBlock::operator=(TryBlock&& other) noexcept {
    if (this != &other) {
        if (writer_ && !already_closed_ && !has_catch_) {
            writer_->unindent();
            writer_->write_line("}");
        }
        writer_ = other.writer_;
        has_catch_ = other.has_catch_;
        already_closed_ = other.already_closed_;
        other.writer_ = nullptr;
    }
    return *this;
}

CatchBlock TryBlock::write_catch(const std::string& exception_type,
                                  const std::string& var_name) {
    writer_->unindent();
    std::string catch_clause = "} catch (" + exception_type;
    if (!var_name.empty()) {
        catch_clause += " " + var_name;
    }
    catch_clause += ") {";
    writer_->write_line(catch_clause);
    writer_->indent();
    has_catch_ = true;
    return CatchBlock(writer_);
}

// ============================================================================
// CatchBlock Implementation
// ============================================================================

CatchBlock::CatchBlock(CodeWriter* writer)
    : StreamableBlock<CatchBlock>(writer),
      already_closed_(false)
{
}

CatchBlock::~CatchBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

CatchBlock::CatchBlock(CatchBlock&& other) noexcept
    : StreamableBlock<CatchBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

CatchBlock& CatchBlock::operator=(CatchBlock&& other) noexcept {
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

CatchBlock CatchBlock::write_catch(const std::string& exception_type,
                                    const std::string& var_name) {
    writer_->unindent();
    writer_->write_line("} catch (" + exception_type + " " + var_name + ") {");
    writer_->indent();
    already_closed_ = true;  // This catch block is now chained, don't close it again
    return CatchBlock(writer_);
}

// ============================================================================
// ScopeBlock Implementation
// ============================================================================

ScopeBlock::ScopeBlock(CodeWriter* writer)
    : StreamableBlock<ScopeBlock>(writer),
      already_closed_(false)
{
    writer_->write_line("{");
    writer_->indent();
}

ScopeBlock::~ScopeBlock() {
    if (writer_ && !already_closed_) {
        writer_->unindent();
        writer_->write_line("}");
    }
}

ScopeBlock::ScopeBlock(ScopeBlock&& other) noexcept
    : StreamableBlock<ScopeBlock>(other.writer_),
      already_closed_(other.already_closed_)
{
    other.writer_ = nullptr;
}

ScopeBlock& ScopeBlock::operator=(ScopeBlock&& other) noexcept {
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

}  // namespace datascript::codegen

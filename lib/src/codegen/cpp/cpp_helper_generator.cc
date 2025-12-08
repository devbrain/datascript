//
// C++ Helper Function Generator Implementation
//

#include <datascript/codegen/cpp/cpp_helper_generator.hh>
#include <datascript/codegen.hh>  // For cpp_options::error_style

namespace datascript::codegen {

using codegen::endl;
using codegen::blank;

// ============================================================================
// Construction
// ============================================================================

CppHelperGenerator::CppHelperGenerator(CppWriterContext& ctx, cpp_options::error_style error_handling)
    : ctx_(ctx), error_handling_(error_handling)
{
}

// ============================================================================
// Public API
// ============================================================================

void CppHelperGenerator::generate_all() {
    generate_exception_classes();
    generate_binary_readers();
    generate_peek_helpers();
    generate_string_readers();
}

// ============================================================================
// Private Generation Methods
// ============================================================================

void CppHelperGenerator::generate_exception_classes() {
    ctx_ << "// ============================================================================" << endl;
    ctx_ << "// Exception Classes" << endl;
    ctx_ << "// ============================================================================" << endl;
    ctx_ << blank;

    ctx_.start_class("ConstraintViolation : public std::runtime_error");
    ctx_ << "public:" << endl;
    ctx_ << "explicit ConstraintViolation(const std::string& message)" << endl;
    ctx_.writer().indent();
    ctx_ << ": std::runtime_error(message) {}" << endl;
    ctx_.writer().unindent();
    ctx_.end_class();
}

void CppHelperGenerator::generate_binary_readers() {
    ctx_ << "// ============================================================================" << endl;
    ctx_ << "// Binary Reading Helpers" << endl;
    ctx_ << "// ============================================================================" << endl;
    ctx_ << blank;

    // uint8 reader with bounds checking
    ctx_.start_inline_function("uint8_t", "read_uint8", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 1 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint8\");" << endl;
    ctx_.end_if();
    ctx_ << "uint8_t v = p[0];" << endl;
    ctx_ << "p += 1;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // uint16 reader with bounds checking
    ctx_.start_inline_function("uint16_t", "read_uint16", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 2 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint16\");" << endl;
    ctx_.end_if();
    ctx_ << "uint16_t v = static_cast<uint16_t>(p[0]) |" << endl;
    ctx_ << "            (static_cast<uint16_t>(p[1]) << 8);" << endl;
    ctx_ << "p += 2;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // uint32 reader with bounds checking
    ctx_.start_inline_function("uint32_t", "read_uint32", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 4 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint32\");" << endl;
    ctx_.end_if();
    ctx_ << "uint32_t v = static_cast<uint32_t>(p[0]) |" << endl;
    ctx_ << "            (static_cast<uint32_t>(p[1]) << 8) |" << endl;
    ctx_ << "            (static_cast<uint32_t>(p[2]) << 16) |" << endl;
    ctx_ << "            (static_cast<uint32_t>(p[3]) << 24);" << endl;
    ctx_ << "p += 4;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // uint64 reader with bounds checking
    ctx_.start_inline_function("uint64_t", "read_uint64", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 8 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint64\");" << endl;
    ctx_.end_if();
    ctx_ << "uint64_t v = static_cast<uint64_t>(p[0]) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[1]) << 8) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[2]) << 16) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[3]) << 24) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[4]) << 32) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[5]) << 40) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[6]) << 48) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[7]) << 56);" << endl;
    ctx_ << "p += 8;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // Alias little-endian versions (default)
    ctx_ << "inline uint16_t read_uint16_le(const uint8_t*& p, const uint8_t* end) { return read_uint16(p, end); }" << endl;
    ctx_ << "inline uint32_t read_uint32_le(const uint8_t*& p, const uint8_t* end) { return read_uint32(p, end); }" << endl;
    ctx_ << "inline uint64_t read_uint64_le(const uint8_t*& p, const uint8_t* end) { return read_uint64(p, end); }" << endl;
    ctx_ << blank;

    // Big-endian readers with bounds checking
    ctx_.start_inline_function("uint16_t", "read_uint16_be", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 2 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint16_be\");" << endl;
    ctx_.end_if();
    ctx_ << "uint16_t v = (static_cast<uint16_t>(p[0]) << 8) |" << endl;
    ctx_ << "             static_cast<uint16_t>(p[1]);" << endl;
    ctx_ << "p += 2;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    ctx_.start_inline_function("uint32_t", "read_uint32_be", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 4 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint32_be\");" << endl;
    ctx_.end_if();
    ctx_ << "uint32_t v = (static_cast<uint32_t>(p[0]) << 24) |" << endl;
    ctx_ << "            (static_cast<uint32_t>(p[1]) << 16) |" << endl;
    ctx_ << "            (static_cast<uint32_t>(p[2]) << 8) |" << endl;
    ctx_ << "             static_cast<uint32_t>(p[3]);" << endl;
    ctx_ << "p += 4;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    ctx_.start_inline_function("uint64_t", "read_uint64_be", "const uint8_t*& p, const uint8_t* end");
    ctx_.start_if("p + 8 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow reading uint64_be\");" << endl;
    ctx_.end_if();
    ctx_ << "uint64_t v = (static_cast<uint64_t>(p[0]) << 56) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[1]) << 48) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[2]) << 40) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[3]) << 32) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[4]) << 24) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[5]) << 16) |" << endl;
    ctx_ << "            (static_cast<uint64_t>(p[6]) << 8) |" << endl;
    ctx_ << "             static_cast<uint64_t>(p[7]);" << endl;
    ctx_ << "p += 8;" << endl;
    ctx_ << "return v;" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // int readers (just cast from unsigned, bounds checking delegated to unsigned readers)
    ctx_ << "inline int8_t read_int8(const uint8_t*& p, const uint8_t* end) { return static_cast<int8_t>(read_uint8(p, end)); }" << endl;
    ctx_ << "inline int16_t read_int16_le(const uint8_t*& p, const uint8_t* end) { return static_cast<int16_t>(read_uint16_le(p, end)); }" << endl;
    ctx_ << "inline int16_t read_int16_be(const uint8_t*& p, const uint8_t* end) { return static_cast<int16_t>(read_uint16_be(p, end)); }" << endl;
    ctx_ << "inline int32_t read_int32_le(const uint8_t*& p, const uint8_t* end) { return static_cast<int32_t>(read_uint32_le(p, end)); }" << endl;
    ctx_ << "inline int32_t read_int32_be(const uint8_t*& p, const uint8_t* end) { return static_cast<int32_t>(read_uint32_be(p, end)); }" << endl;
    ctx_ << "inline int64_t read_int64_le(const uint8_t*& p, const uint8_t* end) { return static_cast<int64_t>(read_uint64_le(p, end)); }" << endl;
    ctx_ << "inline int64_t read_int64_be(const uint8_t*& p, const uint8_t* end) { return static_cast<int64_t>(read_uint64_be(p, end)); }" << endl;
    ctx_ << blank;
}

void CppHelperGenerator::generate_peek_helpers() {
    ctx_ << "// ============================================================================" << endl;
    ctx_ << "// Peek Helpers (Non-Consuming Reads)" << endl;
    ctx_ << "// ============================================================================" << endl;
    ctx_ << blank;

    // peek_uint8
    ctx_.start_inline_function("uint8_t", "peek_uint8", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 1 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint8\");" << endl;
    ctx_.end_if();
    ctx_ << "return p[0];" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint16_le
    ctx_.start_inline_function("uint16_t", "peek_uint16_le", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 2 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint16\");" << endl;
    ctx_.end_if();
    ctx_ << "return static_cast<uint16_t>(p[0]) | (static_cast<uint16_t>(p[1]) << 8);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint16_be
    ctx_.start_inline_function("uint16_t", "peek_uint16_be", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 2 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint16_be\");" << endl;
    ctx_.end_if();
    ctx_ << "return (static_cast<uint16_t>(p[0]) << 8) | static_cast<uint16_t>(p[1]);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint32_le
    ctx_.start_inline_function("uint32_t", "peek_uint32_le", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 4 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint32\");" << endl;
    ctx_.end_if();
    ctx_ << "return static_cast<uint32_t>(p[0]) |" << endl;
    ctx_ << "       (static_cast<uint32_t>(p[1]) << 8) |" << endl;
    ctx_ << "       (static_cast<uint32_t>(p[2]) << 16) |" << endl;
    ctx_ << "       (static_cast<uint32_t>(p[3]) << 24);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint32_be
    ctx_.start_inline_function("uint32_t", "peek_uint32_be", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 4 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint32_be\");" << endl;
    ctx_.end_if();
    ctx_ << "return (static_cast<uint32_t>(p[0]) << 24) |" << endl;
    ctx_ << "       (static_cast<uint32_t>(p[1]) << 16) |" << endl;
    ctx_ << "       (static_cast<uint32_t>(p[2]) << 8) |" << endl;
    ctx_ << "        static_cast<uint32_t>(p[3]);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint64_le
    ctx_.start_inline_function("uint64_t", "peek_uint64_le", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 8 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint64\");" << endl;
    ctx_.end_if();
    ctx_ << "return static_cast<uint64_t>(p[0]) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[1]) << 8) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[2]) << 16) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[3]) << 24) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[4]) << 32) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[5]) << 40) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[6]) << 48) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[7]) << 56);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // peek_uint64_be
    ctx_.start_inline_function("uint64_t", "peek_uint64_be", "const uint8_t* p, const uint8_t* end");
    ctx_.start_if("p + 8 > end");
    ctx_ << "throw std::runtime_error(\"Buffer underflow peeking uint64_be\");" << endl;
    ctx_.end_if();
    ctx_ << "return (static_cast<uint64_t>(p[0]) << 56) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[1]) << 48) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[2]) << 40) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[3]) << 32) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[4]) << 24) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[5]) << 16) |" << endl;
    ctx_ << "       (static_cast<uint64_t>(p[6]) << 8) |" << endl;
    ctx_ << "        static_cast<uint64_t>(p[7]);" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;
}

void CppHelperGenerator::generate_string_readers() {
    // String reader (exception mode) - only emit if exceptions are enabled
    bool emit_exception_mode = (error_handling_ == cpp_options::exceptions_only ||
                                error_handling_ == cpp_options::both);
    if (emit_exception_mode) {
        ctx_.start_inline_function("std::string", "read_string", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "const uint8_t* start = data;" << endl;
        ctx_.start_while("data < end && *data != 0");
        ctx_ << "data++;" << endl;
        ctx_.end_while();
        ctx_.start_if("data >= end");
        ctx_ << "throw std::runtime_error(\"String not null-terminated before end of buffer\");" << endl;
        ctx_.end_if();
        ctx_ << "std::string result(reinterpret_cast<const char*>(start), data - start);" << endl;
        ctx_ << "data++;  // Skip null terminator" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;
    }

    // ReadResult template - only emit if safe mode is enabled
    bool emit_safe_mode = (error_handling_ == cpp_options::results_only ||
                          error_handling_ == cpp_options::both);
    if (emit_safe_mode) {
        ctx_ << "template<typename T>" << endl;
        ctx_.start_struct("ReadResult");
        ctx_ << "T value;" << endl;
        ctx_ << "std::string error_message;" << endl;
        ctx_ << "operator bool() const { return error_message.empty(); }" << endl;
        ctx_.end_struct();

        // String reader (safe mode)
        ctx_.start_inline_function("ReadResult<std::string>", "read_string_safe", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "const uint8_t* start = data;" << endl;
        ctx_.start_while("data < end && *data != 0");
        ctx_ << "data++;" << endl;
        ctx_.end_while();
        ctx_.start_if("data >= end");
        ctx_ << "ReadResult<std::string> result;" << endl;
        ctx_ << "result.error_message = \"String not null-terminated before end of buffer\";" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_if();
        ctx_ << "std::string str(reinterpret_cast<const char*>(start), data - start);" << endl;
        ctx_ << "data++;  // Skip null terminator" << endl;
        ctx_ << "ReadResult<std::string> result;" << endl;
        ctx_ << "result.value = str;" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;
    }

    // UTF-16 string readers (exception mode)
    if (emit_exception_mode) {
        // Little-endian UTF-16
        ctx_.start_inline_function("std::u16string", "read_u16string_le", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "std::u16string result;" << endl;
        ctx_.start_while("data + 1 < end");
        ctx_ << "char16_t ch = static_cast<char16_t>(data[0]) | (static_cast<char16_t>(data[1]) << 8);" << endl;
        ctx_ << "if (ch == 0) break;" << endl;
        ctx_ << "result.push_back(ch);" << endl;
        ctx_ << "data += 2;" << endl;
        ctx_.end_while();
        ctx_.start_if("data + 1 >= end");
        ctx_ << "throw std::runtime_error(\"UTF-16 string not null-terminated before end of buffer\");" << endl;
        ctx_.end_if();
        ctx_ << "data += 2;  // Skip null terminator" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;

        // Big-endian UTF-16
        ctx_.start_inline_function("std::u16string", "read_u16string_be", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "std::u16string result;" << endl;
        ctx_.start_while("data + 1 < end");
        ctx_ << "char16_t ch = (static_cast<char16_t>(data[0]) << 8) | static_cast<char16_t>(data[1]);" << endl;
        ctx_ << "if (ch == 0) break;" << endl;
        ctx_ << "result.push_back(ch);" << endl;
        ctx_ << "data += 2;" << endl;
        ctx_.end_while();
        ctx_.start_if("data + 1 >= end");
        ctx_ << "throw std::runtime_error(\"UTF-16 string not null-terminated before end of buffer\");" << endl;
        ctx_.end_if();
        ctx_ << "data += 2;  // Skip null terminator" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;
    }

    // UTF-32 string readers (exception mode)
    if (emit_exception_mode) {
        // Little-endian UTF-32
        ctx_.start_inline_function("std::u32string", "read_u32string_le", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "std::u32string result;" << endl;
        ctx_.start_while("data + 3 < end");
        ctx_ << "char32_t ch = static_cast<char32_t>(data[0]) | " << endl;
        ctx_ << "              (static_cast<char32_t>(data[1]) << 8) | " << endl;
        ctx_ << "              (static_cast<char32_t>(data[2]) << 16) | " << endl;
        ctx_ << "              (static_cast<char32_t>(data[3]) << 24);" << endl;
        ctx_ << "if (ch == 0) break;" << endl;
        ctx_ << "result.push_back(ch);" << endl;
        ctx_ << "data += 4;" << endl;
        ctx_.end_while();
        ctx_.start_if("data + 3 >= end");
        ctx_ << "throw std::runtime_error(\"UTF-32 string not null-terminated before end of buffer\");" << endl;
        ctx_.end_if();
        ctx_ << "data += 4;  // Skip null terminator" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;

        // Big-endian UTF-32
        ctx_.start_inline_function("std::u32string", "read_u32string_be", "const uint8_t*& data, const uint8_t* end");
        ctx_ << "std::u32string result;" << endl;
        ctx_.start_while("data + 3 < end");
        ctx_ << "char32_t ch = (static_cast<char32_t>(data[0]) << 24) | " << endl;
        ctx_ << "              (static_cast<char32_t>(data[1]) << 16) | " << endl;
        ctx_ << "              (static_cast<char32_t>(data[2]) << 8) | " << endl;
        ctx_ << "              static_cast<char32_t>(data[3]);" << endl;
        ctx_ << "if (ch == 0) break;" << endl;
        ctx_ << "result.push_back(ch);" << endl;
        ctx_ << "data += 4;" << endl;
        ctx_.end_while();
        ctx_.start_if("data + 3 >= end");
        ctx_ << "throw std::runtime_error(\"UTF-32 string not null-terminated before end of buffer\");" << endl;
        ctx_.end_if();
        ctx_ << "data += 4;  // Skip null terminator" << endl;
        ctx_ << "return result;" << endl;
        ctx_.end_inline_function();
        ctx_ << blank;
    }
}

}  // namespace datascript::codegen

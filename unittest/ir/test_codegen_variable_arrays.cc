//
// Code generation tests for variable-size arrays
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Variable-Size Arrays") {
    TEST_CASE("Simple variable-size array with field reference") {
        const char* schema = R"(
struct DataBlock {
    uint16 count;
    uint32 data[count];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& block = ir.structs[0];
            REQUIRE(block.fields.size() == 2);

            // Verify count field
            const auto& count = block.fields[0];
            CHECK(count.name == "count");
            CHECK(count.type.kind == ir::type_kind::uint16);

            // Verify data field is variable-size array
            const auto& data = block.fields[1];
            CHECK(data.name == "data");
            CHECK(data.type.kind == ir::type_kind::array_variable);
            CHECK(data.type.is_variable_size == true);
            CHECK(data.type.array_size_expr != nullptr);
            CHECK(data.type.element_type != nullptr);
            CHECK(data.type.element_type->kind == ir::type_kind::uint32);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify vector declaration
            CHECK(cpp_code.find("std::vector<uint32_t> data;") != std::string::npos);

            // Verify resize call
            CHECK(cpp_code.find("obj.data.resize(obj.count);") != std::string::npos);

            // Verify loop-based reading
            CHECK(cpp_code.find("for (size_t i = 0; i < obj.count; i++)") != std::string::npos);
            CHECK(cpp_code.find("obj.data[i] = read_uint32_le(data, end);") != std::string::npos);

            // Verify count is read before data
            size_t count_pos = cpp_code.find("obj.count = read_uint16_le(data, end);");
            size_t resize_pos = cpp_code.find("obj.data.resize");
            CHECK(count_pos < resize_pos);  // count must be read first
        }
    }

    TEST_CASE("Variable-size array with different element types") {
        const char* schema = R"(
struct Message {
    uint8 length;
    uint8 bytes[length];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify uint8 vector
        CHECK(cpp_code.find("std::vector<uint8_t> bytes;") != std::string::npos);
        CHECK(cpp_code.find("obj.bytes.resize(obj.length);") != std::string::npos);
        CHECK(cpp_code.find("obj.bytes[i] = read_uint8(data, end);") != std::string::npos);
    }

    TEST_CASE("Multiple variable-size arrays") {
        const char* schema = R"(
struct MultiArray {
    uint16 count_a;
    uint32 data_a[count_a];
    uint8 count_b;
    uint16 data_b[count_b];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& s = ir.structs[0];
            REQUIRE(s.fields.size() == 4);

            // Both array fields should be variable-size
            CHECK(s.fields[1].type.kind == ir::type_kind::array_variable);
            CHECK(s.fields[3].type.kind == ir::type_kind::array_variable);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify both vectors
            CHECK(cpp_code.find("std::vector<uint32_t> data_a;") != std::string::npos);
            CHECK(cpp_code.find("std::vector<uint16_t> data_b;") != std::string::npos);

            // Verify both resize calls
            CHECK(cpp_code.find("obj.data_a.resize(obj.count_a);") != std::string::npos);
            CHECK(cpp_code.find("obj.data_b.resize(obj.count_b);") != std::string::npos);

            // Verify reading order
            size_t count_a_pos = cpp_code.find("obj.count_a =");
            size_t data_a_pos = cpp_code.find("obj.data_a.resize");
            size_t count_b_pos = cpp_code.find("obj.count_b =");
            size_t data_b_pos = cpp_code.find("obj.data_b.resize");

            CHECK(count_a_pos < data_a_pos);
            CHECK(data_a_pos < count_b_pos);
            CHECK(count_b_pos < data_b_pos);
        }
    }

    TEST_CASE("Variable-size array with endianness") {
        const char* schema = R"(
struct BigEndianArray {
    uint16 count;
    big uint32 values[count];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify big-endian reading
        CHECK(cpp_code.find("obj.values[i] = read_uint32_be(data, end);") != std::string::npos);
    }

    TEST_CASE("Variable-size string array") {
        const char* schema = R"(
struct StringArray {
    uint8 num_strings;
    string messages[num_strings];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify string vector
        CHECK(cpp_code.find("std::vector<std::string> messages;") != std::string::npos);
        CHECK(cpp_code.find("obj.messages.resize(obj.num_strings);") != std::string::npos);
    }

    TEST_CASE("Fixed-size and variable-size arrays mixed") {
        const char* schema = R"(
struct MixedArrays {
    uint32 header[4];
    uint16 count;
    uint8 payload[count];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& s = ir.structs[0];
            REQUIRE(s.fields.size() == 3);

            // First array is fixed-size
            CHECK(s.fields[0].type.kind == ir::type_kind::array_fixed);
            CHECK(s.fields[0].type.array_size.has_value());
            CHECK(*s.fields[0].type.array_size == 4);

            // Third array is variable-size
            CHECK(s.fields[2].type.kind == ir::type_kind::array_variable);
            CHECK(s.fields[2].type.array_size_expr != nullptr);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Fixed array uses std::array
            CHECK(cpp_code.find("std::array<uint32_t, 4> header;") != std::string::npos);

            // Variable array uses std::vector
            CHECK(cpp_code.find("std::vector<uint8_t> payload;") != std::string::npos);

            // Fixed array loops with literal count
            CHECK(cpp_code.find("for (size_t i = 0; i < 4; i++)") != std::string::npos);

            // Variable array resizes and loops with count field
            CHECK(cpp_code.find("obj.payload.resize(obj.count);") != std::string::npos);
            CHECK(cpp_code.find("for (size_t i = 0; i < obj.count; i++)") != std::string::npos);
        }
    }

    TEST_CASE("Exception-only error handling with variable arrays") {
        const char* schema = R"(
struct Data {
    uint16 size;
    uint32 items[size];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::exceptions_only;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify exception-based read method exists
        CHECK(cpp_code.find("static Data read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);

        // Verify resize call in exception-based method
        CHECK(cpp_code.find("obj.items.resize(obj.size);") != std::string::npos);

        // Should not have read_safe with exceptions_only
        CHECK(cpp_code.find("read_safe(") == std::string::npos);
    }

    TEST_CASE("Complex array size with multiplication") {
        const char* schema = R"(
struct DoubleSize {
    uint16 count;
    uint32 data[count * 2];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& s = ir.structs[0];
            REQUIRE(s.fields.size() == 2);

            // Verify data field has expression
            const auto& data = s.fields[1];
            CHECK(data.type.kind == ir::type_kind::array_variable);
            CHECK(data.type.is_variable_size == true);
            CHECK(data.type.array_size_expr != nullptr);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify multiplication expression in resize
            CHECK(cpp_code.find("obj.data.resize((obj.count * 2));") != std::string::npos);

            // Verify vector declaration
            CHECK(cpp_code.find("std::vector<uint32_t> data;") != std::string::npos);
        }
    }

    TEST_CASE("Complex array size with addition") {
        const char* schema = R"(
struct AddedSize {
    uint16 base_count;
    uint8 extra_count;
    uint32 items[base_count + extra_count];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify addition expression
        CHECK(cpp_code.find("obj.items.resize((obj.base_count + obj.extra_count));") != std::string::npos);

        // Verify both fields read before resize
        size_t base_pos = cpp_code.find("obj.base_count = read_uint16_le(data, end);");
        size_t extra_pos = cpp_code.find("obj.extra_count = read_uint8(data, end);");
        size_t resize_pos = cpp_code.find("obj.items.resize");

        CHECK(base_pos < resize_pos);
        CHECK(extra_pos < resize_pos);
    }

    TEST_CASE("Complex array size with subtraction") {
        const char* schema = R"(
struct SubtractedSize {
    uint16 total;
    uint32 data[total - 1];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify subtraction expression
        CHECK(cpp_code.find("obj.data.resize((obj.total - 1));") != std::string::npos);
    }

    TEST_CASE("Complex array size with division") {
        const char* schema = R"(
struct DividedSize {
    uint16 byte_count;
    uint32 words[byte_count / 4];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify division expression
        CHECK(cpp_code.find("obj.words.resize((obj.byte_count / 4));") != std::string::npos);
    }

    TEST_CASE("Nested complex array size expressions") {
        const char* schema = R"(
struct NestedExpr {
    uint16 count;
    uint8 multiplier;
    uint32 data[(count + 1) * multiplier];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify nested expression with correct parenthesization
        CHECK(cpp_code.find("obj.data.resize(((obj.count + 1) * obj.multiplier));") != std::string::npos);
    }

    TEST_CASE("Multiple complex array size expressions") {
        const char* schema = R"(
struct MultiComplex {
    uint16 n;
    uint32 doubled[n * 2];
    uint32 halved[n / 2];
    uint32 offset[n + 10];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& s = ir.structs[0];
            REQUIRE(s.fields.size() == 4);

            // All three array fields should be variable-size with expressions
            CHECK(s.fields[1].type.kind == ir::type_kind::array_variable);
            CHECK(s.fields[2].type.kind == ir::type_kind::array_variable);
            CHECK(s.fields[3].type.kind == ir::type_kind::array_variable);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify all three expressions
            CHECK(cpp_code.find("obj.doubled.resize((obj.n * 2));") != std::string::npos);
            CHECK(cpp_code.find("obj.halved.resize((obj.n / 2));") != std::string::npos);
            CHECK(cpp_code.find("obj.offset.resize((obj.n + 10));") != std::string::npos);

            // Verify reading order
            size_t n_pos = cpp_code.find("obj.n = read_uint16_le(data, end);");
            size_t doubled_pos = cpp_code.find("obj.doubled.resize");
            size_t halved_pos = cpp_code.find("obj.halved.resize");
            size_t offset_pos = cpp_code.find("obj.offset.resize");

            CHECK(n_pos < doubled_pos);
            CHECK(doubled_pos < halved_pos);
            CHECK(halved_pos < offset_pos);
        }
    }

    TEST_CASE("Complex array size with modulo") {
        const char* schema = R"(
struct ModuloSize {
    uint16 value;
    uint32 remainder[value % 10];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify modulo expression
        CHECK(cpp_code.find("obj.remainder.resize((obj.value % 10));") != std::string::npos);
    }

    TEST_CASE("Unbounded array - read until EOF") {
        const char* schema = R"(
struct DataBlock {
    uint32 header;
    uint8 remaining_bytes[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            REQUIRE(ir.structs.size() == 1);
            const auto& block = ir.structs[0];
            REQUIRE(block.fields.size() == 2);

            // Verify unbounded array field
            const auto& remaining = block.fields[1];
            CHECK(remaining.name == "remaining_bytes");
            CHECK(remaining.type.kind == ir::type_kind::array_variable);
            CHECK(remaining.type.element_type != nullptr);
            CHECK(remaining.type.array_size_expr == nullptr);  // No size expression for unbounded
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify vector declaration
            CHECK(cpp_code.find("std::vector<uint8_t> remaining_bytes;") != std::string::npos);

            // Verify EOF-based reading
            CHECK(cpp_code.find("// Read until end of data") != std::string::npos);
            CHECK(cpp_code.find("while (data < end) {") != std::string::npos);
            CHECK(cpp_code.find("obj.remaining_bytes.push_back(read_uint8(data, end));") != std::string::npos);

            // Verify no resize call (unbounded arrays don't resize)
            size_t remaining_resize = cpp_code.find("obj.remaining_bytes.resize");
            CHECK(remaining_resize == std::string::npos);
        }
    }

    TEST_CASE("Unbounded array of different types") {
        const char* schema = R"(
struct IntStream {
    uint32 magic;
    uint32 values[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify uint32 unbounded array
        CHECK(cpp_code.find("std::vector<uint32_t> values;") != std::string::npos);
        CHECK(cpp_code.find("obj.values.push_back(read_uint32_le(data, end));") != std::string::npos);
    }

    TEST_CASE("Unbounded array with big-endian elements") {
        const char* schema = R"(
struct BigEndianStream {
    big uint16 values[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify big-endian reading
        CHECK(cpp_code.find("obj.values.push_back(read_uint16_be(data, end));") != std::string::npos);
    }

    TEST_CASE("Unbounded string array") {
        const char* schema = R"(
struct MessageLog {
    uint8 version;
    string messages[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify string unbounded array
        CHECK(cpp_code.find("std::vector<std::string> messages;") != std::string::npos);
        CHECK(cpp_code.find("while (data < end) {") != std::string::npos);

        // Should use read_string_safe for result-based method
        CHECK(cpp_code.find("read_string_safe(data, end)") != std::string::npos);
        CHECK(cpp_code.find("obj.messages.push_back(*str_result);") != std::string::npos);
    }

    TEST_CASE("Unbounded struct array") {
        const char* schema = R"(
struct Entry { uint32 id; string name; };

struct Database {
    uint16 version;
    Entry entries[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify struct unbounded array
        CHECK(cpp_code.find("std::vector<Entry> entries;") != std::string::npos);
        CHECK(cpp_code.find("while (data < end) {") != std::string::npos);
        CHECK(cpp_code.find("Entry::read_safe(data, end)") != std::string::npos);
        CHECK(cpp_code.find("obj.entries.push_back(*nested_result);") != std::string::npos);
    }

    TEST_CASE("Exception-only error handling with unbounded arrays") {
        const char* schema = R"(
struct Stream {
    uint8 bytes[];
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::exceptions_only;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify exception-based read method exists
        CHECK(cpp_code.find("static Stream read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);

        // Verify unbounded array in exception mode
        CHECK(cpp_code.find("while (data < end) {") != std::string::npos);
        CHECK(cpp_code.find("obj.bytes.push_back(read_uint8(data, end));") != std::string::npos);

        // Should not have read_safe with exceptions_only
        CHECK(cpp_code.find("read_safe(") == std::string::npos);
    }
}

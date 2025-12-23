//
// Code generation tests for conditional fields
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Conditional Fields") {
    TEST_CASE("Simple conditional field with != operator") {
        const char* schema = R"(
struct Header {
    uint8 flags;
    uint32 extended_data if flags != 0;
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
            const auto& header = ir.structs[0];
            REQUIRE(header.fields.size() == 2);

            // Verify first field is unconditional
            const auto& flags = header.fields[0];
            CHECK(flags.name == "flags");
            CHECK(flags.condition == ir::field::always);

            // Verify second field is conditional
            const auto& extended = header.fields[1];
            CHECK(extended.name == "extended_data");
            CHECK(extended.condition == ir::field::runtime);
            CHECK(extended.runtime_condition.has_value());
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify struct members
            CHECK(cpp_code.find("uint8_t flags;") != std::string::npos);
            CHECK(cpp_code.find("uint32_t extended_data;") != std::string::npos);

            // Verify conditional reading in read_safe
            CHECK(cpp_code.find("// Conditional field: extended_data") != std::string::npos);
            CHECK(cpp_code.find("if ((obj.flags != 0))") != std::string::npos);

            // Verify unconditional flag is read first
            size_t flags_read = cpp_code.find("obj.flags = read_uint8(data, end);");
            size_t conditional = cpp_code.find("if ((obj.flags != 0))");
            CHECK(flags_read < conditional);  // flags must be read before condition
        }
    }

    TEST_CASE("Conditional field with == operator") {
        const char* schema = R"(
struct Message {
    uint8 msg_type;
    string payload if msg_type == 1;
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

        // Verify conditional with equality check
        CHECK(cpp_code.find("if ((obj.msg_type == 1))") != std::string::npos);
        CHECK(cpp_code.find("std::string payload;") != std::string::npos);
    }

    TEST_CASE("Multiple conditional fields") {
        const char* schema = R"(
struct Packet {
    uint8 version;
    uint32 checksum if version > 1;
    uint64 timestamp if version >= 2;
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
            const auto& packet = ir.structs[0];
            REQUIRE(packet.fields.size() == 3);

            // First field unconditional
            CHECK(packet.fields[0].condition == ir::field::always);

            // Second and third are conditional
            CHECK(packet.fields[1].condition == ir::field::runtime);
            CHECK(packet.fields[2].condition == ir::field::runtime);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify both conditions are generated
            CHECK(cpp_code.find("if ((obj.version > 1))") != std::string::npos);
            CHECK(cpp_code.find("if ((obj.version >= 2))") != std::string::npos);

            // Verify fields
            CHECK(cpp_code.find("uint32_t checksum;") != std::string::npos);
            CHECK(cpp_code.find("uint64_t timestamp;") != std::string::npos);
        }
    }

    TEST_CASE("Conditional array field") {
        const char* schema = R"(
struct DataBlock {
    uint8 has_extra;
    uint32[4] extra_data if has_extra != 0;
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

            // Verify conditional array field
            const auto& extra = block.fields[1];
            CHECK(extra.name == "extra_data");
            CHECK(extra.condition == ir::field::runtime);
            CHECK(extra.type.kind == ir::type_kind::array_fixed);
            CHECK(extra.type.array_size.has_value());
            CHECK(*extra.type.array_size == 4);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify conditional array
            CHECK(cpp_code.find("std::array<uint32_t, 4> extra_data;") != std::string::npos);
            CHECK(cpp_code.find("if ((obj.has_extra != 0))") != std::string::npos);

            // Verify array reading is inside conditional block
            size_t if_pos = cpp_code.find("if ((obj.has_extra != 0))");
            size_t for_pos = cpp_code.find("for (size_t i = 0; i < static_cast<size_t>(4); i++)", if_pos);
            CHECK(for_pos != std::string::npos);
            CHECK(for_pos > if_pos);  // Loop must be after conditional
        }
    }

    TEST_CASE("Conditional with less-than operator") {
        const char* schema = R"(
struct Record {
    uint16 size;
    uint8 legacy_field if size < 100;
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

        // Verify less-than condition
        CHECK(cpp_code.find("if ((obj.size < 100))") != std::string::npos);
    }

    TEST_CASE("Exception-only error handling with conditionals") {
        const char* schema = R"(
struct Header {
    uint8 flags;
    uint32 data if flags != 0;
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

        // Verify conditional in exception-based read method
        CHECK(cpp_code.find("static Header read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
        CHECK(cpp_code.find("if ((obj.flags != 0))") != std::string::npos);

        // Should not have read_safe method with exceptions_only
        CHECK(cpp_code.find("read_safe(") == std::string::npos);
    }

    TEST_CASE("Result-only error handling with conditionals") {
        const char* schema = R"(
struct Header {
    uint8 flags;
    uint32 data if flags != 0;
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
        opts.error_handling = codegen::cpp_options::results_only;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify conditional in result-based read method
        CHECK(cpp_code.find("static ReadResult<Header> read_safe(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
        CHECK(cpp_code.find("if ((obj.flags != 0))") != std::string::npos);

        // Should not have throwing read with result_only
        size_t read_pos = cpp_code.find("static Header read(");
        CHECK(read_pos == std::string::npos);
    }

    TEST_CASE("Complex boolean expression with AND operator") {
        const char* schema = R"(
struct ComplexAnd {
    uint8 flags;
    uint8 version;
    uint32 extended_data if (flags != 0) && (version >= 2);
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

            // Verify conditional field
            const auto& extended = s.fields[2];
            CHECK(extended.name == "extended_data");
            CHECK(extended.condition == ir::field::runtime);
            CHECK(extended.runtime_condition.has_value());
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify AND expression is correctly generated
            CHECK(cpp_code.find("if (((obj.flags != 0) && (obj.version >= 2)))") != std::string::npos);

            // Verify both fields are read before condition
            size_t flags_read = cpp_code.find("obj.flags = read_uint8(data, end);");
            size_t version_read = cpp_code.find("obj.version = read_uint8(data, end);");
            size_t condition = cpp_code.find("if (((obj.flags != 0) && (obj.version >= 2)))");

            CHECK(flags_read < condition);
            CHECK(version_read < condition);
        }
    }

    TEST_CASE("Complex boolean expression with OR operator") {
        const char* schema = R"(
struct ComplexOr {
    uint8 msg_type;
    string payload if (msg_type == 1) || (msg_type == 2);
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

        // Verify OR expression is correctly generated
        CHECK(cpp_code.find("if (((obj.msg_type == 1) || (obj.msg_type == 2)))") != std::string::npos);
        CHECK(cpp_code.find("std::string payload;") != std::string::npos);
    }

    TEST_CASE("Complex boolean expression with NOT operator") {
        const char* schema = R"(
struct ComplexNot {
    uint8 version;
    uint32 modern_field if !(version < 2);
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

        // Verify NOT expression is correctly generated
        CHECK(cpp_code.find("if ((!(obj.version < 2)))") != std::string::npos);
    }

    TEST_CASE("Nested complex boolean expressions") {
        const char* schema = R"(
struct NestedExpr {
    uint8 flags;
    uint8 version;
    uint8 feature_level;
    uint64 advanced_data if ((flags != 0) && (version >= 2)) || (feature_level > 5);
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

            // Verify conditional field with nested expression
            const auto& advanced = s.fields[3];
            CHECK(advanced.name == "advanced_data");
            CHECK(advanced.condition == ir::field::runtime);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify nested expression with correct parenthesization
            CHECK(cpp_code.find("if ((((obj.flags != 0) && (obj.version >= 2)) || (obj.feature_level > 5)))") != std::string::npos);
        }
    }

    TEST_CASE("Complex expression with multiple AND conditions") {
        const char* schema = R"(
struct MultiAnd {
    uint8 a;
    uint8 b;
    uint8 c;
    uint32 result if (a > 0) && (b > 0) && (c > 0);
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

        // Verify chained AND expression
        CHECK(cpp_code.find("if ((((obj.a > 0) && (obj.b > 0)) && (obj.c > 0)))") != std::string::npos);
    }

    TEST_CASE("Complex expression with NOT and comparison") {
        const char* schema = R"(
struct NotComparison {
    uint8 status;
    uint32 data if !(status == 0);
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

        // Verify NOT with equality
        CHECK(cpp_code.find("if ((!(obj.status == 0)))") != std::string::npos);
    }

    TEST_CASE("Operator precedence: AND before OR") {
        const char* schema = R"(
struct Precedence {
    uint8 a;
    uint8 b;
    uint8 c;
    uint32 data if (a == 1) || (b == 2) && (c == 3);
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

        // Verify expression maintains precedence (AND binds tighter than OR)
        // Should parse as: (a == 1) || ((b == 2) && (c == 3))
        CHECK(cpp_code.find("if (((obj.a == 1) || ((obj.b == 2) && (obj.c == 3))))") != std::string::npos);
    }
}

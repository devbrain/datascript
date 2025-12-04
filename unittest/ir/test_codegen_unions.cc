//
// Code generation tests for union types
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Union Types") {
    TEST_CASE("Union type generation") {
        const char* schema = R"(
union Value {
    uint32 as_int;
    uint16 as_short;
    uint8 as_byte;
};

struct Message {
    uint8 value_type;
    Value value;
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
            CHECK(ir.structs.size() == 1);
            CHECK(ir.unions.size() == 1);

            const auto& union_def = ir.unions[0];
            CHECK(union_def.name == "Value");
            CHECK(union_def.cases.size() == 3);  // Simple fields become single-field cases
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify union struct wrapper (std::variant-based approach)
            CHECK(cpp_code.find("struct Value {") != std::string::npos);
            CHECK(cpp_code.find("std::variant<") != std::string::npos);
            CHECK(cpp_code.find("value;") != std::string::npos);

            // Verify variant types
            CHECK(cpp_code.find("uint32_t") != std::string::npos);
            CHECK(cpp_code.find("uint16_t") != std::string::npos);
            CHECK(cpp_code.find("uint8_t") != std::string::npos);

            // Verify read_as methods for each variant
            CHECK(cpp_code.find("read_as_as_int") != std::string::npos);
            CHECK(cpp_code.find("read_as_as_short") != std::string::npos);
            CHECK(cpp_code.find("read_as_as_byte") != std::string::npos);

            // Verify unified read() method with trial-and-error
            CHECK(cpp_code.find("static Value read(") != std::string::npos);
            CHECK(cpp_code.find("try {") != std::string::npos);
            CHECK(cpp_code.find("catch (const ConstraintViolation&)") != std::string::npos);
        }
    }

    TEST_CASE("Union with nested struct") {
        const char* schema = R"(
struct Point { uint32 x; uint32 y; };

union Data {
    uint64 as_number;
    Point as_point;
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
        CHECK(ir.unions.size() == 1);
        CHECK(ir.unions[0].cases.size() == 2);  // Simple fields become single-field cases

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify nested struct in union
        CHECK(cpp_code.find("Point as_point;") != std::string::npos);
        CHECK(cpp_code.find("Point::read") != std::string::npos);
    }
}

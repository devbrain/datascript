//
// Code generation tests for arrays
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Arrays") {
    TEST_CASE("Fixed-size primitive arrays") {
        const char* schema = R"(
struct PacketHeader {
    uint8 magic[4];
    uint16 ports[10];
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
            const auto& header = ir.structs[0];
            CHECK(header.fields.size() == 2);

            // Verify first field is array
            const auto& magic = header.fields[0];
            CHECK(magic.type.kind == ir::type_kind::array_fixed);
            CHECK(magic.type.array_size.has_value());
            CHECK(*magic.type.array_size == 4);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify std::array usage
            CHECK(cpp_code.find("std::array<uint8_t, 4> magic;") != std::string::npos);
            CHECK(cpp_code.find("std::array<uint16_t, 10> ports;") != std::string::npos);

            // Verify loop-based reading
            CHECK(cpp_code.find("for (size_t i = 0; i < 4; i++)") != std::string::npos);
            CHECK(cpp_code.find("for (size_t i = 0; i < 10; i++)") != std::string::npos);
        }
    }

    // Note: Variable-size arrays require parser support for T[expr] syntax
    // This is implemented in IR and codegen but parser doesn't support it yet

    TEST_CASE("Fixed-size struct arrays") {
        const char* schema = R"(
struct Point { uint32 x; uint32 y; };

struct Polygon {
    Point vertices[3];
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
        CHECK(ir.structs.size() == 2);

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify array of structs
        CHECK(cpp_code.find("std::array<Point, 3> vertices;") != std::string::npos);
        // Note: Current implementation may not fully support nested struct arrays yet
    }
}

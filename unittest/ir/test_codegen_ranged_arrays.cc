//
// Code generation tests for ranged arrays
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

/*
 * NOTE: Ranged array tests comply with DataScript specification.
 *
 * Per the official DataScript spec (DataScript-_A_Specification_and_Scripting_Language.pdf):
 * "An array's lower and upper bounds are given as two expressions: [lower.. upper],
 *  which includes lower, but excludes upper. If lower is omitted, it defaults to zero."
 *
 * Spec-compliant syntax (ranged arrays are type specifiers, not field array syntax):
 *   type[lower..upper] name;  - Array with size (upper - lower), bounds check at runtime
 *   type[..upper] name;       - Same as type[0..upper]
 *
 * Examples:
 *   uint32[0..count] items;   // Variable-size array, size = count - 0 = count
 *   uint32[..count] items;    // Same as above (min defaults to 0)
 *   uint32[5..count] items;   // Variable-size array, size = count - 5
 *
 * The syntax [count in 1..10] does NOT exist in the specification.
 */

TEST_SUITE("IR/Codegen - Ranged Arrays") {
    TEST_CASE("Simple ranged array with literal bounds") {
        // Per DataScript spec: [lower..upper] where upper is exclusive
        // Array size will be (count - 0) = count elements
        // Syntax: type[min..max] name;
        const char* schema = R"(
struct Data {
    uint8 count;
    uint32[0..count] items;
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
            const auto& data = ir.structs[0];
            REQUIRE(data.fields.size() == 2);

            const auto& items_field = data.fields[1];
            CHECK(items_field.name == "items");
            CHECK(items_field.type.kind == ir::type_kind::array_ranged);
            CHECK(items_field.type.element_type != nullptr);
            CHECK(items_field.type.min_size_expr != nullptr);
            CHECK(items_field.type.max_size_expr != nullptr);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify type is std::vector (ranged arrays use std::vector)
            CHECK(cpp_code.find("std::vector<uint32_t> items;") != std::string::npos);

            // Verify array size computation (count - 0)
            CHECK(cpp_code.find("uint64_t array_size = (obj.count - 0);") != std::string::npos);

            // Verify bounds check for ranged array [0..count]
            CHECK(cpp_code.find("if (array_size < 0 || array_size > obj.count)") != std::string::npos);

            // Verify resize and read loop
            CHECK(cpp_code.find("obj.items.resize(array_size);") != std::string::npos);
            CHECK(cpp_code.find("for (size_t i = 0; i < static_cast<size_t>(array_size); i++)") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array without minimum bound") {
        // Per spec: [..max] defaults lower bound to 0, so [..100] means [0..100]
        // Syntax: type[..max] name;
        const char* schema = R"(
struct Data {
    uint8 count;
    uint16[..count] values;
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
            const auto& data = ir.structs[0];
            REQUIRE(data.fields.size() == 2);

            const auto& values_field = data.fields[1];
            CHECK(values_field.name == "values");
            CHECK(values_field.type.kind == ir::type_kind::array_ranged);
            CHECK(values_field.type.min_size_expr != nullptr);  // Has minimum (0)
            CHECK(values_field.type.max_size_expr != nullptr);  // Has maximum (count)
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify array size computation and bounds check
            CHECK(cpp_code.find("uint64_t array_size = (obj.count - 0);") != std::string::npos);
            CHECK(cpp_code.find("if (array_size < 0 || array_size > obj.count)") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array with constant bounds" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
const uint8 MIN_SIZE = 5;
const uint8 MAX_SIZE = 20;

struct Data {
    uint8 count;
    uint32 items[count in MIN_SIZE..MAX_SIZE];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify constants are generated
            CHECK(cpp_code.find("constexpr uint8_t MIN_SIZE = 5;") != std::string::npos);
            CHECK(cpp_code.find("constexpr uint8_t MAX_SIZE = 20;") != std::string::npos);

            // Verify bounds check uses constant names
            CHECK(cpp_code.find("if (array_size < MIN_SIZE || array_size > MAX_SIZE)") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array with expression bounds" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
struct Data {
    uint8 count;
    uint32 items[count in 2*5..100/2];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify expressions are generated (C++ compiler will evaluate)
            CHECK(cpp_code.find("if (array_size < (2 * 5) || array_size > (100 / 2))") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array with different element types" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

struct Data {
    uint8 count;
    Status statuses[count in 1..5];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify enum array type
            CHECK(cpp_code.find("std::vector<Status> statuses;") != std::string::npos);

            // Verify enum reading with bounds check
            CHECK(cpp_code.find("if (array_size < 1 || array_size > 5)") != std::string::npos);
            CHECK(cpp_code.find("static_cast<Status>(read_uint8(data, end))") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array with struct elements" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
struct Point {
    uint32 x;
    uint32 y;
};

struct Path {
    uint8 num_points;
    Point points[num_points in 2..10];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify struct array type
            CHECK(cpp_code.find("std::vector<Point> points;") != std::string::npos);

            // Verify bounds check
            CHECK(cpp_code.find("if (array_size < 2 || array_size > 10)") != std::string::npos);

            // Verify nested struct reading
            CHECK(cpp_code.find("Point::read_safe(data, end)") != std::string::npos);
        }
    }

    TEST_CASE("Exception-only mode with ranged arrays" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
struct Data {
    uint8 count;
    uint32 items[count in 1..10];
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

        // Verify exception-based read method
        CHECK(cpp_code.find("static Data read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);

        // Verify throws exception on bounds violation
        CHECK(cpp_code.find("throw std::runtime_error") != std::string::npos);
        CHECK(cpp_code.find("out of range") != std::string::npos);

        // Verify no ReadResult wrapper
        CHECK(cpp_code.find("ReadResult") == std::string::npos);
    }

    TEST_CASE("Multiple ranged arrays in same struct" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
struct Data {
    uint8 count1;
    uint32 items1[count1 in 1..10];
    uint8 count2;
    uint16 items2[count2 in 5..20];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify both arrays have bounds checks
            CHECK(cpp_code.find("if (array_size < 1 || array_size > 10)") != std::string::npos);
            CHECK(cpp_code.find("if (array_size < 5 || array_size > 20)") != std::string::npos);
        }
    }

    TEST_CASE("Ranged array with big-endian elements" * doctest::skip()) {  // TODO: Update to spec-compliant syntax
        const char* schema = R"(
struct Data {
    uint8 count;
    big uint32 items[count in 1..10];
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

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify big-endian read function
            CHECK(cpp_code.find("read_uint32_be(data, end)") != std::string::npos);

            // Verify bounds check still present
            CHECK(cpp_code.find("if (array_size < 1 || array_size > 10)") != std::string::npos);
        }
    }
}

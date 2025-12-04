//
// Basic code generation tests
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Basic Functionality") {
    TEST_CASE("Simple struct with primitives") {
        const char* schema = R"(
const uint32 MAGIC = 0xDEADBEEF;

struct Header {
    uint32 magic;
    uint16 version;
    uint8 flags;
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
            CHECK(ir.constants.size() == 1);

            const auto& header = ir.structs[0];
            CHECK(header.name == "Header");
            CHECK(header.fields.size() == 3);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify namespace
            CHECK(cpp_code.find("namespace test {") != std::string::npos);

            // Verify constant
            CHECK(cpp_code.find("constexpr uint32_t MAGIC = 3735928559") != std::string::npos);

            // Verify struct
            CHECK(cpp_code.find("struct Header {") != std::string::npos);
            CHECK(cpp_code.find("uint32_t magic;") != std::string::npos);
            CHECK(cpp_code.find("uint16_t version;") != std::string::npos);
            CHECK(cpp_code.find("uint8_t flags;") != std::string::npos);

            // Verify dual error handling
            CHECK(cpp_code.find("ReadResult<Header> read_safe") != std::string::npos);
            CHECK(cpp_code.find("static Header read(") != std::string::npos);
        }
    }

    TEST_CASE("Enum generation") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

struct Message {
    Status status;
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
        CHECK(ir.enums.size() == 1);

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify enum class
        CHECK(cpp_code.find("enum class Status : uint8_t {") != std::string::npos);
        CHECK(cpp_code.find("OK = 0") != std::string::npos);
        CHECK(cpp_code.find("ERROR = 1") != std::string::npos);

        // Verify static_cast in struct reading
        CHECK(cpp_code.find("static_cast<Status>(read_uint8(data, end))") != std::string::npos);
    }

    TEST_CASE("Endianness support") {
        const char* schema = R"(
struct Header {
    little uint32 magic;
    big uint16 flags;
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

        // Verify little endian reader
        CHECK(cpp_code.find("read_uint32_le(data, end)") != std::string::npos);

        // Verify big endian reader
        CHECK(cpp_code.find("read_uint16_be(data, end)") != std::string::npos);
    }

    TEST_CASE("Nested structs") {
        const char* schema = R"(
struct Point {
    uint32 x;
    uint32 y;
};

struct Rectangle {
    Point top_left;
    Point bottom_right;
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

        // Verify nested struct reading
        CHECK(cpp_code.find("Point::read_safe(data, end)") != std::string::npos);
        CHECK(cpp_code.find("Point top_left;") != std::string::npos);
        CHECK(cpp_code.find("Point bottom_right;") != std::string::npos);
    }
}

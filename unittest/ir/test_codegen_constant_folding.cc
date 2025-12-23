//
// Code generation tests for constant folding in array sizes
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Constant Folding") {
    TEST_CASE("Simple constant array size") {
        const char* schema = R"(
const uint16 SIZE = 10;

struct Data {
    uint32 items[SIZE];
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

        SUBCASE("Constants in IR") {
            // Verify constant is in IR
            REQUIRE(ir.constants.size() == 1);
            CHECK(ir.constants.count("SIZE") == 1);
            CHECK(ir.constants["SIZE"] == 10);
        }

        SUBCASE("Array is fixed-size in IR") {
            REQUIRE(ir.structs.size() == 1);
            const auto& data = ir.structs[0];
            REQUIRE(data.fields.size() == 1);

            const auto& items = data.fields[0];
            CHECK(items.type.kind == ir::type_kind::array_fixed);
            CHECK(items.type.array_size.has_value());
            CHECK(*items.type.array_size == 10);
            CHECK(items.type.is_variable_size == false);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify constant is generated
            CHECK(cpp_code.find("constexpr uint8_t SIZE = 10;") != std::string::npos);

            // Verify array uses constant name (not literal)
            CHECK(cpp_code.find("std::array<uint32_t, SIZE> items;") != std::string::npos);
            CHECK(cpp_code.find("std::array<uint32_t, 10>") == std::string::npos);

            // Verify loop uses constant name
            CHECK(cpp_code.find("for (size_t i = 0; i < static_cast<size_t>(SIZE); i++)") != std::string::npos);
        }
    }

    TEST_CASE("Multiple constants") {
        const char* schema = R"(
const uint8 HEADER_SIZE = 4;
const uint16 DATA_SIZE = 100;

struct Packet {
    uint8 header[HEADER_SIZE];
    uint32 data[DATA_SIZE];
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

        SUBCASE("Constants in IR") {
            REQUIRE(ir.constants.size() == 2);
            CHECK(ir.constants["HEADER_SIZE"] == 4);
            CHECK(ir.constants["DATA_SIZE"] == 100);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Both constants generated
            CHECK(cpp_code.find("constexpr uint8_t HEADER_SIZE = 4;") != std::string::npos);
            CHECK(cpp_code.find("constexpr uint8_t DATA_SIZE = 100;") != std::string::npos);

            // Both arrays use constant names
            CHECK(cpp_code.find("std::array<uint8_t, HEADER_SIZE> header;") != std::string::npos);
            CHECK(cpp_code.find("std::array<uint32_t, DATA_SIZE> data;") != std::string::npos);
        }
    }

    TEST_CASE("Constant type selection") {
        const char* schema = R"(
const uint8 SMALL = 10;
const uint16 MEDIUM = 1000;
const uint32 LARGE = 100000;

struct TypeTest {
    uint8 a[SMALL];
    uint8 b[MEDIUM];
    uint8 c[LARGE];
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

        // Verify appropriate types chosen for constants
        CHECK(cpp_code.find("constexpr uint8_t SMALL = 10;") != std::string::npos);
        CHECK(cpp_code.find("constexpr uint16_t MEDIUM = 1000;") != std::string::npos);
        CHECK(cpp_code.find("constexpr uint32_t LARGE = 100000;") != std::string::npos);
    }

    TEST_CASE("Mixed literal and constant array sizes") {
        const char* schema = R"(
const uint16 SIZE = 10;

struct Mixed {
    uint8 literal_array[5];
    uint32 const_array[SIZE];
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

        // Literal uses number
        CHECK(cpp_code.find("std::array<uint8_t, 5> literal_array;") != std::string::npos);

        // Constant uses name
        CHECK(cpp_code.find("std::array<uint32_t, SIZE> const_array;") != std::string::npos);

        // Literal loop uses number
        CHECK(cpp_code.find("for (size_t i = 0; i < static_cast<size_t>(5); i++)") != std::string::npos);

        // Constant loop uses name
        CHECK(cpp_code.find("for (size_t i = 0; i < static_cast<size_t>(SIZE); i++)") != std::string::npos);
    }

    TEST_CASE("Constant array with different element types") {
        const char* schema = R"(
const uint8 COUNT = 8;

struct MultiType {
    uint8 bytes[COUNT];
    uint16 words[COUNT];
    uint32 dwords[COUNT];
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

        // All arrays use same constant name with different element types
        CHECK(cpp_code.find("std::array<uint8_t, COUNT> bytes;") != std::string::npos);
        CHECK(cpp_code.find("std::array<uint16_t, COUNT> words;") != std::string::npos);
        CHECK(cpp_code.find("std::array<uint32_t, COUNT> dwords;") != std::string::npos);
    }

    TEST_CASE("Constant with big-endian array") {
        const char* schema = R"(
const uint16 SIZE = 10;

struct BigEndianArray {
    big uint32 values[SIZE];
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

        // Verify constant name used with big-endian
        CHECK(cpp_code.find("std::array<uint32_t, SIZE> values;") != std::string::npos);
        CHECK(cpp_code.find("read_uint32_be(data, end)") != std::string::npos);
    }

    TEST_CASE("Exception-only mode with constants") {
        const char* schema = R"(
const uint16 SIZE = 10;

struct Data {
    uint32 items[SIZE];
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

        // Constant still generated in exception mode
        CHECK(cpp_code.find("constexpr uint8_t SIZE = 10;") != std::string::npos);
        CHECK(cpp_code.find("std::array<uint32_t, SIZE> items;") != std::string::npos);

        // Uses exception-based read method
        CHECK(cpp_code.find("static Data read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
    }

    TEST_CASE("Constant larger than uint8") {
        const char* schema = R"(
const uint32 BIG_SIZE = 500;

struct LargeArray {
    uint8 data[BIG_SIZE];
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

        // Constant type should be uint16_t (500 > uint8 max)
        CHECK(cpp_code.find("constexpr uint16_t BIG_SIZE = 500;") != std::string::npos);
        CHECK(cpp_code.find("std::array<uint8_t, BIG_SIZE> data;") != std::string::npos);
    }
}

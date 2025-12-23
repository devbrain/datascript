//
// Code generation tests for bit fields
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Bit Fields") {
    TEST_CASE("Simple bit fields with bit<N> syntax") {
        const char* schema = R"(
struct Flags {
    bit<3> priority;
    bit<5> reserved;
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
            const auto& flags = ir.structs[0];
            REQUIRE(flags.fields.size() == 2);

            // Verify first bitfield
            const auto& priority = flags.fields[0];
            CHECK(priority.name == "priority");
            CHECK(priority.type.kind == ir::type_kind::bitfield);
            CHECK(priority.type.bit_width.has_value());
            CHECK(*priority.type.bit_width == 3);

            // Verify second bitfield
            const auto& reserved = flags.fields[1];
            CHECK(reserved.name == "reserved");
            CHECK(reserved.type.kind == ir::type_kind::bitfield);
            CHECK(reserved.type.bit_width.has_value());
            CHECK(*reserved.type.bit_width == 5);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify types (both should be uint8_t since <= 8 bits)
            CHECK(cpp_code.find("uint8_t priority;") != std::string::npos);
            CHECK(cpp_code.find("uint8_t reserved;") != std::string::npos);

            // Verify bitfield batching: should read byte once and extract both fields
            CHECK(cpp_code.find("// Reading bitfield sequence") != std::string::npos);
            CHECK(cpp_code.find("uint8_t _bitfield_byte0{};") != std::string::npos);
            CHECK(cpp_code.find("_bitfield_byte0 = read_uint8(data, end);") != std::string::npos);
            CHECK(cpp_code.find("obj.priority = (_bitfield_byte0 & 0x7);") != std::string::npos);  // 0x7 = 0b111
            CHECK(cpp_code.find("obj.reserved = ((_bitfield_byte0 >> 3) & 0x1F);") != std::string::npos); // 0x1F = 0b11111, shifted by 3 bits
        }
    }

    TEST_CASE("Bit fields of different widths") {
        const char* schema = R"(
struct BitSizes {
    bit<4> nibble;
    bit<12> medium;
    bit<24> large;
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

        // Verify type selection based on bit width
        CHECK(cpp_code.find("uint8_t nibble;") != std::string::npos);    // 4 bits -> uint8_t
        CHECK(cpp_code.find("uint16_t medium;") != std::string::npos);   // 12 bits -> uint16_t
        CHECK(cpp_code.find("uint32_t large;") != std::string::npos);    // 24 bits -> uint32_t

        // Verify bitfield batching: 4+12+24 = 40 bits = 5 bytes total
        CHECK(cpp_code.find("// Reading bitfield sequence") != std::string::npos);
        // Should read 5 bytes total for all three fields
        CHECK(cpp_code.find("uint8_t _bitfield_byte0{};") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte1{};") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte2{};") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte3{};") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte4{};") != std::string::npos);
    }

    TEST_CASE("Bit field type boundaries") {
        const char* schema = R"(
struct Boundaries {
    bit<8> max_8;
    bit<9> min_16;
    bit<16> max_16;
    bit<17> min_32;
    bit<32> max_32;
    bit<33> min_64;
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

        // Verify boundary conditions for type selection
        CHECK(cpp_code.find("uint8_t max_8;") != std::string::npos);
        CHECK(cpp_code.find("uint16_t min_16;") != std::string::npos);
        CHECK(cpp_code.find("uint16_t max_16;") != std::string::npos);
        CHECK(cpp_code.find("uint32_t min_32;") != std::string::npos);
        CHECK(cpp_code.find("uint32_t max_32;") != std::string::npos);
        CHECK(cpp_code.find("uint64_t min_64;") != std::string::npos);
    }

    TEST_CASE("Bit fields mixed with regular fields") {
        const char* schema = R"(
struct Mixed {
    uint8 header;
    bit<4> flags;
    uint32 data;
    bit<1> valid;
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
            const auto& mixed = ir.structs[0];
            REQUIRE(mixed.fields.size() == 4);

            CHECK(mixed.fields[0].type.kind == ir::type_kind::uint8);
            CHECK(mixed.fields[1].type.kind == ir::type_kind::bitfield);
            CHECK(mixed.fields[2].type.kind == ir::type_kind::uint32);
            CHECK(mixed.fields[3].type.kind == ir::type_kind::bitfield);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify all fields present
            CHECK(cpp_code.find("uint8_t header;") != std::string::npos);
            CHECK(cpp_code.find("uint8_t flags;") != std::string::npos);
            CHECK(cpp_code.find("uint32_t data;") != std::string::npos);
            CHECK(cpp_code.find("uint8_t valid;") != std::string::npos);

            // Verify reading order
            size_t header_pos = cpp_code.find("obj.header =");
            size_t flags_pos = cpp_code.find("obj.flags =");
            size_t data_pos = cpp_code.find("obj.data =");
            size_t valid_pos = cpp_code.find("obj.valid =");

            CHECK(header_pos < flags_pos);
            CHECK(flags_pos < data_pos);
            CHECK(data_pos < valid_pos);
        }
    }

    TEST_CASE("Single bit field (bit<1>)") {
        const char* schema = R"(
struct SingleBit {
    bit<1> flag;
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

        // Verify single bit handling with batching
        CHECK(cpp_code.find("uint8_t flag;") != std::string::npos);
        CHECK(cpp_code.find("// Reading bitfield sequence") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte0{};") != std::string::npos);
        CHECK(cpp_code.find("_bitfield_byte0 = read_uint8(data, end);") != std::string::npos);
        CHECK(cpp_code.find("obj.flag = (_bitfield_byte0 & 0x1);") != std::string::npos); // 0x1 = 0b1
    }

    TEST_CASE("Exception-only error handling with bitfields") {
        const char* schema = R"(
struct BitFlags {
    bit<3> priority;
    bit<5> flags;
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
        CHECK(cpp_code.find("static BitFlags read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);

        // Verify bitfield batching: should read byte once and extract both fields
        CHECK(cpp_code.find("// Reading bitfield sequence") != std::string::npos);
        CHECK(cpp_code.find("uint8_t _bitfield_byte0{};") != std::string::npos);
        CHECK(cpp_code.find("_bitfield_byte0 = read_uint8(data, end);") != std::string::npos);
        CHECK(cpp_code.find("obj.priority = (_bitfield_byte0 & 0x7);") != std::string::npos);
        CHECK(cpp_code.find("obj.flags = ((_bitfield_byte0 >> 3) & 0x1F);") != std::string::npos);

        // Should not have read_safe
        CHECK(cpp_code.find("read_safe(") == std::string::npos);
    }
}

//
// Comprehensive integration tests for code generation
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Comprehensive Integration") {
    TEST_CASE("Complex schema with multiple features") {
        const char* schema = R"(
const uint32 MAGIC = 0x12345678;
const uint8 VERSION = 1;

enum uint8 MessageType {
    REQUEST = 1,
    RESPONSE = 2
};

struct Point {
    uint32 x;
    uint32 y;
};

struct Rectangle {
    Point top_left;
    Point bottom_right;
};

struct Message {
    uint32 magic;
    MessageType msg_type;
    uint16 payload_size;
    Point origin;
    Rectangle bounds;
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

        SUBCASE("IR structure is correct") {
            CHECK(ir.constants.size() == 2);
            CHECK(ir.enums.size() == 1);
            CHECK(ir.structs.size() == 3);

            // Verify Message struct has correct fields
            const auto& message = ir.structs[2];
            CHECK(message.name == "Message");
            CHECK(message.fields.size() == 5);

            // Verify nested struct types are resolved correctly
            const auto& origin_field = message.fields[3];
            CHECK(origin_field.name == "origin");
            CHECK(origin_field.type.kind == ir::type_kind::struct_type);

            const auto& bounds_field = message.fields[4];
            CHECK(bounds_field.name == "bounds");
            CHECK(bounds_field.type.kind == ir::type_kind::struct_type);
        }

        SUBCASE("Generated code has all features") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Constants
            CHECK(cpp_code.find("constexpr uint32_t MAGIC =") != std::string::npos);
            CHECK(cpp_code.find("constexpr uint8_t VERSION =") != std::string::npos);

            // Enum
            CHECK(cpp_code.find("enum class MessageType : uint8_t {") != std::string::npos);

            // Structs
            CHECK(cpp_code.find("struct Point {") != std::string::npos);
            CHECK(cpp_code.find("struct Rectangle {") != std::string::npos);
            CHECK(cpp_code.find("struct Message {") != std::string::npos);

            // Nested struct fields
            CHECK(cpp_code.find("Point origin;") != std::string::npos);
            CHECK(cpp_code.find("Rectangle bounds;") != std::string::npos);

            // Dual error handling
            CHECK(cpp_code.find("ReadResult<Message> read_safe") != std::string::npos);
            CHECK(cpp_code.find("static Message read(") != std::string::npos);

            // Nested reads
            CHECK(cpp_code.find("Point::read_safe(data, end)") != std::string::npos);
            CHECK(cpp_code.find("Rectangle::read_safe(data, end)") != std::string::npos);
        }

        SUBCASE("Type resolution is correct") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Find Message struct definition
            size_t msg_pos = cpp_code.find("struct Message {");
            REQUIRE(msg_pos != std::string::npos);

            size_t read_pos = cpp_code.find("static ReadResult<Message> read_safe", msg_pos);
            REQUIRE(read_pos != std::string::npos);

            std::string msg_struct = cpp_code.substr(msg_pos, read_pos - msg_pos);

            // Verify field types are correct
            CHECK(msg_struct.find("Point origin;") != std::string::npos);
            CHECK(msg_struct.find("Rectangle bounds;") != std::string::npos);

            // Should NOT have wrong types
            CHECK(msg_struct.find("Point bounds;") == std::string::npos);
        }
    }

    TEST_CASE("Real-world PE file format subset") {
        const char* schema = R"(
const uint16 DOS_SIGNATURE = 0x5A4D;

struct ImageDosHeader {
    uint16 e_magic;
    uint16 e_cblp;
    uint16 e_cp;
    uint32 e_lfanew;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "pe.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "pe";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "pe";
        opts.error_handling = codegen::cpp_options::both;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify generates usable code
        CHECK(cpp_code.find("namespace pe {") != std::string::npos);
        CHECK(cpp_code.find("struct ImageDosHeader {") != std::string::npos);
        CHECK(cpp_code.find("ReadResult<ImageDosHeader>") != std::string::npos);
    }
}

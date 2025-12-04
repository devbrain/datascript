//
// Code generation tests for subtypes
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Subtypes") {

    // ========================================
    // Basic Subtype Code Generation
    // ========================================

    TEST_CASE("Simple subtype with uint16") {
        const char* schema = R"(
subtype uint16 UserID : this > 0;
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
            REQUIRE(ir.subtypes.size() == 1);
            const auto& user_id = ir.subtypes[0];
            CHECK(user_id.name == "UserID");
            CHECK(user_id.base_type.kind == ir::type_kind::uint16);

            // Check constraint expression exists
            CHECK(user_id.constraint.type == ir::expr::binary_op);
            CHECK(user_id.constraint.op == ir::expr::gt);
        }

        SUBCASE("Code generation - type alias") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify type alias generation
            CHECK(cpp_code.find("using UserID = uint16_t;") != std::string::npos);
        }

        SUBCASE("Code generation - validation function") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify validation function
            CHECK(cpp_code.find("inline bool validate_UserID(uint16_t value)") != std::string::npos);
            CHECK(cpp_code.find("value > 0") != std::string::npos);
        }

        SUBCASE("Code generation - reader function") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify reader function (exception mode)
            CHECK(cpp_code.find("inline UserID read_UserID(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
            CHECK(cpp_code.find("if (!validate_UserID(value))") != std::string::npos);
        }
    }

    TEST_CASE("Subtype with complex constraint") {
        const char* schema = R"(
subtype uint16 Port : this > 1024 && this < 65535;
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
            REQUIRE(ir.subtypes.size() == 1);
            const auto& port = ir.subtypes[0];
            CHECK(port.name == "Port");

            // Check constraint is logical AND
            CHECK(port.constraint.type == ir::expr::binary_op);
            CHECK(port.constraint.op == ir::expr::logical_and);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify complex constraint is rendered
            CHECK(cpp_code.find("validate_Port") != std::string::npos);
            CHECK(cpp_code.find("value > 1024") != std::string::npos);
            CHECK(cpp_code.find("value < 65535") != std::string::npos);
        }
    }

    // ========================================
    // Different Base Types
    // ========================================

    TEST_CASE("Subtypes with different base types") {
        const char* schema = R"(
subtype uint8 Percentage : this <= 100;
subtype uint32 Count : this > 0;
subtype uint64 Timestamp : this != 0;
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        REQUIRE(ir.subtypes.size() == 3);
        CHECK(ir.subtypes[0].name == "Percentage");
        CHECK(ir.subtypes[0].base_type.kind == ir::type_kind::uint8);
        CHECK(ir.subtypes[1].name == "Count");
        CHECK(ir.subtypes[1].base_type.kind == ir::type_kind::uint32);
        CHECK(ir.subtypes[2].name == "Timestamp");
        CHECK(ir.subtypes[2].base_type.kind == ir::type_kind::uint64);

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify type aliases
        CHECK(cpp_code.find("using Percentage = uint8_t;") != std::string::npos);
        CHECK(cpp_code.find("using Count = uint32_t;") != std::string::npos);
        CHECK(cpp_code.find("using Timestamp = uint64_t;") != std::string::npos);

        // Verify validation functions
        CHECK(cpp_code.find("validate_Percentage(uint8_t value)") != std::string::npos);
        CHECK(cpp_code.find("validate_Count(uint32_t value)") != std::string::npos);
        CHECK(cpp_code.find("validate_Timestamp(uint64_t value)") != std::string::npos);
    }

    // ========================================
    // Subtypes with Documentation
    // ========================================

    TEST_CASE("Subtype with docstring") {
        const char* schema = R"(
/** User identifier, must be positive */
subtype uint16 UserID : this > 0;
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        REQUIRE(ir.subtypes.size() == 1);
        CHECK(ir.subtypes[0].documentation == "User identifier, must be positive");

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify docstring is included
        CHECK(cpp_code.find("User identifier, must be positive") != std::string::npos);
    }

    // ========================================
    // Subtypes Using Constants
    // ========================================

    TEST_CASE("Subtype referencing constants") {
        const char* schema = R"(
const uint16 MIN_PORT = 1024;
const uint16 MAX_PORT = 65535;

subtype uint16 Port : this > MIN_PORT && this < MAX_PORT;
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        REQUIRE(ir.subtypes.size() == 1);
        REQUIRE(ir.constants.size() == 2);

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify constants are generated
        CHECK(cpp_code.find("constexpr uint16_t MIN_PORT = 1024;") != std::string::npos);
        CHECK(cpp_code.find("constexpr uint16_t MAX_PORT = 65535;") != std::string::npos);

        // Verify constants are used in validation
        CHECK(cpp_code.find("validate_Port") != std::string::npos);
        CHECK(cpp_code.find("MIN_PORT") != std::string::npos);
        CHECK(cpp_code.find("MAX_PORT") != std::string::npos);
    }

    // ========================================
    // Subtypes in Structs
    // ========================================

    TEST_CASE("Struct using subtype fields") {
        const char* schema = R"(
subtype uint16 UserID : this > 0;
subtype uint16 Port : this > 1024;

struct Connection {
    UserID user;
    Port port;
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

        REQUIRE(ir.subtypes.size() == 2);
        REQUIRE(ir.structs.size() == 1);

        const auto& conn_struct = ir.structs[0];
        CHECK(conn_struct.name == "Connection");
        REQUIRE(conn_struct.fields.size() == 2);

        // Check that fields reference the subtypes
        CHECK(conn_struct.fields[0].name == "user");
        CHECK(conn_struct.fields[1].name == "port");

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify struct uses subtype names
        CHECK(cpp_code.find("struct Connection") != std::string::npos);
        // Subtypes are type aliases, so fields will use the underlying type name in IR
        // but the subtype validation functions should still be generated
        CHECK(cpp_code.find("validate_UserID") != std::string::npos);
        CHECK(cpp_code.find("validate_Port") != std::string::npos);
    }

    // ========================================
    // Error Handling Modes
    // ========================================

    TEST_CASE("Subtype code generation - exception mode") {
        const char* schema = R"(
subtype uint16 UserID : this > 0;
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

        // Verify exception-based reader
        CHECK(cpp_code.find("inline UserID read_UserID(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
        CHECK(cpp_code.find("throw std::runtime_error") != std::string::npos);

        // Should NOT have safe mode reader
        CHECK(cpp_code.find("read_UserID_safe") == std::string::npos);
    }

    TEST_CASE("Subtype code generation - safe mode") {
        const char* schema = R"(
subtype uint16 UserID : this > 0;
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

        // Verify safe mode reader
        CHECK(cpp_code.find("inline ReadResult<UserID> read_UserID_safe") != std::string::npos);
        CHECK(cpp_code.find("ReadResult<UserID>::error") != std::string::npos);

        // Should NOT have exception-based reader method (but helper functions can throw for bounds checking)
        CHECK(cpp_code.find("inline UserID read_UserID(") == std::string::npos);
    }

    TEST_CASE("Subtype code generation - both modes") {
        const char* schema = R"(
subtype uint16 UserID : this > 0;
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
        opts.error_handling = codegen::cpp_options::both;
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify both readers are generated
        CHECK(cpp_code.find("inline UserID read_UserID(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
        CHECK(cpp_code.find("inline ReadResult<UserID> read_UserID_safe") != std::string::npos);
    }

    // ========================================
    // Complex Integration Tests
    // ========================================

    TEST_CASE("Complex schema with subtypes") {
        const char* schema = R"(
const uint16 MIN_PORT = 1024;

subtype uint16 UserID : this > 0;
subtype uint16 Port : this > MIN_PORT;
subtype uint8 Percentage : this <= 100;

struct User {
    UserID id;
    uint32 flags;
};

struct Server {
    Port port;
    Percentage cpu_usage;
    User admin;
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

        REQUIRE(ir.constants.size() == 1);
        REQUIRE(ir.subtypes.size() == 3);
        REQUIRE(ir.structs.size() == 2);

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify all subtypes are generated
        CHECK(cpp_code.find("using UserID = uint16_t;") != std::string::npos);
        CHECK(cpp_code.find("using Port = uint16_t;") != std::string::npos);
        CHECK(cpp_code.find("using Percentage = uint8_t;") != std::string::npos);

        // Verify all validation functions
        CHECK(cpp_code.find("validate_UserID") != std::string::npos);
        CHECK(cpp_code.find("validate_Port") != std::string::npos);
        CHECK(cpp_code.find("validate_Percentage") != std::string::npos);

        // Verify structs use subtypes
        CHECK(cpp_code.find("struct User") != std::string::npos);
        CHECK(cpp_code.find("struct Server") != std::string::npos);
    }

} // TEST_SUITE

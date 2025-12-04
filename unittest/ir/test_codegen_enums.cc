//
// Code generation tests for enums
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Enums") {
    TEST_CASE("Simple enum with uint8") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1,
    PENDING = 2
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
            REQUIRE(ir.enums.size() == 1);
            const auto& status = ir.enums[0];
            CHECK(status.name == "Status");
            CHECK(status.items.size() == 3);
            CHECK(status.items[0].name == "OK");
            CHECK(status.items[0].value == 0);
            CHECK(status.items[1].name == "ERROR");
            CHECK(status.items[1].value == 1);
            CHECK(status.items[2].name == "PENDING");
            CHECK(status.items[2].value == 2);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify enum class generation
            CHECK(cpp_code.find("enum class Status : uint8_t {") != std::string::npos);
            CHECK(cpp_code.find("OK = 0,") != std::string::npos);
            CHECK(cpp_code.find("ERROR = 1,") != std::string::npos);
            CHECK(cpp_code.find("PENDING = 2") != std::string::npos);
        }
    }

    TEST_CASE("Enum with different base types") {
        const char* schema = R"(
enum uint8 Small { A = 1 };
enum uint16 Medium { B = 1000 };
enum uint32 Large { C = 100000 };
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

        // Verify base type selection
        CHECK(cpp_code.find("enum class Small : uint8_t") != std::string::npos);
        CHECK(cpp_code.find("enum class Medium : uint16_t") != std::string::npos);
        CHECK(cpp_code.find("enum class Large : uint32_t") != std::string::npos);
    }

    TEST_CASE("Enum field in struct") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

struct Message {
    Status status;
    uint32 data;
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
            const auto& msg = ir.structs[0];
            REQUIRE(msg.fields.size() == 2);

            const auto& status_field = msg.fields[0];
            CHECK(status_field.name == "status");
            CHECK(status_field.type.kind == ir::type_kind::enum_type);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify enum field declaration
            CHECK(cpp_code.find("Status status;") != std::string::npos);

            // Verify enum reading with cast
            CHECK(cpp_code.find("obj.status = static_cast<Status>(read_uint8(data, end));") != std::string::npos);
        }
    }

    TEST_CASE("Enum array") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

struct MultiStatus {
    Status statuses[5];
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

        // Verify enum array declaration
        CHECK(cpp_code.find("std::array<Status, 5> statuses;") != std::string::npos);

        // Verify enum array reading
        CHECK(cpp_code.find("for (size_t i = 0; i < 5; i++)") != std::string::npos);
        CHECK(cpp_code.find("obj.statuses[i] = static_cast<Status>(read_uint8(data, end));") != std::string::npos);
    }

    TEST_CASE("Enum with non-sequential values") {
        const char* schema = R"(
enum uint16 HttpStatus {
    OK = 200,
    NOT_FOUND = 404,
    SERVER_ERROR = 500
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
            REQUIRE(ir.enums.size() == 1);
            const auto& http = ir.enums[0];
            CHECK(http.items[0].value == 200);
            CHECK(http.items[1].value == 404);
            CHECK(http.items[2].value == 500);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            CHECK(cpp_code.find("OK = 200,") != std::string::npos);
            CHECK(cpp_code.find("NOT_FOUND = 404,") != std::string::npos);
            CHECK(cpp_code.find("SERVER_ERROR = 500") != std::string::npos);
        }
    }

    TEST_CASE("Multiple enums") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

enum uint8 Priority {
    LOW = 0,
    MEDIUM = 1,
    HIGH = 2
};

struct Task {
    Status status;
    Priority priority;
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
            REQUIRE(ir.enums.size() == 2);
            CHECK(ir.enums[0].name == "Status");
            CHECK(ir.enums[1].name == "Priority");
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Both enums generated
            CHECK(cpp_code.find("enum class Status") != std::string::npos);
            CHECK(cpp_code.find("enum class Priority") != std::string::npos);

            // Both fields in struct
            CHECK(cpp_code.find("Status status;") != std::string::npos);
            CHECK(cpp_code.find("Priority priority;") != std::string::npos);
        }
    }

    TEST_CASE("Enum with uint32 in struct reading") {
        const char* schema = R"(
enum uint32 ErrorCode {
    SUCCESS = 0,
    FAIL = 1
};

struct Response {
    ErrorCode code;
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

        // Verify uint32 reading for enum
        CHECK(cpp_code.find("obj.code = static_cast<ErrorCode>(read_uint32_le(data, end));") != std::string::npos);
    }

    TEST_CASE("Enum conditional field") {
        const char* schema = R"(
enum uint8 Status {
    OK = 0,
    ERROR = 1
};

struct Message {
    uint8 has_status;
    Status status if has_status != 0;
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

        // Verify conditional enum field
        CHECK(cpp_code.find("if ((obj.has_status != 0))") != std::string::npos);
        CHECK(cpp_code.find("obj.status = static_cast<Status>(read_uint8(data, end));") != std::string::npos);
    }

    TEST_CASE("Exception-only mode with enums") {
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

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::exceptions_only;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Enum still generated in exception mode
        CHECK(cpp_code.find("enum class Status : uint8_t") != std::string::npos);

        // Exception-based read method
        CHECK(cpp_code.find("static Message read(const uint8_t*& data, const uint8_t* end)") != std::string::npos);
        CHECK(cpp_code.find("obj.status = static_cast<Status>(read_uint8(data, end));") != std::string::npos);
    }
}

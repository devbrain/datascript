//
// Semantic Phase 3: Type Parameter Count Validation Tests
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>

using namespace datascript;

TEST_SUITE("Semantic - Parameter Count Validation") {

    TEST_CASE("Choice - non-parameterized used with parameters") {
        const char* schema = R"(
enum uint8 MsgType {
    TEXT = 1
};

choice MessagePayload on msg_type {
    case TEXT:
        uint8 text_length;
};

struct Message {
    MsgType msg_type;
    MessagePayload(msg_type) payload;  // Error: expects 0 params, got 1
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE(analysis.has_errors());
        bool found_param_error = false;
        for (const auto& diag : analysis.diagnostics) {
            if (diag.level == semantic::diagnostic_level::error &&
                std::string(diag.code) == "E016") {
                found_param_error = true;
                CHECK(std::string(diag.message).find("expects 0 parameter(s) but got 1") != std::string::npos);
            }
        }
        REQUIRE(found_param_error);
    }

    TEST_CASE("Choice - parameterized used without parameters") {
        const char* schema = R"(
enum uint8 MsgType {
    TEXT = 1
};

choice MessagePayload(MsgType msg_type) on msg_type {
    case TEXT:
        uint8 text_length;
};

struct Message {
    MsgType msg_type;
    MessagePayload payload;  // Error: expects 1 param, got 0
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE(analysis.has_errors());
        bool found_param_error = false;
        for (const auto& diag : analysis.diagnostics) {
            if (diag.level == semantic::diagnostic_level::error &&
                std::string(diag.code) == "E016") {
                found_param_error = true;
                CHECK(std::string(diag.message).find("expects 1 parameter(s) but got 0") != std::string::npos);
            }
        }
        REQUIRE(found_param_error);
    }

    TEST_CASE("Choice - parameterized used correctly") {
        const char* schema = R"(
enum uint8 MsgType {
    TEXT = 1
};

choice MessagePayload(MsgType msg_type) on msg_type {
    case TEXT:
        uint8 text_length;
};

struct Message {
    MsgType msg_type;
    MessagePayload(msg_type) payload;  // OK: correct parameter count
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE_FALSE(analysis.has_errors());
    }

    TEST_CASE("Choice - non-parameterized used without parameters") {
        const char* schema = R"(
enum uint8 MsgType {
    TEXT = 1
};

choice MessagePayload on msg_type {
    case TEXT:
        uint8 text_length;
};

struct Message {
    MsgType msg_type;
    MessagePayload payload;  // OK: non-parameterized choice used without args
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE_FALSE(analysis.has_errors());
    }

    TEST_CASE("Struct - parameterized used without parameters") {
        const char* schema = R"(
struct Buffer(uint16 capacity) {
    uint8 data[capacity];
};

struct Message {
    Buffer buf;  // Error: expects 1 param, got 0
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE(analysis.has_errors());
        bool found_param_error = false;
        for (const auto& diag : analysis.diagnostics) {
            if (diag.level == semantic::diagnostic_level::error &&
                std::string(diag.code) == "E016") {
                found_param_error = true;
                CHECK(std::string(diag.message).find("expects 1 parameter(s) but got 0") != std::string::npos);
            }
        }
        REQUIRE(found_param_error);
    }

    TEST_CASE("Struct - non-parameterized used with parameters") {
        const char* schema = R"(
struct Header {
    uint32 magic;
};

struct Message {
    Header(16) hdr;  // Error: expects 0 params, got 1
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE(analysis.has_errors());
        bool found_param_error = false;
        for (const auto& diag : analysis.diagnostics) {
            if (diag.level == semantic::diagnostic_level::error &&
                std::string(diag.code) == "E016") {
                found_param_error = true;
                CHECK(std::string(diag.message).find("expects 0 parameter(s) but got 1") != std::string::npos);
            }
        }
        REQUIRE(found_param_error);
    }

    TEST_CASE("Struct - wrong parameter count") {
        const char* schema = R"(
struct Matrix(uint8 rows, uint8 cols) {
    uint32 data[rows * cols];
};

struct Data {
    Matrix(4) mat;  // Error: expects 2 params, got 1
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);

        REQUIRE(analysis.has_errors());
        bool found_param_error = false;
        for (const auto& diag : analysis.diagnostics) {
            if (diag.level == semantic::diagnostic_level::error &&
                std::string(diag.code) == "E016") {
                found_param_error = true;
                CHECK(std::string(diag.message).find("expects 2 parameter(s) but got 1") != std::string::npos);
            }
        }
        REQUIRE(found_param_error);
    }
}

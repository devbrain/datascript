//
// Tests for Subtype Semantic Analysis
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>

using namespace datascript;
using namespace datascript::semantic;

namespace {
    // Helper: create a module_set from a single source string
    module_set make_module_set(const std::string& source) {
        auto main_mod = parse_datascript(source);

        module_set modules;
        modules.main.module = std::move(main_mod);
        modules.main.file_path = "<test>";
        modules.main.package_name = "";  // Default package

        return modules;
    }
}

TEST_SUITE("Semantic Analysis - Subtypes") {

    // ========================================
    // Phase 1: Symbol Collection
    // ========================================

    TEST_CASE("Collect subtypes") {
        auto modules = make_module_set(R"(
            subtype uint16 UserID : this > 0;
            subtype uint16 Port : this > 1024;
            subtype uint32 SessionID : this != 0;
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.subtypes.size() == 3);
        CHECK(symbols.main.subtypes.contains("UserID"));
        CHECK(symbols.main.subtypes.contains("Port"));
        CHECK(symbols.main.subtypes.contains("SessionID"));
    }

    TEST_CASE("Detect duplicate subtype definitions") {
        auto modules = make_module_set(R"(
            subtype uint16 UserID : this > 0;
            subtype uint32 UserID : this != 0;
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        // Should have one error for duplicate definition
        REQUIRE(diags.size() == 1);
        CHECK(diags[0].code == diag_codes::E_DUPLICATE_DEFINITION);
        CHECK(diags[0].message.find("UserID") != std::string::npos);

        // Only first definition should be collected
        REQUIRE(symbols.main.subtypes.size() == 1);
        CHECK(symbols.main.subtypes.contains("UserID"));
    }

    TEST_CASE("Subtype name collision with struct") {
        auto modules = make_module_set(R"(
            struct UserID { uint32 value; };
            subtype uint16 UserID : this > 0;
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        // Should detect name collision (or at least collect both symbols)
        // Both symbols should be collected (for better error recovery)
        CHECK(symbols.main.structs.contains("UserID"));
        CHECK(symbols.main.subtypes.contains("UserID"));
    }

    // ========================================
    // Phase 2: Name Resolution
    // ========================================

    TEST_CASE("Resolve subtype base type") {
        auto modules = make_module_set(R"(
            subtype uint16 UserID : this > 0;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Use subtype as field type") {
        auto modules = make_module_set(R"(
            subtype uint16 UserID : this > 0;

            struct User {
                UserID id;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Subtype referring to constant in constraint") {
        auto modules = make_module_set(R"(
            const uint16 MIN_PORT = 1024;
            subtype uint16 Port : this > MIN_PORT;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    // ========================================
    // Phase 3: Type Checking
    // ========================================

    TEST_CASE("Type check subtype constraint") {
        auto modules = make_module_set(R"(
            subtype uint16 UserID : this > 0;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Type check complex constraint") {
        auto modules = make_module_set(R"(
            subtype uint16 Port : this > 1024 && this < 65535;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    // ========================================
    // Integration Tests
    // ========================================

    TEST_CASE("Full semantic analysis of subtypes") {
        auto modules = make_module_set(R"(
            const uint16 MIN_PORT = 1024;
            const uint16 MAX_PORT = 65535;

            subtype uint16 Port : this > MIN_PORT && this < MAX_PORT;
            subtype uint16 UserID : this > 0;
            subtype uint32 SessionID : this != 0;

            struct Connection {
                UserID user;
                Port port;
                SessionID session;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Subtype in parameterized struct") {
        auto modules = make_module_set(R"(
            subtype uint8 Percentage : this <= 100;

            struct Progress(uint8 count) {
                Percentage values[count];
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

} // TEST_SUITE

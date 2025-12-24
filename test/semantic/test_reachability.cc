//
// Tests for Phase 7: Reachability Analysis
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
        modules.main.package_name = "";

        return modules;
    }
}

TEST_SUITE("Semantic Analysis - Reachability") {
    TEST_CASE("All symbols used - no warnings") {
        auto modules = make_module_set(R"(
            const uint32 SIZE = 100;

            struct Container {
                uint32[SIZE] data;
            };

            struct Root {
                Container container;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("WARNING: Unused constant") {
        auto modules = make_module_set(R"(
            const uint32 USED = 100;
            const uint32 UNUSED = 200;

            struct Container {
                uint32[USED] data;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        auto warnings = result.get_warnings();
        bool found_unused_constant = false;
        for (const auto& warning : warnings) {
            if (warning.code == std::string(diag_codes::W_UNUSED_CONSTANT)) {
                if (warning.message.find("UNUSED") != std::string::npos) {
                    found_unused_constant = true;
                    break;
                }
            }
        }
        CHECK(found_unused_constant);
    }

    // NOTE: Removed test for unused struct because reachability analysis
    // no longer warns about unused types (they may be root/entry point types)

    // NOTE: Removed test for unused enum (same reason as unused struct)

    // NOTE: Removed test for unused union (same reason as unused struct)

    // NOTE: Removed test for unused choice (same reason as unused struct)

    TEST_CASE("Constant used in expression") {
        auto modules = make_module_set(R"(
            const uint32 BASE = 100;
            const uint32 OFFSET = 50;
            const uint32 TOTAL = BASE + OFFSET;

            struct Container {
                uint32[TOTAL] data;
            };

            struct Root {
                Container container;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("Nested type usage") {
        auto modules = make_module_set(R"(
            struct Inner {
                uint32 value;
            };

            struct Middle {
                Inner data;
            };

            struct Outer {
                Middle nested;
            };

            struct Root {
                Outer outer;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("Enum used as base type and field type") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };

            struct Container {
                Status current_status;
            };

            struct Root {
                Container container;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("Type used in array") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Path {
                Point[10] points;
            };

            struct Root {
                Path path;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("WARNING: Multiple unused symbols") {
        auto modules = make_module_set(R"(
            const uint32 UNUSED1 = 100;
            const uint32 UNUSED2 = 200;

            struct UnusedStruct {
                uint32 data;
            };

            enum uint8 UnusedEnum {
                VAL1 = 0
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        // Should have 2 warnings (2 constants)
        // Note: No warnings for unused types (struct/enum) as they may be root types
        CHECK(result.warning_count() == 2);
    }

    TEST_CASE("Constant used in enum value") {
        auto modules = make_module_set(R"(
            const uint32 ERROR_BASE = 100;

            enum uint8 Status {
                OK = 0,
                ERROR = ERROR_BASE
            };

            struct Container {
                Status status;
            };

            struct Root {
                Container container;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        CHECK_FALSE(result.has_warnings());
    }

    TEST_CASE("Constant used in field condition") {
        auto modules = make_module_set(R"(
            const bool FEATURE_ENABLED = true;

            struct Container {
                uint32 feature_data if FEATURE_ENABLED;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        // May have warning about always-true condition, but not unused constant
        auto warnings = result.get_warnings();
        for (const auto& warning : warnings) {
            CHECK(warning.code != std::string(diag_codes::W_UNUSED_CONSTANT));
        }
    }
}

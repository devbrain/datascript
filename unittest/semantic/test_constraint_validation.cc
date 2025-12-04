//
// Tests for Phase 6: Constraint Validation
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

TEST_SUITE("Semantic Analysis - Constraint Validation") {
    TEST_CASE("Valid constraint with expression") {
        auto modules = make_module_set(R"(
            constraint ValidRange(uint32 value) {
                value >= 0 && value <= 100
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("WARNING: Always-true constraint") {
        auto modules = make_module_set(R"(
            constraint AlwaysTrue(uint32 value) {
                true
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        auto warnings = result.get_warnings();
        bool found_always_true = false;
        for (const auto& warning : warnings) {
            if (warning.message.find("always true") != std::string::npos) {
                found_always_true = true;
                break;
            }
        }
        CHECK(found_always_true);
    }

    TEST_CASE("ERROR: Always-false constraint") {
        auto modules = make_module_set(R"(
            constraint AlwaysFalse(uint32 value) {
                false
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_always_false = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_CONSTRAINT_VIOLATION)) {
                if (error.message.find("always false") != std::string::npos) {
                    found_always_false = true;
                    break;
                }
            }
        }
        CHECK(found_always_false);
    }

    TEST_CASE("Constraint with multiple parameters") {
        auto modules = make_module_set(R"(
            constraint RangeCheck(uint32 value, uint32 min, uint32 max) {
                value >= min && value <= max
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Constraint with comparison") {
        auto modules = make_module_set(R"(
            constraint PositiveValue(int32 value) {
                value > 0
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("WARNING: Struct field with always-false condition") {
        auto modules = make_module_set(R"(
            struct Container {
                uint32 data if false;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        auto warnings = result.get_warnings();
        bool found_always_false = false;
        for (const auto& warning : warnings) {
            if (warning.message.find("always false") != std::string::npos) {
                found_always_false = true;
                break;
            }
        }
        CHECK(found_always_false);
    }

    TEST_CASE("Struct field with valid condition") {
        auto modules = make_module_set(R"(
            const bool FLAG = true;
            struct Container {
                uint32 data if FLAG;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("WARNING: Union field with always-false condition") {
        auto modules = make_module_set(R"(
            union Value {
                uint32 int_val if false;
                string str_val;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        auto warnings = result.get_warnings();
        bool found_always_false = false;
        for (const auto& warning : warnings) {
            if (warning.message.find("always false") != std::string::npos) {
                found_always_false = true;
                break;
            }
        }
        CHECK(found_always_false);
    }

    TEST_CASE("Choice with valid cases") {
        auto modules = make_module_set(R"(
            choice Data on selector {
                case 1: uint32 int_field;
                case 2: string str_field;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("WARNING: Choice case field with always-false condition") {
        auto modules = make_module_set(R"(
            choice Data on selector {
                case 1: uint32 int_field if false;
                case 2: string str_field;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.has_warnings());

        auto warnings = result.get_warnings();
        bool found_always_false = false;
        for (const auto& warning : warnings) {
            if (warning.message.find("always false") != std::string::npos) {
                found_always_false = true;
                break;
            }
        }
        CHECK(found_always_false);
    }

    TEST_CASE("Multiple constraints") {
        auto modules = make_module_set(R"(
            constraint Positive(int32 value) {
                value > 0
            };

            constraint InRange(uint32 value) {
                value >= 10 && value <= 100
            };

            constraint NonZero(uint32 value) {
                value != 0
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Constraint with logical operators and constants") {
        auto modules = make_module_set(R"(
            const uint32 MIN_VALUE = 10;
            const uint32 MAX_VALUE = 100;
            const bool ENABLED = true;

            constraint RangeWithFlag(uint32 value) {
                (value >= MIN_VALUE || ENABLED) && value <= MAX_VALUE
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }
}

//
// Tests for Phase 4: Constant Evaluation
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

TEST_SUITE("Semantic Analysis - Constant Evaluation") {
    TEST_CASE("Evaluate simple integer literal") {
        auto modules = make_module_set(R"(
            const uint32 VALUE = 42;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Check that constant was evaluated
        auto& analyzed = result.analyzed.value();
        auto& const_def = modules.main.module.constants[0];
        CHECK(analyzed.constant_values.contains(&const_def));
        CHECK(analyzed.constant_values[&const_def] == 42);
    }

    TEST_CASE("Evaluate arithmetic operations") {
        auto modules = make_module_set(R"(
            const uint32 SUM = 10 + 20;
            const uint32 DIFF = 100 - 50;
            const uint32 PROD = 5 * 6;
            const uint32 QUOT = 20 / 4;
            const uint32 MOD = 17 % 5;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 30);   // SUM
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 50);   // DIFF
        CHECK(analyzed.constant_values[&modules.main.module.constants[2]] == 30);   // PROD
        CHECK(analyzed.constant_values[&modules.main.module.constants[3]] == 5);    // QUOT
        CHECK(analyzed.constant_values[&modules.main.module.constants[4]] == 2);    // MOD
    }

    TEST_CASE("Evaluate bitwise operations") {
        auto modules = make_module_set(R"(
            const uint32 AND = 0xFF & 0x0F;
            const uint32 OR = 0xF0 | 0x0F;
            const uint32 XOR = 0xFF ^ 0x0F;
            const uint32 LSHIFT = 1 << 4;
            const uint32 RSHIFT = 0xFF >> 2;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 0x0F);  // AND
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 0xFF);  // OR
        CHECK(analyzed.constant_values[&modules.main.module.constants[2]] == 0xF0);  // XOR
        CHECK(analyzed.constant_values[&modules.main.module.constants[3]] == 16);    // LSHIFT
        CHECK(analyzed.constant_values[&modules.main.module.constants[4]] == 0x3F);  // RSHIFT
    }

    TEST_CASE("Evaluate unary operations") {
        auto modules = make_module_set(R"(
            const int32 NEG = -42;
            const uint32 POS = +42;
            const uint32 NOT = ~0;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(static_cast<int64_t>(analyzed.constant_values[&modules.main.module.constants[0]]) == -42);  // NEG
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 42);                          // POS
        CHECK(analyzed.constant_values[&modules.main.module.constants[2]] == static_cast<uint64_t>(-1));  // NOT
    }

    TEST_CASE("Evaluate complex expression") {
        auto modules = make_module_set(R"(
            const uint32 RESULT = (10 + 20) * 3 - 5;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 85);  // (30 * 3) - 5 = 85
    }

    TEST_CASE("Evaluate constant reference") {
        auto modules = make_module_set(R"(
            const uint32 BASE = 100;
            const uint32 DERIVED = BASE + 50;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 100);  // BASE
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 150);  // DERIVED
    }

    TEST_CASE("Evaluate nested constant references") {
        auto modules = make_module_set(R"(
            const uint32 A = 10;
            const uint32 B = A * 2;
            const uint32 C = B + A;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 10);  // A
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 20);  // B
        CHECK(analyzed.constant_values[&modules.main.module.constants[2]] == 30);  // C
    }

    TEST_CASE("ERROR: Division by zero") {
        auto modules = make_module_set(R"(
            const uint32 INVALID = 100 / 0;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_div_by_zero = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_DIVISION_BY_ZERO)) {
                found_div_by_zero = true;
                break;
            }
        }
        CHECK(found_div_by_zero);
    }

    TEST_CASE("ERROR: Modulo by zero") {
        auto modules = make_module_set(R"(
            const uint32 INVALID = 17 % 0;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_div_by_zero = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_DIVISION_BY_ZERO)) {
                found_div_by_zero = true;
                break;
            }
        }
        CHECK(found_div_by_zero);
    }

    TEST_CASE("ERROR: Circular constant dependency") {
        auto modules = make_module_set(R"(
            const uint32 A = B + 1;
            const uint32 B = A + 1;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_circular = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_CIRCULAR_CONSTANT)) {
                found_circular = true;
                break;
            }
        }
        CHECK(found_circular);
    }

    TEST_CASE("ERROR: Self-referential constant") {
        auto modules = make_module_set(R"(
            const uint32 SELF = SELF + 1;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_circular = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_CIRCULAR_CONSTANT)) {
                found_circular = true;
                break;
            }
        }
        CHECK(found_circular);
    }

    TEST_CASE("Hexadecimal literals") {
        auto modules = make_module_set(R"(
            const uint32 HEX1 = 0xFF;
            const uint32 HEX2 = 0x100;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 255);  // 0xFF
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 256);  // 0x100
    }

    TEST_CASE("Negative number expressions") {
        auto modules = make_module_set(R"(
            const int32 NEG1 = -42;
            const int32 NEG2 = -100 + 50;
            const int32 NEG3 = 10 - 60;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(static_cast<int64_t>(analyzed.constant_values[&modules.main.module.constants[0]]) == -42);  // NEG1
        CHECK(static_cast<int64_t>(analyzed.constant_values[&modules.main.module.constants[1]]) == -50);  // NEG2
        CHECK(static_cast<int64_t>(analyzed.constant_values[&modules.main.module.constants[2]]) == -50);  // NEG3
    }

    TEST_CASE("Precedence: multiplication before addition") {
        auto modules = make_module_set(R"(
            const uint32 RESULT = 2 + 3 * 4;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 14);  // 2 + (3 * 4) = 14
    }

    TEST_CASE("Parentheses override precedence") {
        auto modules = make_module_set(R"(
            const uint32 RESULT = (2 + 3) * 4;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 20);  // (2 + 3) * 4 = 20
    }

    TEST_CASE("Bitwise operations with constants") {
        auto modules = make_module_set(R"(
            const uint32 MASK = 0x0F;
            const uint32 VALUE = 0xFF;
            const uint32 MASKED = VALUE & MASK;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.constant_values[&modules.main.module.constants[0]] == 0x0F);  // MASK
        CHECK(analyzed.constant_values[&modules.main.module.constants[1]] == 0xFF);  // VALUE
        CHECK(analyzed.constant_values[&modules.main.module.constants[2]] == 0x0F);  // MASKED
    }

    TEST_CASE("Enum values with implicit numbering") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK,
                WARNING,
                ERROR
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
        // Enum implicit values: OK=0, WARNING=1, ERROR=2
    }

    TEST_CASE("Enum values with explicit numbering") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                WARNING = 10,
                ERROR = 20
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Enum values with computed expressions") {
        auto modules = make_module_set(R"(
            const uint32 BASE = 100;
            enum uint8 Status {
                OK = 0,
                WARNING = BASE + 10,
                ERROR = BASE + 20
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }
}

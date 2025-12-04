//
// Tests for Phase 3: Type Checking
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

TEST_SUITE("Semantic Analysis - Type Checking") {
    TEST_CASE("Integer literal type") {
        auto modules = make_module_set(R"(
            const uint32 VALUE = 42;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Boolean literal type") {
        auto modules = make_module_set(R"(
            const bool FLAG = true;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("String literal type") {
        auto modules = make_module_set(R"(
            const string NAME = "test";
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Arithmetic operations require integer operands") {
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
    }

    TEST_CASE("Bitwise operations require integer operands") {
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
    }

    TEST_CASE("Comparison operations return boolean") {
        auto modules = make_module_set(R"(
            const bool EQ = 10 == 10;
            const bool NE = 10 != 20;
            const bool LT = 5 < 10;
            const bool GT = 10 > 5;
            const bool LE = 5 <= 10;
            const bool GE = 10 >= 5;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Logical operations require boolean operands") {
        auto modules = make_module_set(R"(
            const bool AND = true && false;
            const bool OR = true || false;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Unary operators on integers") {
        auto modules = make_module_set(R"(
            const int32 NEG = -42;
            const uint32 POS = +42;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Unary logical not on boolean") {
        auto modules = make_module_set(R"(
            const bool NOT = !true;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("ERROR: Arithmetic on non-integer") {
        auto modules = make_module_set(R"(
            const uint32 INVALID = true + false;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        CHECK(errors.size() >= 1);

        bool found_invalid_operand = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_INVALID_OPERAND_TYPE)) {
                found_invalid_operand = true;
                break;
            }
        }
        CHECK(found_invalid_operand);
    }

    TEST_CASE("ERROR: Logical operation on non-boolean") {
        auto modules = make_module_set(R"(
            const bool INVALID = 10 && 20;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_invalid_operand = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_INVALID_OPERAND_TYPE)) {
                found_invalid_operand = true;
                break;
            }
        }
        CHECK(found_invalid_operand);
    }

    TEST_CASE("ERROR: Type mismatch in constant") {
        auto modules = make_module_set(R"(
            const uint32 INVALID = "not a number";
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_type_mismatch = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_TYPE_MISMATCH)) {
                found_type_mismatch = true;
                break;
            }
        }
        CHECK(found_type_mismatch);
    }

    TEST_CASE("ERROR: Boolean type mismatch") {
        auto modules = make_module_set(R"(
            const bool INVALID = 42;
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_type_mismatch = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_TYPE_MISMATCH)) {
                found_type_mismatch = true;
                break;
            }
        }
        CHECK(found_type_mismatch);
    }

    TEST_CASE("Enum base type must be integer") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("ERROR: Enum base type must be integer, not string") {
        auto modules = make_module_set(R"(
            enum string Status {
                OK = 0,
                ERROR = 1
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_type_mismatch = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_TYPE_MISMATCH)) {
                if (error.message.find("Enum base type") != std::string::npos) {
                    found_type_mismatch = true;
                    break;
                }
            }
        }
        CHECK(found_type_mismatch);
    }

    TEST_CASE("ERROR: Enum base type must be integer, not bool") {
        auto modules = make_module_set(R"(
            enum bool Status {
                OK = 0,
                ERROR = 1
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_type_mismatch = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_TYPE_MISMATCH)) {
                if (error.message.find("Enum base type") != std::string::npos) {
                    found_type_mismatch = true;
                    break;
                }
            }
        }
        CHECK(found_type_mismatch);
    }

    TEST_CASE("Enum item values must be integer") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1,
                PENDING = 2
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("ERROR: Enum item values must be integer, not string") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = "zero"
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_type_mismatch = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_TYPE_MISMATCH)) {
                if (error.message.find("Enum item value") != std::string::npos) {
                    found_type_mismatch = true;
                    break;
                }
            }
        }
        CHECK(found_type_mismatch);
    }

    TEST_CASE("Complex expression with multiple operators") {
        auto modules = make_module_set(R"(
            const uint32 COMPLEX = (10 + 20) * (30 - 5) / 2;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Negative integer literals") {
        auto modules = make_module_set(R"(
            const int32 NEG1 = -42;
            const int32 NEG2 = -100;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Comparison of integers") {
        auto modules = make_module_set(R"(
            const bool CMP1 = 10 < 20;
            const bool CMP2 = 100 >= 50;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("ERROR: Incompatible types in comparison") {
        auto modules = make_module_set(R"(
            const bool INVALID = 42 == "string";
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());

        auto errors = result.get_errors();
        bool found_incompatible_types = false;
        for (const auto& error : errors) {
            if (error.code == std::string(diag_codes::E_INCOMPATIBLE_TYPES)) {
                found_incompatible_types = true;
                break;
            }
        }
        CHECK(found_incompatible_types);
    }

    TEST_CASE("Boolean comparisons") {
        auto modules = make_module_set(R"(
            const bool CMP1 = true == false;
            const bool CMP2 = true != false;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("String comparisons") {
        auto modules = make_module_set(R"(
            const bool CMP1 = "abc" == "def";
            const bool CMP2 = "abc" != "def";
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Field type lookup in nested structs") {
        auto modules = make_module_set(R"(
            struct Inner {
                uint32 value;
                bool flag;
            };

            struct Outer {
                Inner inner;
                uint32 other: other == inner.value;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Field type lookup - integer field access in constraint") {
        auto modules = make_module_set(R"(
            struct Point {
                uint16 x;
                uint16 y;
            };

            struct Rectangle {
                Point top_left;
                Point bottom_right;
                uint32 width: width == bottom_right.x - top_left.x;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Function return type inference - simple function call") {
        auto modules = make_module_set(R"(
            struct Header {
                uint16 total_length;
                uint16 header_size;

                function uint16 get_header_length() {
                    return header_size;
                }

                function uint16 get_payload_length() {
                    return total_length - get_header_length();
                }
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }

    TEST_CASE("Function return type inference - recursive calls") {
        auto modules = make_module_set(R"(
            struct Calculator {
                uint32 a;
                uint32 b;
                uint32 c;

                function uint32 sum() {
                    return a + b + c;
                }

                function uint32 multiply(uint32 x, uint32 y) {
                    return x * y;
                }

                function uint32 chain_calls() {
                    return multiply(sum(), 2);
                }
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
    }
}

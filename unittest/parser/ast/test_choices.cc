#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Choice Definitions") {

    // ========================================
    // Simple Choice with Single Case
    // ========================================

    TEST_CASE("Choice with single case") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on type {
                case 1: uint8 byte_message;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "Message");
        REQUIRE(mod.choices[0].cases.size() == 1);

        // Check case
        auto& c = mod.choices[0].cases[0];
        CHECK(!c.is_default);
        REQUIRE(c.case_exprs.size() == 1);
        CHECK(c.field.name == "byte_message");
    }

    // ========================================
    // Choice with Multiple Cases
    // ========================================

    TEST_CASE("Choice with multiple cases") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Data on kind {
                case 1: uint8 byte_val;
                case 2: uint16 word_val;
                case 3: uint32 dword_val;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "Data");
        REQUIRE(mod.choices[0].cases.size() == 3);

        CHECK(mod.choices[0].cases[0].field.name == "byte_val");
        CHECK(mod.choices[0].cases[1].field.name == "word_val");
        CHECK(mod.choices[0].cases[2].field.name == "dword_val");

        // All should be case (not default)
        CHECK(!mod.choices[0].cases[0].is_default);
        CHECK(!mod.choices[0].cases[1].is_default);
        CHECK(!mod.choices[0].cases[2].is_default);
    }

    // ========================================
    // Choice with Default Case
    // ========================================

    TEST_CASE("Choice with default case") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on type {
                case 1: uint8 byte_message;
                default: uint8[] raw_data;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 2);

        // First case
        CHECK(!mod.choices[0].cases[0].is_default);
        CHECK(mod.choices[0].cases[0].field.name == "byte_message");

        // Default case
        CHECK(mod.choices[0].cases[1].is_default);
        CHECK(mod.choices[0].cases[1].case_exprs.empty());
        CHECK(mod.choices[0].cases[1].field.name == "raw_data");
    }

    // ========================================
    // Choice with Multiple Case Expressions
    // ========================================

    TEST_CASE("Choice with multiple case expressions") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Value on tag {
                case 1:
                case 2:
                case 3: uint8 small_val;
                case 10: uint32 large_val;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 2);

        // First case should have 3 case expressions
        auto& c1 = mod.choices[0].cases[0];
        CHECK(!c1.is_default);
        REQUIRE(c1.case_exprs.size() == 3);
        CHECK(c1.field.name == "small_val");

        // Second case should have 1 case expression
        auto& c2 = mod.choices[0].cases[1];
        CHECK(!c2.is_default);
        REQUIRE(c2.case_exprs.size() == 1);
        CHECK(c2.field.name == "large_val");
    }

    // ========================================
    // Choice with Complex Selector
    // ========================================

    TEST_CASE("Choice with complex selector expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Packet on flags & 0xFF {
                case 0: uint8 control;
                case 1: string data;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "Packet");

        // Check selector is a binary expression
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.choices[0].selector.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::bit_and);
    }

    // ========================================
    // Choice with Different Field Types
    // ========================================

    TEST_CASE("Choice with different field types") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Variant on type {
                case 1: uint32 int_value;
                case 2: string text_value;
                case 3: bool flag_value;
                case 4: uint8[4] array_value;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 4);

        // Check uint32
        auto* prim = std::get_if<datascript::ast::primitive_type>(&mod.choices[0].cases[0].field.field_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->bits == 32);

        // Check string
        auto* str = std::get_if<datascript::ast::string_type>(&mod.choices[0].cases[1].field.field_type.node);
        REQUIRE(str != nullptr);

        // Check bool
        auto* b = std::get_if<datascript::ast::bool_type>(&mod.choices[0].cases[2].field.field_type.node);
        REQUIRE(b != nullptr);

        // Check array
        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.choices[0].cases[3].field.field_type.node);
        REQUIRE(arr != nullptr);
    }

    // ========================================
    // Choice with Conditional Fields
    // ========================================

    TEST_CASE("Choice with conditional fields") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Optional on mode {
                case 0: uint8 simple;
                case 1: uint32 extended if flags > 0;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 2);

        // First field has no condition
        CHECK(!mod.choices[0].cases[0].field.condition.has_value());

        // Second field has condition
        CHECK(mod.choices[0].cases[1].field.condition.has_value());
    }

    // ========================================
    // Choice with Bit Fields
    // ========================================

    TEST_CASE("Choice with bit field types") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Compact on selector {
                case 1: bit:8 bits8;
                case 2: bit:16 bits16;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 2);

        auto* bf1 = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.choices[0].cases[0].field.field_type.node);
        REQUIRE(bf1 != nullptr);
        CHECK(bf1->width == 8);

        auto* bf2 = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.choices[0].cases[1].field.field_type.node);
        REQUIRE(bf2 != nullptr);
        CHECK(bf2->width == 16);
    }

    // ========================================
    // Choice with Endianness
    // ========================================

    TEST_CASE("Choice with endianness modifiers") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Network on protocol {
                case 1: big uint32 big_endian;
                case 2: little uint32 little_endian;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].cases.size() == 2);

        auto* prim1 = std::get_if<datascript::ast::primitive_type>(&mod.choices[0].cases[0].field.field_type.node);
        REQUIRE(prim1 != nullptr);
        CHECK(prim1->byte_order == datascript::ast::endian::big);

        auto* prim2 = std::get_if<datascript::ast::primitive_type>(&mod.choices[0].cases[1].field.field_type.node);
        REQUIRE(prim2 != nullptr);
        CHECK(prim2->byte_order == datascript::ast::endian::little);
    }

    // ========================================
    // Multiple Choices
    // ========================================

    TEST_CASE("Multiple choice definitions") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on type {
                case 1: uint8 byte;
                case 2: string text;
            };

            choice Response on code {
                case 200: string success;
                default: uint8 error;
            };
        )"));

        REQUIRE(mod.choices.size() == 2);

        CHECK(mod.choices[0].name == "Message");
        CHECK(mod.choices[0].cases.size() == 2);

        CHECK(mod.choices[1].name == "Response");
        CHECK(mod.choices[1].cases.size() == 2);
        CHECK(mod.choices[1].cases[1].is_default);
    }

    // ========================================
    // Choices Mixed with Other Definitions
    // ========================================

    TEST_CASE("Choice with structs and enums") {
        auto mod = datascript::parse_datascript(std::string(R"(
            enum uint8 Type {
                TYPE_A,
                TYPE_B
            };

            struct Header {
                uint8 type;
            };

            choice Payload on type {
                case 1: uint8 data_a;
                case 2: string data_b;
            };
        )"));

        CHECK(mod.enums.size() == 1);
        CHECK(mod.structs.size() == 1);
        REQUIRE(mod.choices.size() == 1);

        CHECK(mod.choices[0].name == "Payload");
        CHECK(mod.choices[0].cases.size() == 2);
    }

    // ========================================
    // Complex Choice Example
    // ========================================

    TEST_CASE("Complex choice with all features") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice ComplexMessage on flags & 0x0F {
                case 0:
                case 1: uint8 simple;
                case 2: uint8[4] array;
                case 3: big uint32 network_order if flags > 2;
                case 10:
                case 11:
                case 12: string text;
                default: uint8[] raw;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "ComplexMessage");
        REQUIRE(mod.choices[0].cases.size() == 5);

        // Check multiple case expressions
        CHECK(mod.choices[0].cases[0].case_exprs.size() == 2);
        CHECK(mod.choices[0].cases[3].case_exprs.size() == 3);

        // Check default
        CHECK(mod.choices[0].cases[4].is_default);
        CHECK(mod.choices[0].cases[4].case_exprs.empty());
    }
}

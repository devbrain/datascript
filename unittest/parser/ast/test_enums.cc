#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Enums and Bitmasks") {

    // ========================================
    // Simple Enum Definitions
    // ========================================

    TEST_CASE("Simple enum with uint8") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint8 Color { RED, GREEN, BLUE };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Color");
        CHECK(mod.enums[0].is_bitmask == false);

        // Check base type is uint8
        auto* base = std::get_if<datascript::ast::primitive_type>(&mod.enums[0].base_type.node);
        REQUIRE(base != nullptr);
        CHECK(base->is_signed == false);
        CHECK(base->bits == 8);

        // Check enum items
        REQUIRE(mod.enums[0].items.size() == 3);
        CHECK(mod.enums[0].items[0].name == "RED");
        CHECK_FALSE(mod.enums[0].items[0].value.has_value());

        CHECK(mod.enums[0].items[1].name == "GREEN");
        CHECK_FALSE(mod.enums[0].items[1].value.has_value());

        CHECK(mod.enums[0].items[2].name == "BLUE");
        CHECK_FALSE(mod.enums[0].items[2].value.has_value());
    }

    TEST_CASE("Enum with explicit values") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint32 Status { OK = 0, ERROR = 1, PENDING = 2 };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Status");

        // Check items with values
        REQUIRE(mod.enums[0].items.size() == 3);

        CHECK(mod.enums[0].items[0].name == "OK");
        REQUIRE(mod.enums[0].items[0].value.has_value());
        auto* val0 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[0].value->node);
        REQUIRE(val0 != nullptr);
        CHECK(val0->value == 0);

        CHECK(mod.enums[0].items[1].name == "ERROR");
        REQUIRE(mod.enums[0].items[1].value.has_value());
        auto* val1 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[1].value->node);
        REQUIRE(val1 != nullptr);
        CHECK(val1->value == 1);

        CHECK(mod.enums[0].items[2].name == "PENDING");
        REQUIRE(mod.enums[0].items[2].value.has_value());
        auto* val2 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[2].value->node);
        REQUIRE(val2 != nullptr);
        CHECK(val2->value == 2);
    }

    TEST_CASE("Enum with mixed explicit and implicit values") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint16 Priority { LOW, MEDIUM = 5, HIGH };"
        ));

        REQUIRE(mod.enums.size() == 1);
        REQUIRE(mod.enums[0].items.size() == 3);

        // First item - implicit
        CHECK(mod.enums[0].items[0].name == "LOW");
        CHECK_FALSE(mod.enums[0].items[0].value.has_value());

        // Second item - explicit value
        CHECK(mod.enums[0].items[1].name == "MEDIUM");
        REQUIRE(mod.enums[0].items[1].value.has_value());
        auto* val = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[1].value->node);
        REQUIRE(val != nullptr);
        CHECK(val->value == 5);

        // Third item - implicit (would be 6 in semantic analysis)
        CHECK(mod.enums[0].items[2].name == "HIGH");
        CHECK_FALSE(mod.enums[0].items[2].value.has_value());
    }

    TEST_CASE("Enum with different base types") {
        auto mod = datascript::parse_datascript(std::string(
            "enum int8 SignedEnum { NEG = -1, ZERO = 0, POS = 1 };\n"
            "enum uint64 LargeEnum { SMALL = 0, LARGE = 1000000 };\n"
        ));

        REQUIRE(mod.enums.size() == 2);

        // First enum - int8
        CHECK(mod.enums[0].name == "SignedEnum");
        auto* base1 = std::get_if<datascript::ast::primitive_type>(&mod.enums[0].base_type.node);
        REQUIRE(base1 != nullptr);
        CHECK(base1->is_signed == true);
        CHECK(base1->bits == 8);

        // Second enum - uint64
        CHECK(mod.enums[1].name == "LargeEnum");
        auto* base2 = std::get_if<datascript::ast::primitive_type>(&mod.enums[1].base_type.node);
        REQUIRE(base2 != nullptr);
        CHECK(base2->is_signed == false);
        CHECK(base2->bits == 64);
    }

    TEST_CASE("Single item enum") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint8 SingleValue { ONLY_VALUE };"
        ));

        REQUIRE(mod.enums.size() == 1);
        REQUIRE(mod.enums[0].items.size() == 1);
        CHECK(mod.enums[0].items[0].name == "ONLY_VALUE");
    }

    // ========================================
    // Bitmask Definitions
    // ========================================

    TEST_CASE("Simple bitmask") {
        auto mod = datascript::parse_datascript(std::string(
            "bitmask uint32 Flags { READ, WRITE, EXECUTE };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Flags");
        CHECK(mod.enums[0].is_bitmask == true);

        // Check base type
        auto* base = std::get_if<datascript::ast::primitive_type>(&mod.enums[0].base_type.node);
        REQUIRE(base != nullptr);
        CHECK(base->bits == 32);

        // Check items
        REQUIRE(mod.enums[0].items.size() == 3);
        CHECK(mod.enums[0].items[0].name == "READ");
        CHECK(mod.enums[0].items[1].name == "WRITE");
        CHECK(mod.enums[0].items[2].name == "EXECUTE");
    }

    TEST_CASE("Bitmask with explicit bit positions") {
        auto mod = datascript::parse_datascript(std::string(
            "bitmask uint8 Permissions { READ = 1, WRITE = 2, EXECUTE = 4 };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].is_bitmask == true);
        REQUIRE(mod.enums[0].items.size() == 3);

        // Check values
        auto* val0 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[0].value->node);
        REQUIRE(val0 != nullptr);
        CHECK(val0->value == 1);

        auto* val1 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[1].value->node);
        REQUIRE(val1 != nullptr);
        CHECK(val1->value == 2);

        auto* val2 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[2].value->node);
        REQUIRE(val2 != nullptr);
        CHECK(val2->value == 4);
    }

    TEST_CASE("Bitmask with shift expressions") {
        auto mod = datascript::parse_datascript(std::string(
            "bitmask uint32 Bits { BIT0 = 1 << 0, BIT1 = 1 << 1, BIT2 = 1 << 2 };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].is_bitmask == true);
        REQUIRE(mod.enums[0].items.size() == 3);

        // Check first item has a binary expression (shift)
        REQUIRE(mod.enums[0].items[0].value.has_value());
        auto* shift0 = std::get_if<datascript::ast::binary_expr>(&mod.enums[0].items[0].value->node);
        REQUIRE(shift0 != nullptr);
        CHECK(shift0->op == datascript::ast::binary_op::lshift);
    }

    // ========================================
    // Complex Enum Values
    // ========================================

    TEST_CASE("Enum with constant expressions") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint16 Values { A = 10, B = A + 5, C = B * 2 };"
        ));

        REQUIRE(mod.enums.size() == 1);
        REQUIRE(mod.enums[0].items.size() == 3);

        // B should have an addition expression
        REQUIRE(mod.enums[0].items[1].value.has_value());
        auto* add = std::get_if<datascript::ast::binary_expr>(&mod.enums[0].items[1].value->node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);

        // C should have a multiplication expression
        REQUIRE(mod.enums[0].items[2].value.has_value());
        auto* mul = std::get_if<datascript::ast::binary_expr>(&mod.enums[0].items[2].value->node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);
    }

    TEST_CASE("Enum with hexadecimal values") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint32 Hex { A = 0x10, B = 0xFF, C = 0xDEADBEEF };"
        ));

        REQUIRE(mod.enums.size() == 1);
        REQUIRE(mod.enums[0].items.size() == 3);

        auto* val0 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[0].value->node);
        REQUIRE(val0 != nullptr);
        CHECK(val0->value == 0x10);

        auto* val1 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[1].value->node);
        REQUIRE(val1 != nullptr);
        CHECK(val1->value == 0xFF);

        auto* val2 = std::get_if<datascript::ast::literal_int>(&mod.enums[0].items[2].value->node);
        REQUIRE(val2 != nullptr);
        CHECK(val2->value == 0xDEADBEEF);
    }

    // ========================================
    // Multiple Enums
    // ========================================

    TEST_CASE("Multiple enums and bitmasks in one module") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint8 Color { RED, GREEN, BLUE };\n"
            "bitmask uint32 Flags { READ = 1, WRITE = 2 };\n"
            "enum uint16 Status { OK, ERROR };\n"
        ));

        REQUIRE(mod.enums.size() == 3);

        // First - enum
        CHECK(mod.enums[0].name == "Color");
        CHECK(mod.enums[0].is_bitmask == false);
        CHECK(mod.enums[0].items.size() == 3);

        // Second - bitmask
        CHECK(mod.enums[1].name == "Flags");
        CHECK(mod.enums[1].is_bitmask == true);
        CHECK(mod.enums[1].items.size() == 2);

        // Third - enum
        CHECK(mod.enums[2].name == "Status");
        CHECK(mod.enums[2].is_bitmask == false);
        CHECK(mod.enums[2].items.size() == 2);
    }

    TEST_CASE("Enum mixed with other module elements") {
        auto mod = datascript::parse_datascript(std::string(
            "package test;\n"
            "import util.*;\n"
            "const uint32 MAX = 100;\n"
            "enum uint8 Status { OK, ERROR };\n"
            "constraint IsPositive { value > 0 };\n"
        ));

        // Check all elements present
        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts[0] == "test");

        REQUIRE(mod.imports.size() == 1);
        CHECK(mod.imports[0].name_parts[0] == "util");

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "MAX");

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Status");
        CHECK(mod.enums[0].items.size() == 2);

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "IsPositive");
    }

    // ========================================
    // Edge Cases
    // ========================================

    TEST_CASE("Enum with underscore in names") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint32 HttpStatus { STATUS_OK = 200, STATUS_NOT_FOUND = 404 };"
        ));

        REQUIRE(mod.enums.size() == 1);
        REQUIRE(mod.enums[0].items.size() == 2);
        CHECK(mod.enums[0].items[0].name == "STATUS_OK");
        CHECK(mod.enums[0].items[1].name == "STATUS_NOT_FOUND");
    }

    TEST_CASE("Enum with many items") {
        auto mod = datascript::parse_datascript(std::string(
            "enum uint8 Days { MON, TUE, WED, THU, FRI, SAT, SUN };"
        ));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].items.size() == 7);
        CHECK(mod.enums[0].items[0].name == "MON");
        CHECK(mod.enums[0].items[6].name == "SUN");
    }
}

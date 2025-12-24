#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Subtypes") {

    // ========================================
    // Basic Subtype Definition
    // ========================================

    TEST_CASE("Simple subtype with primitive base type") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint16 UserID : this > 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        CHECK(mod.subtypes[0].name == "UserID");

        // Check base type is uint16
        auto* prim = std::get_if<datascript::ast::primitive_type>(&mod.subtypes[0].base_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 16);

        // Check constraint expression is a comparison
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::gt);

        // Check left operand is 'this'
        auto* left = std::get_if<datascript::ast::identifier>(&binary->left->node);
        REQUIRE(left != nullptr);
        CHECK(left->name == "this");

        // Check right operand is 0
        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 0);
    }

    TEST_CASE("Subtype with complex constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint16 Port : this > 1024 && this < 65535;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        CHECK(mod.subtypes[0].name == "Port");

        // Check constraint is logical AND
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::log_and);
    }

    TEST_CASE("Subtype with different base types") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint8 Percentage : this <= 100;\n"
            "subtype uint32 IPv4Address : this != 0;\n"
            "subtype uint64 Timestamp : this > 0;\n"
        ));

        REQUIRE(mod.subtypes.size() == 3);
        CHECK(mod.subtypes[0].name == "Percentage");
        CHECK(mod.subtypes[1].name == "IPv4Address");
        CHECK(mod.subtypes[2].name == "Timestamp");
    }

    // ========================================
    // Docstring Support
    // ========================================

    TEST_CASE("Subtype with docstring") {
        auto mod = datascript::parse_datascript(std::string(
            "/** User identifier, must be positive */\n"
            "subtype uint16 UserID : this > 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        CHECK(mod.subtypes[0].name == "UserID");
        REQUIRE(mod.subtypes[0].docstring.has_value());
        CHECK(mod.subtypes[0].docstring.value() == "User identifier, must be positive");
    }

    TEST_CASE("Subtype with multi-line docstring") {
        auto mod = datascript::parse_datascript(std::string(
            "/**\n"
            " * Network port number.\n"
            " * Must be in the valid range (1024-65535).\n"
            " */\n"
            "subtype uint16 Port : this > 1024 && this < 65535;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        CHECK(mod.subtypes[0].name == "Port");
        REQUIRE(mod.subtypes[0].docstring.has_value());
        CHECK(mod.subtypes[0].docstring.value().find("Network port number") != std::string::npos);
    }

    // ========================================
    // Expression Types in Constraints
    // ========================================

    TEST_CASE("Subtype with equality constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint8 MagicByte : this == 0x42;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::eq);
    }

    TEST_CASE("Subtype with inequality constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint32 NonZero : this != 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::ne);
    }

    TEST_CASE("Subtype with bitwise constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint32 AlignedAddress : (this & 0x3) == 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        // Outer expression is equality
        auto* eq_expr = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(eq_expr != nullptr);
        CHECK(eq_expr->op == datascript::ast::binary_op::eq);

        // Left side is bitwise AND
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&eq_expr->left->node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::bit_and);
    }

    TEST_CASE("Subtype with logical OR constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint8 ValidValue : this == 0 || this == 1;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* or_expr = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(or_expr != nullptr);
        CHECK(or_expr->op == datascript::ast::binary_op::log_or);
    }

    // ========================================
    // Different Base Types
    // ========================================

    TEST_CASE("Subtype with uint8 base") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint8 ByteValue : this < 128;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* prim = std::get_if<datascript::ast::primitive_type>(&mod.subtypes[0].base_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 8);
    }

    TEST_CASE("Subtype with uint32 base") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint32 Count : this > 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* prim = std::get_if<datascript::ast::primitive_type>(&mod.subtypes[0].base_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 32);
    }

    TEST_CASE("Subtype with uint64 base") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint64 LargeNumber : this != 0;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* prim = std::get_if<datascript::ast::primitive_type>(&mod.subtypes[0].base_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 64);
    }

    // ========================================
    // Multiple Subtypes
    // ========================================

    TEST_CASE("Multiple subtypes in one module") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint16 UserID : this > 0;\n"
            "subtype uint16 Port : this > 1024;\n"
            "subtype uint32 SessionID : this != 0;\n"
        ));

        REQUIRE(mod.subtypes.size() == 3);
        CHECK(mod.subtypes[0].name == "UserID");
        CHECK(mod.subtypes[1].name == "Port");
        CHECK(mod.subtypes[2].name == "SessionID");
    }

    TEST_CASE("Subtypes mixed with other definitions") {
        auto mod = datascript::parse_datascript(std::string(
            "const uint8 MAX_VALUE = 100;\n"
            "subtype uint8 Percentage : this <= MAX_VALUE;\n"
            "struct User { uint16 id; };\n"
            "subtype uint16 UserID : this > 0;\n"
        ));

        REQUIRE(mod.constants.size() == 1);
        REQUIRE(mod.subtypes.size() == 2);
        REQUIRE(mod.structs.size() == 1);

        CHECK(mod.subtypes[0].name == "Percentage");
        CHECK(mod.subtypes[1].name == "UserID");
    }

    // ========================================
    // Edge Cases
    // ========================================

    TEST_CASE("Subtype with literal true constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint8 AnyByte : true;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* lit = std::get_if<datascript::ast::literal_bool>(&mod.subtypes[0].constraint.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == true);
    }

    TEST_CASE("Subtype with hexadecimal literal in constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint32 MagicNumber : this == 0xDEADBEEF;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(binary != nullptr);

        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 0xDEADBEEF);
    }

    TEST_CASE("Subtype with parenthesized constraint") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint16 ValidRange : (this > 0 && this < 100);"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        CHECK(mod.subtypes[0].name == "ValidRange");
    }

    TEST_CASE("Subtype with complex nested expression") {
        auto mod = datascript::parse_datascript(std::string(
            "subtype uint32 ComplexConstraint : (this > 0 && this < 1000) || this == 0xFFFFFFFF;"
        ));

        REQUIRE(mod.subtypes.size() == 1);
        // Top level should be logical OR
        auto* or_expr = std::get_if<datascript::ast::binary_expr>(&mod.subtypes[0].constraint.node);
        REQUIRE(or_expr != nullptr);
        CHECK(or_expr->op == datascript::ast::binary_op::log_or);
    }

} // TEST_SUITE

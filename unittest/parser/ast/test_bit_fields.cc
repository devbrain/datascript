#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Bit Fields") {

    // ========================================
    // Fixed Bit Fields (bit:N)
    // ========================================

    TEST_CASE("Fixed bit field with literal") {
        auto mod = datascript::parse_datascript(std::string("const bit:8 X = 0xFF;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is bit_field_type_fixed
        auto* bf = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);
        CHECK(bf->width == 8);
    }

    TEST_CASE("Fixed bit field 16-bit") {
        auto mod = datascript::parse_datascript(std::string("const bit:16 Y = 0x1234;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);
        CHECK(bf->width == 16);
    }

    TEST_CASE("Fixed bit field 32-bit") {
        auto mod = datascript::parse_datascript(std::string("const bit:32 Z = 123;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);
        CHECK(bf->width == 32);
    }

    // ========================================
    // Dynamic Bit Fields (bit<expr>)
    // ========================================

    TEST_CASE("Dynamic bit field with integer literal") {
        auto mod = datascript::parse_datascript(std::string("const bit<8> X = 0xFF;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is bit_field_type_expr
        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        // Check the width expression is a literal
        auto* lit = std::get_if<datascript::ast::literal_int>(&bf->width_expr.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == 8);
    }

    TEST_CASE("Dynamic bit field with identifier") {
        auto mod = datascript::parse_datascript(std::string("const bit<WIDTH> X = 0xFF;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        // Check the width expression is an identifier
        auto* ident = std::get_if<datascript::ast::identifier>(&bf->width_expr.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "WIDTH");
    }

    TEST_CASE("Dynamic bit field with arithmetic expression") {
        auto mod = datascript::parse_datascript(std::string("const bit<WIDTH + 8> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        // Check the width expression is a binary expression
        auto* binary = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::add);

        // Left operand should be identifier
        auto* left = std::get_if<datascript::ast::identifier>(&binary->left->node);
        REQUIRE(left != nullptr);
        CHECK(left->name == "WIDTH");

        // Right operand should be literal
        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 8);
    }

    TEST_CASE("Dynamic bit field with multiplication") {
        auto mod = datascript::parse_datascript(std::string("const bit<SIZE * 2> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        auto* binary = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::mul);
    }

    TEST_CASE("Dynamic bit field with shift expression") {
        auto mod = datascript::parse_datascript(std::string("const bit<1 << 3> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        auto* binary = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::lshift);

        auto* left = std::get_if<datascript::ast::literal_int>(&binary->left->node);
        REQUIRE(left != nullptr);
        CHECK(left->value == 1);

        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 3);
    }

    TEST_CASE("Dynamic bit field with bitwise AND") {
        auto mod = datascript::parse_datascript(std::string("const bit<FLAGS & MASK> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        auto* binary = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::bit_and);
    }

    TEST_CASE("Dynamic bit field with unary minus") {
        auto mod = datascript::parse_datascript(std::string("const bit<-WIDTH> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        auto* unary = std::get_if<datascript::ast::unary_expr>(&bf->width_expr.node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::neg);
    }

    TEST_CASE("Dynamic bit field with complex expression") {
        auto mod = datascript::parse_datascript(std::string("const bit<(BASE + OFFSET) * 2> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        // Top level should be multiplication
        auto* mul = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);

        // Left side should be addition (from parentheses)
        auto* add = std::get_if<datascript::ast::binary_expr>(&mul->left->node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);
    }

    // ========================================
    // Comparison Inside Bit Fields (requires parentheses)
    // ========================================

    TEST_CASE("Dynamic bit field with comparison requires parentheses") {
        // bit<(A < B)> should work - comparison wrapped in expression via parentheses
        auto mod = datascript::parse_datascript(std::string("const bit<(A < B)> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        // The parentheses allow the full expression to be used
        auto* binary = std::get_if<datascript::ast::binary_expr>(&bf->width_expr.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::lt);
    }

    TEST_CASE("Dynamic bit field with ternary requires parentheses") {
        // bit<(COND ? A : B)> should work
        auto mod = datascript::parse_datascript(std::string("const bit<(FLAG ? 8 : 16)> X = 0;"));

        auto* bf = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&bf->width_expr.node);
        REQUIRE(ternary != nullptr);
    }

    // ========================================
    // Edge Cases
    // ========================================

    TEST_CASE("Multiple bit field constants") {
        auto mod = datascript::parse_datascript(std::string(
            "const bit:8 A = 1;\n"
            "const bit<16> B = 2;\n"
            "const bit<SIZE> C = 3;\n"
        ));

        REQUIRE(mod.constants.size() == 3);

        // First is fixed
        auto* bf1 = std::get_if<datascript::ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf1 != nullptr);
        CHECK(bf1->width == 8);

        // Second is dynamic with literal
        auto* bf2 = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[1].ctype.node);
        REQUIRE(bf2 != nullptr);

        // Third is dynamic with identifier
        auto* bf3 = std::get_if<datascript::ast::bit_field_type_expr>(&mod.constants[2].ctype.node);
        REQUIRE(bf3 != nullptr);
    }
}

#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Expressions") {

    // ========================================
    // Unary Expressions
    // ========================================

    TEST_CASE("Unary minus with literal") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = -42;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that value is a unary_expr
        auto* unary = std::get_if<datascript::ast::unary_expr>(&mod.constants[0].value.node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::neg);

        // Check operand is integer literal
        auto* literal = std::get_if<datascript::ast::literal_int>(&unary->operand->node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 42);
    }

    TEST_CASE("Unary plus with literal") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = +123;"));

        auto* unary = std::get_if<datascript::ast::unary_expr>(&mod.constants[0].value.node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::pos);
    }

    TEST_CASE("Unary bit_not with literal") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = ~0xFF;"));

        auto* unary = std::get_if<datascript::ast::unary_expr>(&mod.constants[0].value.node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::bit_not);
    }

    TEST_CASE("Unary log_not with boolean") {
        auto mod = datascript::parse_datascript(std::string("const bool X = !true;"));

        auto* unary = std::get_if<datascript::ast::unary_expr>(&mod.constants[0].value.node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::log_not);
    }

    TEST_CASE("Double negation") {
        auto mod = datascript::parse_datascript(std::string("const int32 X = --5;"));

        auto* unary1 = std::get_if<datascript::ast::unary_expr>(&mod.constants[0].value.node);
        REQUIRE(unary1 != nullptr);
        CHECK(unary1->op == datascript::ast::unary_op::neg);

        auto* unary2 = std::get_if<datascript::ast::unary_expr>(&unary1->operand->node);
        REQUIRE(unary2 != nullptr);
        CHECK(unary2->op == datascript::ast::unary_op::neg);
    }

    // ========================================
    // Binary Expressions - Arithmetic
    // ========================================

    TEST_CASE("Addition") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 10 + 20;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::add);

        auto* left = std::get_if<datascript::ast::literal_int>(&binary->left->node);
        REQUIRE(left != nullptr);
        CHECK(left->value == 10);

        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 20);
    }

    TEST_CASE("Subtraction") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 100 - 30;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::sub);
    }

    TEST_CASE("Multiplication") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 5 * 6;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::mul);
    }

    TEST_CASE("Division") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 50 / 2;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::div);
    }

    TEST_CASE("Modulo") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 17 % 5;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::mod);
    }

    TEST_CASE("Operator precedence: multiplication before addition") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 2 + 3 * 4;"));

        // Should parse as: 2 + (3 * 4)
        auto* add = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);

        // Right side should be multiplication
        auto* mul = std::get_if<datascript::ast::binary_expr>(&add->right->node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);
    }

    TEST_CASE("Parentheses override precedence") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = (2 + 3) * 4;"));

        // Should parse as: (2 + 3) * 4
        auto* mul = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);

        // Left side should be addition
        auto* add = std::get_if<datascript::ast::binary_expr>(&mul->left->node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);
    }

    // ========================================
    // Binary Expressions - Comparison
    // ========================================

    TEST_CASE("Equality") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 5 == 5;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::eq);
    }

    TEST_CASE("Inequality") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 5 != 3;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::ne);
    }

    TEST_CASE("Less than") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 3 < 5;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::lt);
    }

    TEST_CASE("Greater than") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 7 > 3;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::gt);
    }

    TEST_CASE("Less than or equal") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 5 <= 5;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::le);
    }

    TEST_CASE("Greater than or equal") {
        auto mod = datascript::parse_datascript(std::string("const bool X = 10 >= 5;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::ge);
    }

    // ========================================
    // Binary Expressions - Bitwise
    // ========================================

    TEST_CASE("Bitwise AND") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 0xFF & 0x0F;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::bit_and);
    }

    TEST_CASE("Bitwise OR") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 0xF0 | 0x0F;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::bit_or);
    }

    TEST_CASE("Bitwise XOR") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 0xFF ^ 0xAA;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::bit_xor);
    }

    TEST_CASE("Left shift") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 1 << 8;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::lshift);
    }

    TEST_CASE("Right shift") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 256 >> 4;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::rshift);
    }

    // ========================================
    // Binary Expressions - Logical
    // ========================================

    TEST_CASE("Logical AND") {
        auto mod = datascript::parse_datascript(std::string("const bool X = true && false;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::log_and);
    }

    TEST_CASE("Logical OR") {
        auto mod = datascript::parse_datascript(std::string("const bool X = true || false;"));

        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::log_or);
    }

    // ========================================
    // Ternary Expression
    // ========================================

    TEST_CASE("Ternary conditional") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = true ? 10 : 20;"));

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constants[0].value.node);
        REQUIRE(ternary != nullptr);

        // Check condition
        auto* cond = std::get_if<datascript::ast::literal_bool>(&ternary->condition->node);
        REQUIRE(cond != nullptr);
        CHECK(cond->value == true);

        // Check true branch
        auto* true_val = std::get_if<datascript::ast::literal_int>(&ternary->true_expr->node);
        REQUIRE(true_val != nullptr);
        CHECK(true_val->value == 10);

        // Check false branch
        auto* false_val = std::get_if<datascript::ast::literal_int>(&ternary->false_expr->node);
        REQUIRE(false_val != nullptr);
        CHECK(false_val->value == 20);
    }

    TEST_CASE("Nested ternary") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = true ? false ? 1 : 2 : 3;"));

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constants[0].value.node);
        REQUIRE(ternary != nullptr);

        // True branch should be another ternary
        auto* nested = std::get_if<datascript::ast::ternary_expr>(&ternary->true_expr->node);
        REQUIRE(nested != nullptr);
    }

    // ========================================
    // Complex Expressions
    // ========================================

    TEST_CASE("Mixed operators with correct precedence") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 1 + 2 * 3 - 4 / 2;"));

        // Should parse as: (1 + (2 * 3)) - (4 / 2)
        auto* sub = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(sub != nullptr);
        CHECK(sub->op == datascript::ast::binary_op::sub);

        // Left side: 1 + (2 * 3)
        auto* add = std::get_if<datascript::ast::binary_expr>(&sub->left->node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);

        // Right side of left: 2 * 3
        auto* mul = std::get_if<datascript::ast::binary_expr>(&add->right->node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);

        // Right side: 4 / 2
        auto* div = std::get_if<datascript::ast::binary_expr>(&sub->right->node);
        REQUIRE(div != nullptr);
        CHECK(div->op == datascript::ast::binary_op::div);
    }

    TEST_CASE("Expression with identifiers") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = FOO + BAR * 2;"));

        auto* add = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);

        // Left operand is identifier
        auto* left_id = std::get_if<datascript::ast::identifier>(&add->left->node);
        REQUIRE(left_id != nullptr);
        CHECK(left_id->name == "FOO");

        // Right operand is multiplication
        auto* mul = std::get_if<datascript::ast::binary_expr>(&add->right->node);
        REQUIRE(mul != nullptr);

        auto* right_id = std::get_if<datascript::ast::identifier>(&mul->left->node);
        REQUIRE(right_id != nullptr);
        CHECK(right_id->name == "BAR");
    }

    TEST_CASE("Unary in binary expression") {
        auto mod = datascript::parse_datascript(std::string("const int32 X = -5 + 10;"));

        auto* add = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(add != nullptr);
        CHECK(add->op == datascript::ast::binary_op::add);

        // Left side is unary minus
        auto* unary = std::get_if<datascript::ast::unary_expr>(&add->left->node);
        REQUIRE(unary != nullptr);
        CHECK(unary->op == datascript::ast::unary_op::neg);
    }
}

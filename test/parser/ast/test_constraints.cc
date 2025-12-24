#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Constraints") {

    // ========================================
    // Simple Constraints
    // ========================================

    TEST_CASE("Simple constraint with literal") {
        auto mod = datascript::parse_datascript(std::string("constraint IsValid { true };"));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "IsValid");
        CHECK(mod.constraints[0].params.empty());

        // Check condition is a boolean literal
        auto* lit = std::get_if<datascript::ast::literal_bool>(&mod.constraints[0].condition.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == true);
    }

    TEST_CASE("Constraint with identifier expression") {
        auto mod = datascript::parse_datascript(std::string("constraint CheckValue { value };"));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "CheckValue");

        // Check condition is an identifier
        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constraints[0].condition.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "value");
    }

    TEST_CASE("Constraint with comparison") {
        auto mod = datascript::parse_datascript(std::string("constraint IsPositive { value > 0 };"));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "IsPositive");

        // Check condition is a binary expression
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::gt);

        // Check left operand is identifier
        auto* left = std::get_if<datascript::ast::identifier>(&binary->left->node);
        REQUIRE(left != nullptr);
        CHECK(left->name == "value");

        // Check right operand is zero
        auto* right = std::get_if<datascript::ast::literal_int>(&binary->right->node);
        REQUIRE(right != nullptr);
        CHECK(right->value == 0);
    }

    TEST_CASE("Constraint with range check") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint InRange { value >= 0 && value <= 100 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "InRange");

        // Check condition is logical AND
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::log_and);

        // Check left side is value >= 0
        auto* left_cmp = std::get_if<datascript::ast::binary_expr>(&and_expr->left->node);
        REQUIRE(left_cmp != nullptr);
        CHECK(left_cmp->op == datascript::ast::binary_op::ge);

        // Check right side is value <= 100
        auto* right_cmp = std::get_if<datascript::ast::binary_expr>(&and_expr->right->node);
        REQUIRE(right_cmp != nullptr);
        CHECK(right_cmp->op == datascript::ast::binary_op::le);
    }

    TEST_CASE("Constraint with arithmetic expression") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint SizeCheck { width * height <= MAX_SIZE };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "SizeCheck");

        // Top level should be comparison
        auto* cmp = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(cmp != nullptr);
        CHECK(cmp->op == datascript::ast::binary_op::le);

        // Left side should be multiplication
        auto* mul = std::get_if<datascript::ast::binary_expr>(&cmp->left->node);
        REQUIRE(mul != nullptr);
        CHECK(mul->op == datascript::ast::binary_op::mul);
    }

    TEST_CASE("Constraint with bitwise operations") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint HasFlags { (flags & REQUIRED_MASK) == REQUIRED_MASK };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "HasFlags");

        // Top level should be equality
        auto* eq = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(eq != nullptr);
        CHECK(eq->op == datascript::ast::binary_op::eq);

        // Left side should be bitwise AND
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&eq->left->node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::bit_and);
    }

    TEST_CASE("Constraint with ternary expression") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint CheckConditional { enabled ? value > 0 : true };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "CheckConditional");

        // Should be ternary expression
        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(ternary != nullptr);

        // Condition should be identifier
        auto* cond = std::get_if<datascript::ast::identifier>(&ternary->condition->node);
        REQUIRE(cond != nullptr);
        CHECK(cond->name == "enabled");
    }

    TEST_CASE("Constraint with negation") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint NotZero { value != 0 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "NotZero");

        auto* ne = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(ne != nullptr);
        CHECK(ne->op == datascript::ast::binary_op::ne);
    }

    TEST_CASE("Constraint with logical NOT") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint NotDisabled { !disabled };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "NotDisabled");

        auto* log_not = std::get_if<datascript::ast::unary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(log_not != nullptr);
        CHECK(log_not->op == datascript::ast::unary_op::log_not);
    }

    // ========================================
    // Multiple Constraints
    // ========================================

    TEST_CASE("Multiple constraints in module") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint IsPositive { value > 0 };\n"
            "constraint IsValid { enabled };\n"
            "constraint InRange { value <= 100 };\n"
        ));

        REQUIRE(mod.constraints.size() == 3);
        CHECK(mod.constraints[0].name == "IsPositive");
        CHECK(mod.constraints[1].name == "IsValid");
        CHECK(mod.constraints[2].name == "InRange");
    }

    TEST_CASE("Constraints mixed with constants") {
        auto mod = datascript::parse_datascript(std::string(
            "const uint32 MAX = 100;\n"
            "constraint InRange { value <= MAX };\n"
            "const bool ENABLED = true;\n"
            "constraint IsEnabled { ENABLED };\n"
        ));

        REQUIRE(mod.constants.size() == 2);
        REQUIRE(mod.constraints.size() == 2);

        CHECK(mod.constants[0].name == "MAX");
        CHECK(mod.constraints[0].name == "InRange");
        CHECK(mod.constants[1].name == "ENABLED");
        CHECK(mod.constraints[1].name == "IsEnabled");
    }

    // ========================================
    // Edge Cases
    // ========================================

    TEST_CASE("Constraint with complex nested expression") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint ComplexCheck { ((a + b) * c > threshold) && (flags & mask) != 0 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "ComplexCheck");

        // Should be logical AND at top level
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::log_and);
    }

    TEST_CASE("Constraint name follows identifier rules") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint Check_Value_123 { value > 0 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "Check_Value_123");
    }

    TEST_CASE("Constraint with parenthesized expression") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint ParenCheck { (value) };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "ParenCheck");

        // Parentheses don't create nodes, just group
        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constraints[0].condition.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "value");
    }

    // ========================================
    // Parameterized Constraints
    // ========================================

    TEST_CASE("Constraint with single parameter") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint IsGreaterThan(uint32 threshold) { value > threshold };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "IsGreaterThan");
        REQUIRE(mod.constraints[0].params.size() == 1);

        // Check parameter
        CHECK(mod.constraints[0].params[0].name == "threshold");
        auto* int_type = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[0].param_type.node);
        REQUIRE(int_type != nullptr);
        CHECK(int_type->is_signed == false);
        CHECK(int_type->bits == 32);

        // Check condition is a comparison
        auto* binary = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(binary != nullptr);
        CHECK(binary->op == datascript::ast::binary_op::gt);
    }

    TEST_CASE("Constraint with two parameters") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint InRange(uint32 min, uint32 max) { value >= min && value <= max };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "InRange");
        REQUIRE(mod.constraints[0].params.size() == 2);

        // Check first parameter
        CHECK(mod.constraints[0].params[0].name == "min");
        auto* type1 = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[0].param_type.node);
        REQUIRE(type1 != nullptr);
        CHECK(type1->bits == 32);

        // Check second parameter
        CHECK(mod.constraints[0].params[1].name == "max");
        auto* type2 = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[1].param_type.node);
        REQUIRE(type2 != nullptr);
        CHECK(type2->bits == 32);

        // Check condition is logical AND
        auto* and_expr = std::get_if<datascript::ast::binary_expr>(&mod.constraints[0].condition.node);
        REQUIRE(and_expr != nullptr);
        CHECK(and_expr->op == datascript::ast::binary_op::log_and);
    }

    TEST_CASE("Constraint with three parameters") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint ValidTriangle(uint16 a, uint16 b, uint16 c) { a + b > c && b + c > a && a + c > b };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "ValidTriangle");
        REQUIRE(mod.constraints[0].params.size() == 3);

        CHECK(mod.constraints[0].params[0].name == "a");
        CHECK(mod.constraints[0].params[1].name == "b");
        CHECK(mod.constraints[0].params[2].name == "c");

        // All should be uint16
        for (int i = 0; i < 3; i++) {
            auto* type = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[i].param_type.node);
            REQUIRE(type != nullptr);
            CHECK(type->is_signed == false);
            CHECK(type->bits == 16);
        }
    }

    TEST_CASE("Constraint with different parameter types") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint ValidConfig(bool enabled, uint32 size, string name) { enabled || size > 0 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "ValidConfig");
        REQUIRE(mod.constraints[0].params.size() == 3);

        // Check bool parameter
        CHECK(mod.constraints[0].params[0].name == "enabled");
        auto* bool_type = std::get_if<datascript::ast::bool_type>(&mod.constraints[0].params[0].param_type.node);
        REQUIRE(bool_type != nullptr);

        // Check uint32 parameter
        CHECK(mod.constraints[0].params[1].name == "size");
        auto* int_type = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[1].param_type.node);
        REQUIRE(int_type != nullptr);
        CHECK(int_type->bits == 32);

        // Check string parameter
        CHECK(mod.constraints[0].params[2].name == "name");
        auto* str_type = std::get_if<datascript::ast::string_type>(&mod.constraints[0].params[2].param_type.node);
        REQUIRE(str_type != nullptr);
    }

    TEST_CASE("Constraint with endianness in parameter") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint CheckLittle(little uint32 value) { value > 0 };"
        ));

        REQUIRE(mod.constraints.size() == 1);
        REQUIRE(mod.constraints[0].params.size() == 1);

        CHECK(mod.constraints[0].params[0].name == "value");
        auto* type = std::get_if<datascript::ast::primitive_type>(&mod.constraints[0].params[0].param_type.node);
        REQUIRE(type != nullptr);
        CHECK(type->bits == 32);
        CHECK(type->byte_order == datascript::ast::endian::little);
    }

    TEST_CASE("Mix of simple and parameterized constraints") {
        auto mod = datascript::parse_datascript(std::string(
            "constraint Simple { value > 0 };\n"
            "constraint WithParam(uint32 min) { value >= min };\n"
            "constraint AnotherSimple { enabled };\n"
        ));

        REQUIRE(mod.constraints.size() == 3);

        // First constraint - simple
        CHECK(mod.constraints[0].name == "Simple");
        CHECK(mod.constraints[0].params.empty());

        // Second constraint - parameterized
        CHECK(mod.constraints[1].name == "WithParam");
        REQUIRE(mod.constraints[1].params.size() == 1);
        CHECK(mod.constraints[1].params[0].name == "min");

        // Third constraint - simple
        CHECK(mod.constraints[2].name == "AnotherSimple");
        CHECK(mod.constraints[2].params.empty());
    }
}

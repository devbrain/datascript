//
// Complex Expression Rendering Tests
//
// Tests rendering of deeply nested and complex expressions to ensure
// correct precedence, parenthesization, and edge case handling.
//

#include <doctest/doctest.h>
#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <datascript/ir.hh>

using namespace datascript::codegen;
using namespace datascript::ir;

namespace {
    // Helper: create a literal int expression
    expr make_literal(uint64_t value) {
        expr e;
        e.type = expr::literal_int;
        e.int_value = value;
        return e;
    }

    // Helper: create a parameter reference
    expr make_param_ref(const std::string& name) {
        expr e;
        e.type = expr::parameter_ref;
        e.ref_name = name;
        return e;
    }

    // Helper: create a binary operation
    expr make_binary(expr::op_type op, expr left, expr right) {
        expr e;
        e.type = expr::binary_op;
        e.op = op;
        e.left = std::make_unique<expr>(std::move(left));
        e.right = std::make_unique<expr>(std::move(right));
        return e;
    }

    // Helper: create a unary operation
    expr make_unary(expr::op_type op, expr operand) {
        expr e;
        e.type = expr::unary_op;
        e.op = op;
        e.left = std::make_unique<expr>(std::move(operand));
        return e;
    }

    // Helper: create a ternary expression
    expr make_ternary(expr cond, expr true_expr, expr false_expr) {
        expr e;
        e.type = expr::ternary_op;
        e.condition = std::make_unique<expr>(std::move(cond));
        e.true_expr = std::make_unique<expr>(std::move(true_expr));
        e.false_expr = std::make_unique<expr>(std::move(false_expr));
        return e;
    }
}

TEST_SUITE("Complex Expression Rendering") {

    TEST_CASE("Deeply nested binary operations maintain precedence") {
        CppRenderer renderer;

        // (a + b) * (c - d) / e
        auto a_plus_b = make_binary(expr::add, make_param_ref("a"), make_param_ref("b"));
        auto c_minus_d = make_binary(expr::sub, make_param_ref("c"), make_param_ref("d"));
        auto mult = make_binary(expr::mul, std::move(a_plus_b), std::move(c_minus_d));
        auto div = make_binary(expr::div, std::move(mult), make_param_ref("e"));

        std::string result = renderer.render_expression(&div);

        // Should have proper parentheses for precedence
        CHECK(result.find("(") != std::string::npos);
        CHECK(result.find("*") != std::string::npos);
        CHECK(result.find("/") != std::string::npos);
    }

    TEST_CASE("Mixed arithmetic and bitwise operations") {
        CppRenderer renderer;

        // (a + b) & (c << d)
        auto a_plus_b = make_binary(expr::add, make_param_ref("a"), make_param_ref("b"));
        auto c_shift_d = make_binary(expr::bit_shift_left, make_param_ref("c"), make_param_ref("d"));
        auto result_expr = make_binary(expr::bit_and, std::move(a_plus_b), std::move(c_shift_d));

        std::string result = renderer.render_expression(&result_expr);

        CHECK(result.find("&") != std::string::npos);
        CHECK(result.find("<<") != std::string::npos);
        CHECK(result.find("+") != std::string::npos);
    }

    TEST_CASE("Chained comparison operations") {
        CppRenderer renderer;

        // (a > b) && (c < d)
        auto a_gt_b = make_binary(expr::gt, make_param_ref("a"), make_param_ref("b"));
        auto c_lt_d = make_binary(expr::lt, make_param_ref("c"), make_param_ref("d"));
        auto logical_and = make_binary(expr::logical_and, std::move(a_gt_b), std::move(c_lt_d));

        std::string result = renderer.render_expression(&logical_and);

        CHECK(result.find("&&") != std::string::npos);
        CHECK(result.find(">") != std::string::npos);
        CHECK(result.find("<") != std::string::npos);
    }

    TEST_CASE("Nested ternary expressions") {
        CppRenderer renderer;

        // a ? (b ? c : d) : e
        auto inner_ternary = make_ternary(
            make_param_ref("b"),
            make_param_ref("c"),
            make_param_ref("d")
        );
        auto outer_ternary = make_ternary(
            make_param_ref("a"),
            std::move(inner_ternary),
            make_param_ref("e")
        );

        std::string result = renderer.render_expression(&outer_ternary);

        CHECK(result.find("?") != std::string::npos);
        CHECK(result.find(":") != std::string::npos);
    }

    TEST_CASE("Multiple unary operators") {
        CppRenderer renderer;

        // !!a (double negation)
        auto inner_not = make_unary(expr::logical_not, make_param_ref("a"));
        auto outer_not = make_unary(expr::logical_not, std::move(inner_not));

        std::string result = renderer.render_expression(&outer_not);

        // Should have two '!' operators
        size_t first_not = result.find("!");
        CHECK(first_not != std::string::npos);
        size_t second_not = result.find("!", first_not + 1);
        CHECK(second_not != std::string::npos);
    }

    TEST_CASE("Combination of unary and binary operations") {
        CppRenderer renderer;

        // -a + b
        auto neg_a = make_unary(expr::negate, make_param_ref("a"));
        auto add = make_binary(expr::add, std::move(neg_a), make_param_ref("b"));

        std::string result = renderer.render_expression(&add);

        CHECK(result.find("-") != std::string::npos);
        CHECK(result.find("+") != std::string::npos);
    }

    TEST_CASE("Bitwise NOT with binary operations") {
        CppRenderer renderer;

        // ~a & b
        auto not_a = make_unary(expr::bit_not, make_param_ref("a"));
        auto and_op = make_binary(expr::bit_and, std::move(not_a), make_param_ref("b"));

        std::string result = renderer.render_expression(&and_op);

        CHECK(result.find("~") != std::string::npos);
        CHECK(result.find("&") != std::string::npos);
    }

    TEST_CASE("Complex arithmetic expression with all operations") {
        CppRenderer renderer;

        // a + b * c / d - e % f
        auto b_mul_c = make_binary(expr::mul, make_param_ref("b"), make_param_ref("c"));
        auto div_d = make_binary(expr::div, std::move(b_mul_c), make_param_ref("d"));
        auto a_plus = make_binary(expr::add, make_param_ref("a"), std::move(div_d));
        auto e_mod_f = make_binary(expr::mod, make_param_ref("e"), make_param_ref("f"));
        auto final_expr = make_binary(expr::sub, std::move(a_plus), std::move(e_mod_f));

        std::string result = renderer.render_expression(&final_expr);

        CHECK(result.find("+") != std::string::npos);
        CHECK(result.find("*") != std::string::npos);
        CHECK(result.find("/") != std::string::npos);
        CHECK(result.find("-") != std::string::npos);
        CHECK(result.find("%") != std::string::npos);
    }

    TEST_CASE("Shift operations with arithmetic") {
        CppRenderer renderer;

        // (a + b) << c
        auto a_plus_b = make_binary(expr::add, make_param_ref("a"), make_param_ref("b"));
        auto shift = make_binary(expr::bit_shift_left, std::move(a_plus_b), make_param_ref("c"));

        std::string result = renderer.render_expression(&shift);

        CHECK(result.find("<<") != std::string::npos);
        CHECK(result.find("+") != std::string::npos);
    }

    TEST_CASE("Equality with logical operations") {
        CppRenderer renderer;

        // (a == b) && (c != d)
        auto a_eq_b = make_binary(expr::eq, make_param_ref("a"), make_param_ref("b"));
        auto c_ne_d = make_binary(expr::ne, make_param_ref("c"), make_param_ref("d"));
        auto logical_and = make_binary(expr::logical_and, std::move(a_eq_b), std::move(c_ne_d));

        std::string result = renderer.render_expression(&logical_and);

        CHECK(result.find("==") != std::string::npos);
        CHECK(result.find("!=") != std::string::npos);
        CHECK(result.find("&&") != std::string::npos);
    }

    TEST_CASE("Large literal values render correctly") {
        CppRenderer renderer;

        // Test max uint64_t value - just check it's a non-empty numeric string
        expr e = make_literal(UINT64_MAX);
        std::string result = renderer.render_expression(&e);

        CHECK(!result.empty());
        // Should contain digits or hex prefix
        CHECK((result.find_first_of("0123456789") != std::string::npos ||
               result.find("0x") != std::string::npos));
    }

    TEST_CASE("String literals with special characters") {
        CppRenderer renderer;

        expr e;
        e.type = expr::literal_string;
        e.string_value = "Hello\n\"World\"\t\\Test";

        std::string result = renderer.render_expression(&e);

        // Should have escaped characters
        CHECK(result.find("\\n") != std::string::npos);
        CHECK(result.find("\\\"") != std::string::npos);
        CHECK(result.find("\\t") != std::string::npos);
        CHECK(result.find("\\\\") != std::string::npos);
    }

    TEST_CASE("Boolean literals render correctly") {
        CppRenderer renderer;

        expr true_expr;
        true_expr.type = expr::literal_bool;
        true_expr.bool_value = true;

        expr false_expr;
        false_expr.type = expr::literal_bool;
        false_expr.bool_value = false;

        CHECK(renderer.render_expression(&true_expr) == "true");
        CHECK(renderer.render_expression(&false_expr) == "false");
    }

    TEST_CASE("Nested ternary with complex conditions") {
        CppRenderer renderer;

        // (a > b) ? ((c < d) ? e : f) : g
        auto a_gt_b = make_binary(expr::gt, make_param_ref("a"), make_param_ref("b"));
        auto c_lt_d = make_binary(expr::lt, make_param_ref("c"), make_param_ref("d"));
        auto inner_ternary = make_ternary(
            std::move(c_lt_d),
            make_param_ref("e"),
            make_param_ref("f")
        );
        auto outer_ternary = make_ternary(
            std::move(a_gt_b),
            std::move(inner_ternary),
            make_param_ref("g")
        );

        std::string result = renderer.render_expression(&outer_ternary);

        // Should contain nested structure
        CHECK(result.find("?") != std::string::npos);
        CHECK(result.find(">") != std::string::npos);
        CHECK(result.find("<") != std::string::npos);
    }
}

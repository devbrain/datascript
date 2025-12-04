//
// Tests for C++ Expression Renderer
//

#include <doctest/doctest.h>
#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <datascript/base_renderer.hh>

using namespace datascript;
using namespace datascript::codegen;

// Test helper class to access internal expression rendering
class CppRendererTest : public CppRenderer {
public:
    std::string render_expr_for_test(const ir::expr& expr, const ExprContext& ctx) {
        return render_expr_internal(expr, ctx);
    }
};

TEST_SUITE("Codegen - C++ Expression Renderer") {

    // Helper to create expressions
    ir::expr make_literal_int(int64_t value) {
        ir::expr e;
        e.type = ir::expr::literal_int;
        e.int_value = value;
        return e;
    }

    ir::expr make_literal_bool(bool value) {
        ir::expr e;
        e.type = ir::expr::literal_bool;
        e.bool_value = value;
        return e;
    }

    ir::expr make_literal_string(const std::string& value) {
        ir::expr e;
        e.type = ir::expr::literal_string;
        e.string_value = value;
        return e;
    }

    ir::expr make_field_ref(const std::string& name) {
        ir::expr e;
        e.type = ir::expr::field_ref;
        e.ref_name = name;
        return e;
    }

    ir::expr make_constant_ref(const std::string& name) {
        ir::expr e;
        e.type = ir::expr::constant_ref;
        e.ref_name = name;
        return e;
    }

    ir::expr make_binary_op(ir::expr::op_type op, ir::expr left, ir::expr right) {
        ir::expr e;
        e.type = ir::expr::binary_op;
        e.op = op;
        e.left = std::make_unique<ir::expr>(std::move(left));
        e.right = std::make_unique<ir::expr>(std::move(right));
        return e;
    }

    ir::expr make_unary_op(ir::expr::op_type op, ir::expr operand) {
        ir::expr e;
        e.type = ir::expr::unary_op;
        e.op = op;
        e.left = std::make_unique<ir::expr>(std::move(operand));
        return e;
    }

    ir::expr make_ternary_op(ir::expr cond, ir::expr true_expr, ir::expr false_expr) {
        ir::expr e;
        e.type = ir::expr::ternary_op;
        e.condition = std::make_unique<ir::expr>(std::move(cond));
        e.true_expr = std::make_unique<ir::expr>(std::move(true_expr));
        e.false_expr = std::make_unique<ir::expr>(std::move(false_expr));
        return e;
    }

    // ========================================================================
    // Literal Tests
    // ========================================================================

    TEST_CASE("Literal integer") {
        CppRendererTest renderer;
        ExprContext ctx;

        auto expr = make_literal_int(42);
        CHECK(renderer.render_expr_for_test(expr, ctx) == "42");

        auto expr_neg = make_literal_int(-123);
        CHECK(renderer.render_expr_for_test(expr_neg, ctx) == "-123");

        auto expr_zero = make_literal_int(0);
        CHECK(renderer.render_expr_for_test(expr_zero, ctx) == "0");
    }

    TEST_CASE("Literal boolean") {
        CppRendererTest renderer;
        ExprContext ctx;

        auto expr_true = make_literal_bool(true);
        CHECK(renderer.render_expr_for_test(expr_true, ctx) == "true");

        auto expr_false = make_literal_bool(false);
        CHECK(renderer.render_expr_for_test(expr_false, ctx) == "false");
    }

    TEST_CASE("Literal string") {
        CppRendererTest renderer;
        ExprContext ctx;

        auto expr = make_literal_string("hello");
        CHECK(renderer.render_expr_for_test(expr, ctx) == "\"hello\"");

        auto expr_empty = make_literal_string("");
        CHECK(renderer.render_expr_for_test(expr_empty, ctx) == "\"\"");
    }

    // ========================================================================
    // Reference Tests
    // ========================================================================

    TEST_CASE("Field reference without context") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = false;

        auto expr = make_field_ref("count");
        CHECK(renderer.render_expr_for_test(expr, ctx) == "count");
    }

    TEST_CASE("Field reference in struct method") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;
        ctx.object_name = "obj";

        auto expr = make_field_ref("count");
        CHECK(renderer.render_expr_for_test(expr, ctx) == "obj.count");
    }

    TEST_CASE("Field reference with custom object name") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;
        ctx.object_name = "data";

        auto expr = make_field_ref("value");
        CHECK(renderer.render_expr_for_test(expr, ctx) == "data.value");
    }

    TEST_CASE("Constant reference") {
        CppRendererTest renderer;
        ExprContext ctx;

        auto expr = make_constant_ref("MAX_SIZE");
        CHECK(renderer.render_expr_for_test(expr, ctx) == "MAX_SIZE");
    }

    // ========================================================================
    // Binary Operation Tests
    // ========================================================================

    TEST_CASE("Binary arithmetic operations") {
        CppRendererTest renderer;
        ExprContext ctx;

        SUBCASE("Addition") {
            auto expr = make_binary_op(ir::expr::add,
                                      make_literal_int(5),
                                      make_literal_int(3));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(5 + 3)");
        }

        SUBCASE("Subtraction") {
            auto expr = make_binary_op(ir::expr::sub,
                                      make_literal_int(10),
                                      make_literal_int(4));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(10 - 4)");
        }

        SUBCASE("Multiplication") {
            auto expr = make_binary_op(ir::expr::mul,
                                      make_literal_int(6),
                                      make_literal_int(7));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(6 * 7)");
        }

        SUBCASE("Division") {
            auto expr = make_binary_op(ir::expr::div,
                                      make_literal_int(20),
                                      make_literal_int(4));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(20 / 4)");
        }

        SUBCASE("Modulo") {
            auto expr = make_binary_op(ir::expr::mod,
                                      make_literal_int(10),
                                      make_literal_int(3));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(10 % 3)");
        }
    }

    TEST_CASE("Binary comparison operations") {
        CppRendererTest renderer;
        ExprContext ctx;

        SUBCASE("Equal") {
            auto expr = make_binary_op(ir::expr::eq,
                                      make_literal_int(5),
                                      make_literal_int(5));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(5 == 5)");
        }

        SUBCASE("Not equal") {
            auto expr = make_binary_op(ir::expr::ne,
                                      make_literal_int(5),
                                      make_literal_int(3));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(5 != 3)");
        }

        SUBCASE("Less than") {
            auto expr = make_binary_op(ir::expr::lt,
                                      make_literal_int(3),
                                      make_literal_int(5));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(3 < 5)");
        }

        SUBCASE("Greater than") {
            auto expr = make_binary_op(ir::expr::gt,
                                      make_literal_int(7),
                                      make_literal_int(4));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(7 > 4)");
        }

        SUBCASE("Less or equal") {
            auto expr = make_binary_op(ir::expr::le,
                                      make_literal_int(3),
                                      make_literal_int(5));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(3 <= 5)");
        }

        SUBCASE("Greater or equal") {
            auto expr = make_binary_op(ir::expr::ge,
                                      make_literal_int(7),
                                      make_literal_int(4));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(7 >= 4)");
        }
    }

    TEST_CASE("Binary logical operations") {
        CppRendererTest renderer;
        ExprContext ctx;

        SUBCASE("Logical AND") {
            auto expr = make_binary_op(ir::expr::logical_and,
                                      make_literal_bool(true),
                                      make_literal_bool(false));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(true && false)");
        }

        SUBCASE("Logical OR") {
            auto expr = make_binary_op(ir::expr::logical_or,
                                      make_literal_bool(true),
                                      make_literal_bool(false));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(true || false)");
        }
    }

    TEST_CASE("Binary bitwise operations") {
        CppRendererTest renderer;
        ExprContext ctx;

        SUBCASE("Bitwise AND") {
            auto expr = make_binary_op(ir::expr::bit_and,
                                      make_literal_int(0xFF),
                                      make_literal_int(0x0F));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(255 & 15)");
        }

        SUBCASE("Bitwise OR") {
            auto expr = make_binary_op(ir::expr::bit_or,
                                      make_literal_int(0xF0),
                                      make_literal_int(0x0F));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(240 | 15)");
        }

        SUBCASE("Bitwise XOR") {
            auto expr = make_binary_op(ir::expr::bit_xor,
                                      make_literal_int(0xFF),
                                      make_literal_int(0x0F));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(255 ^ 15)");
        }

        SUBCASE("Bit shift left") {
            auto expr = make_binary_op(ir::expr::bit_shift_left,
                                      make_literal_int(1),
                                      make_literal_int(4));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(1 << 4)");
        }

        SUBCASE("Bit shift right") {
            auto expr = make_binary_op(ir::expr::bit_shift_right,
                                      make_literal_int(16),
                                      make_literal_int(2));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(16 >> 2)");
        }
    }

    // ========================================================================
    // Unary Operation Tests
    // ========================================================================

    TEST_CASE("Unary operations") {
        CppRendererTest renderer;
        ExprContext ctx;

        SUBCASE("Negation") {
            auto expr = make_unary_op(ir::expr::negate,
                                     make_literal_int(42));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(-42)");
        }

        SUBCASE("Logical NOT") {
            auto expr = make_unary_op(ir::expr::logical_not,
                                     make_literal_bool(true));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(!true)");
        }

        SUBCASE("Bitwise NOT") {
            auto expr = make_unary_op(ir::expr::bit_not,
                                     make_literal_int(0xFF));
            CHECK(renderer.render_expr_for_test(expr, ctx) == "(~255)");
        }
    }

    // ========================================================================
    // Ternary Operation Tests
    // ========================================================================

    TEST_CASE("Ternary conditional") {
        CppRendererTest renderer;
        ExprContext ctx;

        auto expr = make_ternary_op(
            make_literal_bool(true),
            make_literal_int(10),
            make_literal_int(20)
        );

        CHECK(renderer.render_expr_for_test(expr, ctx) == "(true ? 10 : 20)");
    }

    // ========================================================================
    // Complex Expression Tests
    // ========================================================================

    TEST_CASE("Nested expressions") {
        CppRendererTest renderer;
        ExprContext ctx;

        // (5 + 3) * 2
        auto expr = make_binary_op(ir::expr::mul,
            make_binary_op(ir::expr::add,
                          make_literal_int(5),
                          make_literal_int(3)),
            make_literal_int(2)
        );

        CHECK(renderer.render_expr_for_test(expr, ctx) == "((5 + 3) * 2)");
    }

    TEST_CASE("Expression with field references") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;
        ctx.object_name = "obj";

        // obj.count > 10
        auto expr = make_binary_op(ir::expr::gt,
                                  make_field_ref("count"),
                                  make_literal_int(10));

        CHECK(renderer.render_expr_for_test(expr, ctx) == "(obj.count > 10)");
    }

    TEST_CASE("Expression with constants") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;

        // obj.value < MAX_SIZE
        auto expr = make_binary_op(ir::expr::lt,
                                  make_field_ref("value"),
                                  make_constant_ref("MAX_SIZE"));

        CHECK(renderer.render_expr_for_test(expr, ctx) == "(obj.value < MAX_SIZE)");
    }

    TEST_CASE("Complex constraint expression") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;

        // obj.count >= 1 && obj.count <= 10
        auto expr = make_binary_op(ir::expr::logical_and,
            make_binary_op(ir::expr::ge,
                          make_field_ref("count"),
                          make_literal_int(1)),
            make_binary_op(ir::expr::le,
                          make_field_ref("count"),
                          make_literal_int(10))
        );

        CHECK(renderer.render_expr_for_test(expr, ctx) == "((obj.count >= 1) && (obj.count <= 10))");
    }

    // ========================================================================
    // Variable Context Tests
    // ========================================================================

    TEST_CASE("Variable context mapping") {
        CppRendererTest renderer;
        ExprContext ctx;
        ctx.in_struct_method = true;
        ctx.add_variable("size", "array_size");

        // When we reference "size", it should map to "array_size"
        auto expr = make_field_ref("size");  // This will be treated as parameter_ref

        ir::expr param_expr;
        param_expr.type = ir::expr::parameter_ref;
        param_expr.ref_name = "size";

        CHECK(renderer.render_expr_for_test(param_expr, ctx) == "array_size");
    }
}

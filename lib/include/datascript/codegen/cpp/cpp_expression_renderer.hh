//
// C++ Expression Renderer
//
// Converts IR expressions to C++ expression strings.
// Handles all expression types: literals, references, operators, function calls.
//

#pragma once

#include <string>
#include <datascript/ir.hh>
#include <datascript/base_renderer.hh>  // For ExprContext

namespace datascript::codegen {

/**
 * Renders IR expressions to C++ code.
 *
 * This class encapsulates all expression rendering logic, converting
 * DataScript IR expressions into equivalent C++ code. It handles:
 * - Literals (int, bool, string)
 * - References (parameters, fields, constants)
 * - Binary operations (+, -, *, /, ==, !=, <, >, &&, ||, |, &, ^, <<, >>)
 * - Unary operations (-, !, ~)
 * - Ternary conditional (? :)
 * - Function calls
 * - Array indexing
 *
 * The renderer is context-aware, adjusting field and parameter references
 * based on whether we're in a struct method, union field reader, or
 * other specialized context.
 */
class CppExpressionRenderer {
public:
    /**
     * Constructor.
     *
     * @param ctx Expression context (object name, struct method flags, etc.)
     * @param module IR bundle for constant/type resolution
     */
    CppExpressionRenderer(const ExprContext& ctx, const ir::bundle* module);

    /**
     * Render an IR expression to C++ code.
     *
     * @param expr IR expression to render
     * @return C++ expression string
     */
    std::string render(const ir::expr& expr);

private:
    const ExprContext& ctx_;
    const ir::bundle* module_;  // Reserved for future use

    // Literal rendering
    std::string render_literal_int(int64_t value);
    std::string render_literal_bool(bool value);
    std::string render_literal_string(const std::string& value);

    // Reference rendering
    std::string render_parameter_ref(const std::string& name);
    std::string render_field_ref(const std::string& name);
    std::string render_constant_ref(const std::string& name);

    // Composite expression rendering
    std::string render_array_index(const ir::expr& expr);
    std::string render_function_call(const ir::expr& expr);
    std::string render_binary_op(const ir::expr& expr);
    std::string render_unary_op(const ir::expr& expr);
    std::string render_ternary_op(const ir::expr& expr);

    // Operator string helpers
    std::string get_binary_op_string(ir::expr::op_type op);
    std::string get_unary_op_string(ir::expr::op_type op);
};

}  // namespace datascript::codegen

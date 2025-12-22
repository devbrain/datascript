//
// C++ Expression Renderer Implementation
//

#include <datascript/codegen/cpp/cpp_expression_renderer.hh>
#include <datascript/codegen.hh>  // For invalid_ir_error
#include <sstream>

namespace datascript::codegen {

// ============================================================================
// Construction
// ============================================================================

CppExpressionRenderer::CppExpressionRenderer(const ExprContext& ctx, const ir::bundle* module)
    : ctx_(ctx), module_(module)
{
}

// ============================================================================
// Public API
// ============================================================================

std::string CppExpressionRenderer::render(const ir::expr& expr) {
    switch (expr.type) {
        case ir::expr::literal_int:
            return render_literal_int(static_cast<int64_t>(expr.int_value));

        case ir::expr::literal_bool:
            return render_literal_bool(expr.bool_value);

        case ir::expr::literal_string:
            return render_literal_string(expr.string_value);

        case ir::expr::parameter_ref:
            return render_parameter_ref(expr.ref_name);

        case ir::expr::field_ref:
            return render_field_ref(expr.ref_name);

        case ir::expr::constant_ref:
            return render_constant_ref(expr.ref_name);

        case ir::expr::array_index:
            return render_array_index(expr);

        case ir::expr::binary_op:
            return render_binary_op(expr);

        case ir::expr::unary_op:
            return render_unary_op(expr);

        case ir::expr::ternary_op:
            return render_ternary_op(expr);

        case ir::expr::function_call:
            return render_function_call(expr);

        default:
            return "/* UNKNOWN EXPR */";
    }
}

// ============================================================================
// Private: Literal Rendering
// ============================================================================

std::string CppExpressionRenderer::render_literal_int(int64_t value) {
    return std::to_string(value);
}

std::string CppExpressionRenderer::render_literal_bool(bool value) {
    return value ? "true" : "false";
}

std::string CppExpressionRenderer::render_literal_string(const std::string& value) {
    std::string result = "\"";
    for (char c : value) {
        switch (c) {
            case '"':  result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n";  break;
            case '\r': result += "\\r";  break;
            case '\t': result += "\\t";  break;
            case '\0': result += "\\0";  break;
            default:   result += c;      break;
        }
    }
    result += "\"";
    return result;
}

// ============================================================================
// Private: Reference Rendering
// ============================================================================

std::string CppExpressionRenderer::render_parameter_ref(const std::string& name) {
    // In struct methods, parameter_ref might be a constant or field reference
    // Check if it's a variable first
    if (ctx_.variable_names.count(name)) {
        return ctx_.get_variable(name);
    }

    // Check if it's a module constant (namespace-level, no object prefix needed)
    if (ctx_.module_constants && ctx_.module_constants->count(name)) {
        return name;
    }

    // In struct methods, treat as field reference if we don't know otherwise
    if (ctx_.in_struct_method) {
        return ctx_.object_name + "." + name;
    }

    return name;
}

std::string CppExpressionRenderer::render_field_ref(const std::string& name) {
    // Union field readers: use parent-> prefix for parent struct field references
    // EXCEPT for the field currently being read (it's a local variable)
    // Also handle member access like "opt_header_32.Magic" where opt_header_32 is current field
    if (ctx_.use_parent_context && !ctx_.current_field_name.empty()) {
        // Check if this is a self-reference or member access on self
        bool is_self_ref = (name == ctx_.current_field_name) ||
                          (name.rfind(ctx_.current_field_name + ".", 0) == 0);
        if (!is_self_ref) {
            return "parent->" + name;
        }
    } else if (ctx_.use_parent_context) {
        return "parent->" + name;
    }

    // Field references need object prefix when in struct methods
    if (ctx_.in_struct_method) {
        return ctx_.object_name + "." + name;
    }

    return name;
}

std::string CppExpressionRenderer::render_constant_ref(const std::string& name) {
    // Constants are just their name (they're global/namespace-level)
    // Convert DataScript qualified names (Enum.MEMBER) to C++ scope resolution (Enum::MEMBER)
    std::string result = name;
    size_t pos = 0;
    while ((pos = result.find('.', pos)) != std::string::npos) {
        result.replace(pos, 1, "::");
        pos += 2;  // Skip past the "::" we just inserted
    }

    return result;
}

// ============================================================================
// Private: Composite Expression Rendering
// ============================================================================

std::string CppExpressionRenderer::render_array_index(const ir::expr& expr) {
    if (!expr.left || !expr.right) {
        return "/* ERROR: incomplete array index */";
    }

    std::string array = render(*expr.left);
    std::string index = render(*expr.right);

    return array + "[" + index + "]";
}

std::string CppExpressionRenderer::render_binary_op(const ir::expr& expr) {
    if (!expr.left || !expr.right) {
        return "/* ERROR: incomplete binary op */";
    }

    std::string left = render(*expr.left);
    std::string right = render(*expr.right);
    std::string op = get_binary_op_string(expr.op);

    return "(" + left + op + right + ")";
}

std::string CppExpressionRenderer::get_binary_op_string(ir::expr::op_type op) {
    switch (op) {
        // Arithmetic
        case ir::expr::add:              return " + ";
        case ir::expr::sub:              return " - ";
        case ir::expr::mul:              return " * ";
        case ir::expr::div:              return " / ";
        case ir::expr::mod:              return " % ";

        // Comparison
        case ir::expr::eq:               return " == ";
        case ir::expr::ne:               return " != ";
        case ir::expr::lt:               return " < ";
        case ir::expr::gt:               return " > ";
        case ir::expr::le:               return " <= ";
        case ir::expr::ge:               return " >= ";

        // Logical
        case ir::expr::logical_and:      return " && ";
        case ir::expr::logical_or:       return " || ";

        // Bitwise
        case ir::expr::bit_and:          return " & ";
        case ir::expr::bit_or:           return " | ";
        case ir::expr::bit_xor:          return " ^ ";
        case ir::expr::bit_shift_left:   return " << ";
        case ir::expr::bit_shift_right:  return " >> ";

        default:
            return " /* UNKNOWN OP */ ";
    }
}

std::string CppExpressionRenderer::render_unary_op(const ir::expr& expr) {
    if (!expr.left) {
        return "/* ERROR: incomplete unary op */";
    }

    std::string operand = render(*expr.left);
    std::string op = get_unary_op_string(expr.op);

    return "(" + op + operand + ")";
}

std::string CppExpressionRenderer::get_unary_op_string(ir::expr::op_type op) {
    switch (op) {
        case ir::expr::negate:         return "-";
        case ir::expr::logical_not:    return "!";
        case ir::expr::bit_not:        return "~";
        default:
            return "/* UNKNOWN UNARY OP */";
    }
}

std::string CppExpressionRenderer::render_ternary_op(const ir::expr& expr) {
    if (!expr.condition || !expr.true_expr || !expr.false_expr) {
        return "/* ERROR: incomplete ternary op */";
    }

    std::string cond = render(*expr.condition);
    std::string true_val = render(*expr.true_expr);
    std::string false_val = render(*expr.false_expr);

    return "(" + cond + " ? " + true_val + " : " + false_val + ")";
}

std::string CppExpressionRenderer::render_function_call(const ir::expr& expr) {
    std::string func_name = expr.ref_name;

    // If in struct method and this is a simple function name (not qualified),
    // it's likely a method call on the current object
    if (ctx_.in_struct_method && func_name.find('.') == std::string::npos && func_name.find("::") == std::string::npos) {
        func_name = ctx_.object_name + "." + func_name;
    }

    // Build argument list
    std::ostringstream args_stream;
    for (size_t i = 0; i < expr.arguments.size(); ++i) {
        if (i > 0) args_stream << ", ";
        args_stream << render(*expr.arguments[i]);
    }

    return func_name + "(" + args_stream.str() + ")";
}

}  // namespace datascript::codegen

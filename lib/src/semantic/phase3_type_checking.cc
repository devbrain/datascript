//
// Phase 3: Type Checking
//
// Verifies that all expressions have valid types and that operations
// are performed on compatible types.
//
// This phase focuses on validation only - it doesn't build a type map.
//

#include <datascript/semantic.hh>
#include <algorithm>

namespace datascript::semantic::phases {

namespace {
    // Helper: add error diagnostic
    void add_error(std::vector<diagnostic>& diags,
                  const char* code,
                  const std::string& message,
                  const ast::source_pos& pos) {
        diags.push_back(diagnostic{
            diagnostic_level::error,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });
    }

    // ========================================================================
    // Type Classification
    // ========================================================================

    enum class type_cat {
        integer,
        boolean,
        string,
        array,
        user_defined,
        bitfield,
        unknown
    };

    type_cat categorize_type(const ast::type& t) {
        if (std::holds_alternative<ast::primitive_type>(t.node)) {
            return type_cat::integer;
        }
        if (std::holds_alternative<ast::bool_type>(t.node)) {
            return type_cat::boolean;
        }
        if (std::holds_alternative<ast::string_type>(t.node)) {
            return type_cat::string;
        }
        if (std::holds_alternative<ast::array_type_fixed>(t.node) ||
            std::holds_alternative<ast::array_type_range>(t.node) ||
            std::holds_alternative<ast::array_type_unsized>(t.node)) {
            return type_cat::array;
        }
        if (std::holds_alternative<ast::qualified_name>(t.node)) {
            return type_cat::user_defined;
        }
        if (std::holds_alternative<ast::bit_field_type_fixed>(t.node) ||
            std::holds_alternative<ast::bit_field_type_expr>(t.node)) {
            return type_cat::bitfield;
        }
        return type_cat::unknown;
    }

    // Convert type_cat to readable string for error messages
    std::string type_cat_to_string(type_cat cat) {
        switch (cat) {
            case type_cat::integer: return "integer";
            case type_cat::boolean: return "boolean";
            case type_cat::string: return "string";
            case type_cat::array: return "array";
            case type_cat::user_defined: return "user-defined";
            case type_cat::bitfield: return "bitfield";
            case type_cat::unknown: return "unknown";
        }
        return "unknown";
    }

    // Convert ast::type to readable string for error messages
    std::string type_to_string(const ast::type& t) {
        if (std::holds_alternative<ast::primitive_type>(t.node)) {
            const auto& prim = std::get<ast::primitive_type>(t.node);
            std::string name = prim.is_signed ? "int" : "uint";
            name += std::to_string(prim.bits);
            return name;  // e.g., "uint32", "int8"
        }
        if (std::holds_alternative<ast::bool_type>(t.node)) {
            return "bool";
        }
        if (std::holds_alternative<ast::string_type>(t.node)) {
            return "string";
        }
        if (std::holds_alternative<ast::array_type_fixed>(t.node)) {
            const auto& arr = std::get<ast::array_type_fixed>(t.node);
            return type_to_string(*arr.element_type) + "[...]";
        }
        if (std::holds_alternative<ast::array_type_range>(t.node)) {
            const auto& arr = std::get<ast::array_type_range>(t.node);
            return type_to_string(*arr.element_type) + "[..]";
        }
        if (std::holds_alternative<ast::array_type_unsized>(t.node)) {
            const auto& arr = std::get<ast::array_type_unsized>(t.node);
            return type_to_string(*arr.element_type) + "[]";
        }
        if (std::holds_alternative<ast::qualified_name>(t.node)) {
            const auto& qname = std::get<ast::qualified_name>(t.node);
            std::string result;
            for (size_t i = 0; i < qname.parts.size(); ++i) {
                if (i > 0) result += ".";
                result += qname.parts[i];
            }
            return result;
        }
        if (std::holds_alternative<ast::bit_field_type_fixed>(t.node)) {
            const auto& bf = std::get<ast::bit_field_type_fixed>(t.node);
            return "bit:" + std::to_string(bf.width);
        }
        if (std::holds_alternative<ast::bit_field_type_expr>(t.node)) {
            return "bit:<expr>";
        }
        return "unknown";
    }

    // Convert binary operator to readable string for error messages
    std::string op_to_string(ast::binary_op op) {
        switch (op) {
            case ast::binary_op::add: return "+";
            case ast::binary_op::sub: return "-";
            case ast::binary_op::mul: return "*";
            case ast::binary_op::div: return "/";
            case ast::binary_op::mod: return "%";
            case ast::binary_op::bit_and: return "&";
            case ast::binary_op::bit_or: return "|";
            case ast::binary_op::bit_xor: return "^";
            case ast::binary_op::lshift: return "<<";
            case ast::binary_op::rshift: return ">>";
            case ast::binary_op::eq: return "==";
            case ast::binary_op::ne: return "!=";
            case ast::binary_op::lt: return "<";
            case ast::binary_op::gt: return ">";
            case ast::binary_op::le: return "<=";
            case ast::binary_op::ge: return ">=";
            case ast::binary_op::log_and: return "&&";
            case ast::binary_op::log_or: return "||";
        }
        return "?";
    }

    // Convert unary operator to readable string for error messages
    std::string op_to_string(ast::unary_op op) {
        switch (op) {
            case ast::unary_op::neg: return "unary -";
            case ast::unary_op::pos: return "unary +";
            case ast::unary_op::bit_not: return "~";
            case ast::unary_op::log_not: return "!";
        }
        return "?";
    }

    bool types_compatible(const ast::type& t1, const ast::type& t2) {
        auto cat1 = categorize_type(t1);
        auto cat2 = categorize_type(t2);

        // Same category = compatible (simplified)
        return cat1 == cat2;
    }

    // ========================================================================
    // Expression Type Checking
    // ========================================================================

    // Context for type checking expressions within struct/union definitions
    struct type_check_context {
        const ast::struct_def* current_struct = nullptr;
        const ast::union_def* current_union = nullptr;
        const ast::choice_def* current_choice = nullptr;
    };

    // Forward declarations
    type_cat check_expr(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags);

    type_cat check_expr_with_context(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        const type_check_context& ctx,
        std::vector<diagnostic>& diags);

    type_cat check_binary_expr(
        const ast::binary_expr& binary,
        const analyzed_module_set& analyzed,
        const type_check_context& ctx,
        std::vector<diagnostic>& diags)
    {
        auto left_cat = check_expr_with_context(*binary.left, analyzed, ctx, diags);
        auto right_cat = check_expr_with_context(*binary.right, analyzed, ctx, diags);

        switch (binary.op) {
            case ast::binary_op::add:
            case ast::binary_op::sub:
            case ast::binary_op::mul:
            case ast::binary_op::div:
            case ast::binary_op::mod:
            case ast::binary_op::bit_and:
            case ast::binary_op::bit_or:
            case ast::binary_op::bit_xor:
            case ast::binary_op::lshift:
            case ast::binary_op::rshift:
                // Arithmetic/bitwise operators require integer operands
                if (left_cat != type_cat::integer) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(binary.op) + "' requires integer operands, " +
                        "but left operand has type '" + type_cat_to_string(left_cat) + "'",
                        binary.pos);
                }
                if (right_cat != type_cat::integer) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(binary.op) + "' requires integer operands, " +
                        "but right operand has type '" + type_cat_to_string(right_cat) + "'",
                        binary.pos);
                }
                return type_cat::integer;

            case ast::binary_op::eq:
            case ast::binary_op::ne:
            case ast::binary_op::lt:
            case ast::binary_op::gt:
            case ast::binary_op::le:
            case ast::binary_op::ge:
                // Comparison operators require compatible operands
                if (left_cat != right_cat &&
                    left_cat != type_cat::unknown &&
                    right_cat != type_cat::unknown) {
                    add_error(diags, diag_codes::E_INCOMPATIBLE_TYPES,
                        "Operator '" + op_to_string(binary.op) + "' requires compatible operands, " +
                        "but got '" + type_cat_to_string(left_cat) + "' and '" +
                        type_cat_to_string(right_cat) + "'",
                        binary.pos);
                }
                return type_cat::boolean;

            case ast::binary_op::log_and:
            case ast::binary_op::log_or:
                // Logical operators require boolean operands
                if (left_cat != type_cat::boolean) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(binary.op) + "' requires boolean operands, " +
                        "but left operand has type '" + type_cat_to_string(left_cat) + "'",
                        binary.pos);
                }
                if (right_cat != type_cat::boolean) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(binary.op) + "' requires boolean operands, " +
                        "but right operand has type '" + type_cat_to_string(right_cat) + "'",
                        binary.pos);
                }
                return type_cat::boolean;
        }

        return type_cat::unknown;
    }

    type_cat check_unary_expr(
        const ast::unary_expr& unary,
        const analyzed_module_set& analyzed,
        const type_check_context& ctx,
        std::vector<diagnostic>& diags)
    {
        auto operand_cat = check_expr_with_context(*unary.operand, analyzed, ctx, diags);

        switch (unary.op) {
            case ast::unary_op::neg:
            case ast::unary_op::pos:
            case ast::unary_op::bit_not:
                if (operand_cat != type_cat::integer) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(unary.op) + "' requires integer operand, " +
                        "but got type '" + type_cat_to_string(operand_cat) + "'",
                        unary.pos);
                }
                return type_cat::integer;

            case ast::unary_op::log_not:
                if (operand_cat != type_cat::boolean) {
                    add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                        "Operator '" + op_to_string(unary.op) + "' requires boolean operand, " +
                        "but got type '" + type_cat_to_string(operand_cat) + "'",
                        unary.pos);
                }
                return type_cat::boolean;
        }

        return type_cat::unknown;
    }

    type_cat check_ternary_expr(
        const ast::ternary_expr& ternary,
        const analyzed_module_set& analyzed,
        const type_check_context& ctx,
        std::vector<diagnostic>& diags)
    {
        auto cond_cat = check_expr_with_context(*ternary.condition, analyzed, ctx, diags);
        auto true_cat = check_expr_with_context(*ternary.true_expr, analyzed, ctx, diags);
        auto false_cat = check_expr_with_context(*ternary.false_expr, analyzed, ctx, diags);

        if (cond_cat != type_cat::boolean) {
            add_error(diags, diag_codes::E_TYPE_MISMATCH,
                "Ternary condition must be boolean type", ternary.pos);
        }

        if (true_cat != false_cat &&
            true_cat != type_cat::unknown &&
            false_cat != type_cat::unknown) {
            add_error(diags, diag_codes::E_INCOMPATIBLE_TYPES,
                "Ternary branches have incompatible types", ternary.pos);
        }

        return true_cat;
    }

    // Context-aware expression type checking (can resolve field references)
    type_cat check_expr_with_context(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        const type_check_context& ctx,
        std::vector<diagnostic>& diags)
    {
        if (std::holds_alternative<ast::literal_int>(expr.node)) {
            return type_cat::integer;
        }
        else if (std::holds_alternative<ast::literal_bool>(expr.node)) {
            return type_cat::boolean;
        }
        else if (std::holds_alternative<ast::literal_string>(expr.node)) {
            return type_cat::string;
        }
        else if (auto* id = std::get_if<ast::identifier>(&expr.node)) {
            // Look up identifier type - first try constants
            if (auto* const_def = analyzed.symbols.find_constant(id->name)) {
                return categorize_type(const_def->ctype);
            }

            // If in struct context, try to resolve field reference
            if (ctx.current_struct) {
                for (const auto& body_item : ctx.current_struct->body) {
                    if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                        if (field->name == id->name) {
                            return categorize_type(field->field_type);
                        }
                    }
                }
            }

            // If in union context, try to resolve field reference
            if (ctx.current_union) {
                for (const auto& union_case : ctx.current_union->cases) {
                    for (const auto& item : union_case.items) {
                        if (std::holds_alternative<ast::field_def>(item)) {
                            const auto& field = std::get<ast::field_def>(item);
                            if (field.name == id->name) {
                                return categorize_type(field.field_type);
                            }
                        }
                    }
                }
            }

            // If in choice context, try to resolve field reference
            if (ctx.current_choice) {
                for (const auto& choice_case : ctx.current_choice->cases) {
                    if (choice_case.field.name == id->name) {
                        return categorize_type(choice_case.field.field_type);
                    }
                }
            }

            return type_cat::unknown;  // Error reported in Phase 2
        }
        else if (auto* binary = std::get_if<ast::binary_expr>(&expr.node)) {
            return check_binary_expr(*binary, analyzed, ctx, diags);
        }
        else if (auto* unary = std::get_if<ast::unary_expr>(&expr.node)) {
            return check_unary_expr(*unary, analyzed, ctx, diags);
        }
        else if (auto* ternary = std::get_if<ast::ternary_expr>(&expr.node)) {
            return check_ternary_expr(*ternary, analyzed, ctx, diags);
        }
        else if (auto* array_index = std::get_if<ast::array_index_expr>(&expr.node)) {
            check_expr_with_context(*array_index->array, analyzed, ctx, diags);
            auto index_cat = check_expr_with_context(*array_index->index, analyzed, ctx, diags);
            if (index_cat != type_cat::integer) {
                add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                    "Array index must be integer type", array_index->pos);
            }
            return type_cat::unknown;  // Element type unknown without full type inference
        }
        else if (auto* field_access = std::get_if<ast::field_access_expr>(&expr.node)) {
            check_expr_with_context(*field_access->object, analyzed, ctx, diags);

            // Try to determine field type for simple cases (object is an identifier)
            if (auto* obj_id = std::get_if<ast::identifier>(&field_access->object->node)) {
                // Look up the object's type
                const ast::type* obj_type = nullptr;

                // Check if it's a field in the current struct
                if (ctx.current_struct) {
                    for (const auto& body_item : ctx.current_struct->body) {
                        if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                            if (field->name == obj_id->name) {
                                obj_type = &field->field_type;
                                break;
                            }
                        }
                    }
                }

                // If found, look up the field within that type
                if (obj_type && std::holds_alternative<ast::qualified_name>(obj_type->node)) {
                    const auto& qname = std::get<ast::qualified_name>(obj_type->node);

                    // Look up struct definition
                    const ast::struct_def* struct_def = nullptr;
                    if (qname.parts.size() == 1) {
                        struct_def = analyzed.symbols.find_struct(qname.parts[0]);
                    } else {
                        struct_def = analyzed.symbols.find_struct_qualified(qname.parts);
                    }

                    // Find field in struct
                    if (struct_def) {
                        for (const auto& body_item : struct_def->body) {
                            if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                                if (field->name == field_access->field_name) {
                                    return categorize_type(field->field_type);
                                }
                            }
                        }
                    }
                }
            }

            return type_cat::unknown;  // Complex expressions or field not found
        }
        else if (auto* func_call = std::get_if<ast::function_call_expr>(&expr.node)) {
            check_expr_with_context(*func_call->function, analyzed, ctx, diags);
            for (const auto& arg : func_call->arguments) {
                check_expr_with_context(arg, analyzed, ctx, diags);
            }

            // Try to determine function return type for simple cases (function is an identifier)
            if (auto* func_id = std::get_if<ast::identifier>(&func_call->function->node)) {
                // Check if it's a function in the current struct
                if (ctx.current_struct) {
                    for (const auto& body_item : ctx.current_struct->body) {
                        if (auto* func = std::get_if<ast::function_def>(&body_item)) {
                            if (func->name == func_id->name) {
                                return categorize_type(func->return_type);
                            }
                        }
                    }
                }

                // Check if it's a function in the current union
                if (ctx.current_union) {
                    for (const auto& union_case : ctx.current_union->cases) {
                        for (const auto& item : union_case.items) {
                            if (auto* func = std::get_if<ast::function_def>(&item)) {
                                if (func->name == func_id->name) {
                                    return categorize_type(func->return_type);
                                }
                            }
                        }
                    }
                }

                // Note: Choices don't have member functions, only structs and unions do
            }

            return type_cat::unknown;  // Complex expressions or function not found
        }

        return type_cat::unknown;
    }

    type_cat check_expr(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        if (std::holds_alternative<ast::literal_int>(expr.node)) {
            return type_cat::integer;
        }
        else if (std::holds_alternative<ast::literal_bool>(expr.node)) {
            return type_cat::boolean;
        }
        else if (std::holds_alternative<ast::literal_string>(expr.node)) {
            return type_cat::string;
        }
        else if (auto* id = std::get_if<ast::identifier>(&expr.node)) {
            // Look up identifier type
            if (auto* const_def = analyzed.symbols.find_constant(id->name)) {
                return categorize_type(const_def->ctype);
            }
            return type_cat::unknown;  // Error reported in Phase 2
        }
        else if (auto* binary = std::get_if<ast::binary_expr>(&expr.node)) {
            type_check_context empty_ctx;
            return check_binary_expr(*binary, analyzed, empty_ctx, diags);
        }
        else if (auto* unary = std::get_if<ast::unary_expr>(&expr.node)) {
            type_check_context empty_ctx;
            return check_unary_expr(*unary, analyzed, empty_ctx, diags);
        }
        else if (auto* ternary = std::get_if<ast::ternary_expr>(&expr.node)) {
            type_check_context empty_ctx;
            return check_ternary_expr(*ternary, analyzed, empty_ctx, diags);
        }
        else if (auto* array_index = std::get_if<ast::array_index_expr>(&expr.node)) {
            check_expr(*array_index->array, analyzed, diags);
            auto index_cat = check_expr(*array_index->index, analyzed, diags);
            if (index_cat != type_cat::integer) {
                add_error(diags, diag_codes::E_INVALID_OPERAND_TYPE,
                    "Array index must be integer type", array_index->pos);
            }
            return type_cat::unknown;  // Element type unknown without full type inference
        }
        else if (auto* field_access = std::get_if<ast::field_access_expr>(&expr.node)) {
            check_expr(*field_access->object, analyzed, diags);
            // Note: Field type lookup without struct context requires full type inference.
            // This would need to track actual types, not just categories.
            // For now, we validate the expression but return unknown type.
            return type_cat::unknown;
        }
        else if (auto* func_call = std::get_if<ast::function_call_expr>(&expr.node)) {
            check_expr(*func_call->function, analyzed, diags);
            for (const auto& arg : func_call->arguments) {
                check_expr(arg, analyzed, diags);
            }
            // Note: Function return type lookup without struct context requires tracking
            // function definitions at module level or maintaining a function registry.
            // For now, we validate the expression but return unknown type.
            return type_cat::unknown;
        }

        return type_cat::unknown;
    }

    // ========================================================================
    // Module-Level Type Checking
    // ========================================================================

    void check_module_types(
        const ast::module& mod,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Check constant definitions
        for (const auto& const_def : mod.constants) {
            auto value_cat = check_expr(const_def.value, analyzed, diags);
            auto declared_cat = categorize_type(const_def.ctype);

            if (value_cat != declared_cat &&
                value_cat != type_cat::unknown &&
                declared_cat != type_cat::unknown) {
                add_error(diags, diag_codes::E_TYPE_MISMATCH,
                    "Constant value type does not match declared type",
                    const_def.pos);
            }
        }

        // Check enum base types must be integer
        for (const auto& enum_def : mod.enums) {
            if (categorize_type(enum_def.base_type) != type_cat::integer) {
                add_error(diags, diag_codes::E_TYPE_MISMATCH,
                    "Enum base type must be an integer type",
                    enum_def.pos);
            }

            // Check enum item values must be integer
            for (const auto& item : enum_def.items) {
                if (item.value) {
                    auto value_cat = check_expr(item.value.value(), analyzed, diags);
                    if (value_cat != type_cat::integer && value_cat != type_cat::unknown) {
                        add_error(diags, diag_codes::E_TYPE_MISMATCH,
                            "Enum item value must be integer type",
                            item.pos);
                    }
                }
            }
        }

        // Check struct field conditions must be boolean
        for (const auto& struct_def : mod.structs) {
            type_check_context ctx;
            ctx.current_struct = &struct_def;

            for (const auto& body_item : struct_def.body) {
                // Only process fields
                if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                    if (field->condition) {
                        auto cond_cat = check_expr_with_context(field->condition.value(), analyzed, ctx, diags);
                        if (cond_cat != type_cat::boolean && cond_cat != type_cat::unknown) {
                            add_error(diags, diag_codes::E_TYPE_MISMATCH,
                                "Field condition must be boolean type",
                                field->pos);
                        }
                    }
                }
            }
        }

        // Check union field and case conditions must be boolean
        for (const auto& union_def : mod.unions) {
            type_check_context ctx;
            ctx.current_union = &union_def;

            for (const auto& union_case : union_def.cases) {
                // Check each item in the case
                for (const auto& item : union_case.items) {
                    if (std::holds_alternative<ast::field_def>(item)) {
                        const auto& field = std::get<ast::field_def>(item);
                        if (field.condition) {
                            auto cond_cat = check_expr_with_context(field.condition.value(), analyzed, ctx, diags);
                            if (cond_cat != type_cat::boolean && cond_cat != type_cat::unknown) {
                                add_error(diags, diag_codes::E_TYPE_MISMATCH,
                                    "Field condition must be boolean type",
                                    field.pos);
                            }
                        }
                    }
                }

                // Check case condition if present
                if (union_case.condition) {
                    auto cond_cat = check_expr_with_context(union_case.condition.value(), analyzed, ctx, diags);
                    if (cond_cat != type_cat::boolean && cond_cat != type_cat::unknown) {
                        add_error(diags, diag_codes::E_TYPE_MISMATCH,
                            "Union case condition must be boolean type",
                            union_case.pos);
                    }
                }
            }
        }

        // Check choice selectors and case expressions
        for (const auto& choice_def : mod.choices) {
            auto selector_cat = check_expr(choice_def.selector, analyzed, diags);

            for (const auto& case_def : choice_def.cases) {
                // Check case expressions match selector type
                for (const auto& case_expr : case_def.case_exprs) {
                    auto case_cat = check_expr(case_expr, analyzed, diags);
                    if (selector_cat != case_cat &&
                        selector_cat != type_cat::unknown &&
                        case_cat != type_cat::unknown) {
                        add_error(diags, diag_codes::E_TYPE_MISMATCH,
                            "Case expression type does not match selector type",
                            choice_def.pos);
                    }
                }

                // Check case field condition
                if (case_def.field.condition) {
                    auto cond_cat = check_expr(case_def.field.condition.value(), analyzed, diags);
                    if (cond_cat != type_cat::boolean && cond_cat != type_cat::unknown) {
                        add_error(diags, diag_codes::E_TYPE_MISMATCH,
                            "Case field condition must be boolean type",
                            case_def.field.pos);
                    }
                }
            }
        }

        // Check constraint conditions must be boolean
        for (const auto& constraint_def : mod.constraints) {
            auto cond_cat = check_expr(constraint_def.condition, analyzed, diags);
            if (cond_cat != type_cat::boolean && cond_cat != type_cat::unknown) {
                add_error(diags, diag_codes::E_TYPE_MISMATCH,
                    "Constraint condition must be boolean type",
                    constraint_def.pos);
            }
        }

        // Helper to build type name from qualified_name
        auto qname_to_string = [](const ast::qualified_name& qname) -> std::string {
            std::string result;
            for (size_t i = 0; i < qname.parts.size(); ++i) {
                if (i > 0) result += ".";
                result += qname.parts[i];
            }
            return result;
        };

        // Check parameterized type argument counts match parameter counts
        auto validate_type_ref = [&](const ast::type& type_ref, const ast::source_pos& pos) {
            if (auto* type_inst = std::get_if<ast::type_instantiation>(&type_ref.node)) {
                // base_type is already a qualified_name
                const auto& qname = type_inst->base_type;

                // Check if it's a struct
                if (auto* struct_def = analyzed.symbols.find_struct_qualified(qname.parts)) {
                    size_t expected_params = struct_def->parameters.size();
                    size_t actual_args = type_inst->arguments.size();
                    if (actual_args != expected_params) {
                        std::string msg = "Type '" + qname_to_string(qname) + "' expects " +
                                        std::to_string(expected_params) + " parameter(s) but got " +
                                        std::to_string(actual_args);
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
                // Check if it's a union
                else if (auto* union_def = analyzed.symbols.find_union_qualified(qname.parts)) {
                    size_t expected_params = union_def->parameters.size();
                    size_t actual_args = type_inst->arguments.size();
                    if (actual_args != expected_params) {
                        std::string msg = "Type '" + qname_to_string(qname) + "' expects " +
                                        std::to_string(expected_params) + " parameter(s) but got " +
                                        std::to_string(actual_args);
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
                // Check if it's a choice
                else if (auto* choice_def = analyzed.symbols.find_choice_qualified(qname.parts)) {
                    size_t expected_params = choice_def->parameters.size();
                    size_t actual_args = type_inst->arguments.size();
                    if (actual_args != expected_params) {
                        std::string msg = "Choice '" + qname_to_string(qname) + "' expects " +
                                        std::to_string(expected_params) + " parameter(s) but got " +
                                        std::to_string(actual_args);
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
            }
            // Also check qualified names without instantiation
            else if (auto* qname = std::get_if<ast::qualified_name>(&type_ref.node)) {
                // Check if this is a parameterized type used without arguments
                if (auto* struct_def = analyzed.symbols.find_struct_qualified(qname->parts)) {
                    if (!struct_def->parameters.empty()) {
                        std::string msg = "Type '" + qname_to_string(*qname) + "' expects " +
                                        std::to_string(struct_def->parameters.size()) +
                                        " parameter(s) but got 0";
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
                else if (auto* union_def = analyzed.symbols.find_union_qualified(qname->parts)) {
                    if (!union_def->parameters.empty()) {
                        std::string msg = "Type '" + qname_to_string(*qname) + "' expects " +
                                        std::to_string(union_def->parameters.size()) +
                                        " parameter(s) but got 0";
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
                else if (auto* choice_def = analyzed.symbols.find_choice_qualified(qname->parts)) {
                    if (!choice_def->parameters.empty()) {
                        std::string msg = "Choice '" + qname_to_string(*qname) + "' expects " +
                                        std::to_string(choice_def->parameters.size()) +
                                        " parameter(s) but got 0";
                        add_error(diags, diag_codes::E_PARAM_COUNT_MISMATCH, msg, pos);
                    }
                }
            }
        };

        // Validate struct field types
        for (const auto& struct_def : mod.structs) {
            for (const auto& body_item : struct_def.body) {
                if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                    validate_type_ref(field->field_type, field->pos);
                }
            }
        }

        // Validate union field types
        for (const auto& union_def : mod.unions) {
            for (const auto& union_case : union_def.cases) {
                for (const auto& item : union_case.items) {
                    if (std::holds_alternative<ast::field_def>(item)) {
                        const auto& field = std::get<ast::field_def>(item);
                        validate_type_ref(field.field_type, field.pos);
                    }
                }
            }
        }

        // Validate choice case field types
        for (const auto& choice_def : mod.choices) {
            for (const auto& case_def : choice_def.cases) {
                validate_type_ref(case_def.field.field_type, case_def.field.pos);
            }
        }
    }

} // anonymous namespace

// ============================================================================
// Public API: Type Checking
// ============================================================================

void check_types(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Check types in main module
    check_module_types(modules.main.module, analyzed, diagnostics);

    // Check types in all imported modules
    for (const auto& imported : modules.imported) {
        check_module_types(imported.module, analyzed, diagnostics);
    }
}

} // namespace datascript::semantic::phases

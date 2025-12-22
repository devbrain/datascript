//
// Phase 4: Constant Evaluation
//
// Evaluates constant expressions at compile time and detects errors
// like overflow, underflow, and division by zero.
//

#include <datascript/semantic.hh>
#include <algorithm>
#include <limits>
#include <set>

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
    // Constant Value Representation
    // ========================================================================

    struct const_value {
        enum class kind { integer, boolean, string_lit };

        kind type;
        int64_t int_val;
        bool bool_val;
        std::string str_val;

        static const_value make_int(int64_t val) {
            const_value v;
            v.type = kind::integer;
            v.int_val = val;
            return v;
        }

        static const_value make_bool(bool val) {
            const_value v;
            v.type = kind::boolean;
            v.bool_val = val;
            return v;
        }

        static const_value make_string(const std::string& val) {
            const_value v;
            v.type = kind::string_lit;
            v.str_val = val;
            return v;
        }
    };

    // ========================================================================
    // Expression Evaluation
    // ========================================================================

    // Forward declaration
    std::optional<const_value> evaluate_expr(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        std::set<const ast::constant_def*>& evaluation_stack);

    // Evaluate binary expression
    std::optional<const_value> evaluate_binary(
        const ast::binary_expr& binary,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        std::set<const ast::constant_def*>& evaluation_stack)
    {
        auto left = evaluate_expr(*binary.left, analyzed, diags, evaluation_stack);
        auto right = evaluate_expr(*binary.right, analyzed, diags, evaluation_stack);

        if (!left || !right) {
            return std::nullopt;
        }

        // Arithmetic and bitwise operators
        if (left->type == const_value::kind::integer &&
            right->type == const_value::kind::integer) {

            int64_t l = left->int_val;
            int64_t r = right->int_val;

            switch (binary.op) {
                case ast::binary_op::add: {
                    // Check for overflow
                    if ((r > 0 && l > std::numeric_limits<int64_t>::max() - r) ||
                        (r < 0 && l < std::numeric_limits<int64_t>::min() - r)) {
                        add_error(diags, diag_codes::E_OVERFLOW,
                            "Integer overflow in addition", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l + r);
                }
                case ast::binary_op::sub: {
                    // Check for overflow
                    if ((r < 0 && l > std::numeric_limits<int64_t>::max() + r) ||
                        (r > 0 && l < std::numeric_limits<int64_t>::min() + r)) {
                        add_error(diags, diag_codes::E_UNDERFLOW,
                            "Integer underflow in subtraction", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l - r);
                }
                case ast::binary_op::mul: {
                    // Check for overflow (simplified)
                    if (r != 0 && std::abs(l) > std::numeric_limits<int64_t>::max() / std::abs(r)) {
                        add_error(diags, diag_codes::E_OVERFLOW,
                            "Integer overflow in multiplication", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l * r);
                }
                case ast::binary_op::div: {
                    if (r == 0) {
                        add_error(diags, diag_codes::E_DIVISION_BY_ZERO,
                            "Division by zero", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l / r);
                }
                case ast::binary_op::mod: {
                    if (r == 0) {
                        add_error(diags, diag_codes::E_DIVISION_BY_ZERO,
                            "Modulo by zero", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l % r);
                }
                case ast::binary_op::bit_and:
                    return const_value::make_int(l & r);
                case ast::binary_op::bit_or:
                    return const_value::make_int(l | r);
                case ast::binary_op::bit_xor:
                    return const_value::make_int(l ^ r);
                case ast::binary_op::lshift:
                    if (r < 0 || r >= 64) {
                        add_error(diags, diag_codes::E_OVERFLOW,
                            "Shift amount out of range", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l << r);
                case ast::binary_op::rshift:
                    if (r < 0 || r >= 64) {
                        add_error(diags, diag_codes::E_OVERFLOW,
                            "Shift amount out of range", binary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(l >> r);

                // Comparison operators (return boolean)
                case ast::binary_op::eq:
                    return const_value::make_bool(l == r);
                case ast::binary_op::ne:
                    return const_value::make_bool(l != r);
                case ast::binary_op::lt:
                    return const_value::make_bool(l < r);
                case ast::binary_op::gt:
                    return const_value::make_bool(l > r);
                case ast::binary_op::le:
                    return const_value::make_bool(l <= r);
                case ast::binary_op::ge:
                    return const_value::make_bool(l >= r);

                default:
                    break;
            }
        }

        // Boolean operators
        if (left->type == const_value::kind::boolean &&
            right->type == const_value::kind::boolean) {

            bool l = left->bool_val;
            bool r = right->bool_val;

            switch (binary.op) {
                case ast::binary_op::log_and:
                    return const_value::make_bool(l && r);
                case ast::binary_op::log_or:
                    return const_value::make_bool(l || r);
                case ast::binary_op::eq:
                    return const_value::make_bool(l == r);
                case ast::binary_op::ne:
                    return const_value::make_bool(l != r);
                default:
                    break;
            }
        }

        // String comparisons
        if (left->type == const_value::kind::string_lit &&
            right->type == const_value::kind::string_lit) {

            switch (binary.op) {
                case ast::binary_op::eq:
                    return const_value::make_bool(left->str_val == right->str_val);
                case ast::binary_op::ne:
                    return const_value::make_bool(left->str_val != right->str_val);
                default:
                    break;
            }
        }

        return std::nullopt;
    }

    // Evaluate unary expression
    std::optional<const_value> evaluate_unary(
        const ast::unary_expr& unary,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        std::set<const ast::constant_def*>& evaluation_stack)
    {
        auto operand = evaluate_expr(*unary.operand, analyzed, diags, evaluation_stack);
        if (!operand) {
            return std::nullopt;
        }

        if (operand->type == const_value::kind::integer) {
            int64_t val = operand->int_val;

            switch (unary.op) {
                case ast::unary_op::neg:
                    if (val == std::numeric_limits<int64_t>::min()) {
                        add_error(diags, diag_codes::E_OVERFLOW,
                            "Integer overflow in negation", unary.pos);
                        return std::nullopt;
                    }
                    return const_value::make_int(-val);
                case ast::unary_op::pos:
                    return const_value::make_int(val);
                case ast::unary_op::bit_not:
                    return const_value::make_int(~val);
                default:
                    break;
            }
        }

        if (operand->type == const_value::kind::boolean) {
            if (unary.op == ast::unary_op::log_not) {
                return const_value::make_bool(!operand->bool_val);
            }
        }

        return std::nullopt;
    }

    // Evaluate ternary expression
    std::optional<const_value> evaluate_ternary(
        const ast::ternary_expr& ternary,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        std::set<const ast::constant_def*>& evaluation_stack)
    {
        auto cond = evaluate_expr(*ternary.condition, analyzed, diags, evaluation_stack);
        if (!cond || cond->type != const_value::kind::boolean) {
            return std::nullopt;
        }

        if (cond->bool_val) {
            return evaluate_expr(*ternary.true_expr, analyzed, diags, evaluation_stack);
        } else {
            return evaluate_expr(*ternary.false_expr, analyzed, diags, evaluation_stack);
        }
    }

    // Main expression evaluation function
    std::optional<const_value> evaluate_expr(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        std::set<const ast::constant_def*>& evaluation_stack)
    {
        // Literals
        if (auto* int_lit = std::get_if<ast::literal_int>(&expr.node)) {
            return const_value::make_int(static_cast<int64_t>(int_lit->value));
        }
        else if (auto* bool_lit = std::get_if<ast::literal_bool>(&expr.node)) {
            return const_value::make_bool(bool_lit->value);
        }
        else if (auto* str_lit = std::get_if<ast::literal_string>(&expr.node)) {
            return const_value::make_string(str_lit->value);
        }
        // Identifier - look up constant
        else if (auto* id = std::get_if<ast::identifier>(&expr.node)) {
            if (auto* const_def = analyzed.symbols.find_constant(id->name)) {
                // Check for circular dependency
                if (evaluation_stack.contains(const_def)) {
                    add_error(diags, diag_codes::E_CIRCULAR_CONSTANT,
                        "Circular constant dependency for '" + id->name + "'",
                        id->pos);
                    return std::nullopt;
                }

                // Check if already evaluated
                auto it = analyzed.constant_values.find(const_def);
                if (it != analyzed.constant_values.end()) {
                    return const_value::make_int(static_cast<int64_t>(it->second));
                }

                // Evaluate recursively
                evaluation_stack.insert(const_def);
                auto result = evaluate_expr(const_def->value, analyzed, diags, evaluation_stack);
                evaluation_stack.erase(const_def);
                return result;
            }
            // Unknown identifier - error already reported in Phase 2
            return std::nullopt;
        }
        // Operators
        else if (auto* binary = std::get_if<ast::binary_expr>(&expr.node)) {
            return evaluate_binary(*binary, analyzed, diags, evaluation_stack);
        }
        else if (auto* unary = std::get_if<ast::unary_expr>(&expr.node)) {
            return evaluate_unary(*unary, analyzed, diags, evaluation_stack);
        }
        else if (auto* ternary = std::get_if<ast::ternary_expr>(&expr.node)) {
            return evaluate_ternary(*ternary, analyzed, diags, evaluation_stack);
        }

        // Other expression types can't be evaluated at compile time
        return std::nullopt;
    }

    // ========================================================================
    // Alignment Directive Validation
    // ========================================================================

    void validate_alignment_directives(
        const ast::module& mod,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Helper: validate alignment in struct body
        auto validate_struct_body = [&](const std::vector<ast::struct_body_item>& body) {
            for (const auto& item : body) {
                if (const auto* align_dir = std::get_if<ast::alignment_directive>(&item)) {
                    // Try to evaluate alignment expression
                    std::set<const ast::constant_def*> evaluation_stack;
                    auto value = evaluate_expr(align_dir->alignment_expr, analyzed, diags, evaluation_stack);

                    // Report error if not a constant integer
                    if (!value || value->type != const_value::kind::integer) {
                        add_error(
                            diags,
                            "E4001",
                            "Alignment expression must be a constant integer literal",
                            align_dir->pos
                        );
                    }
                }
            }
        };

        // Check structs
        for (const auto& struct_def : mod.structs) {
            validate_struct_body(struct_def.body);
        }

        // Check unions (alignment directives in union case bodies)
        for (const auto& union_def : mod.unions) {
            for (const auto& union_case : union_def.cases) {
                for (const auto& item : union_case.items) {
                    if (const auto* align_dir = std::get_if<ast::alignment_directive>(&item)) {
                        std::set<const ast::constant_def*> evaluation_stack;
                        auto value = evaluate_expr(align_dir->alignment_expr, analyzed, diags, evaluation_stack);

                        if (!value || value->type != const_value::kind::integer) {
                            add_error(
                                diags,
                                "E4001",
                                "Alignment expression must be a constant integer literal",
                                align_dir->pos
                            );
                        }
                    }
                }
            }
        }

        // Note: Choices don't support alignment directives in their case fields,
        // so no validation needed for choices.
    }

    // ========================================================================
    // Module-Level Constant Evaluation
    // ========================================================================

    void evaluate_module_constants(
        const ast::module& mod,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Evaluate all constants in the module
        for (const auto& const_def : mod.constants) {
            // Skip if already evaluated (from circular dependency check)
            if (analyzed.constant_values.contains(&const_def)) {
                continue;
            }

            std::set<const ast::constant_def*> evaluation_stack;
            evaluation_stack.insert(&const_def);

            auto value = evaluate_expr(const_def.value, analyzed, diags, evaluation_stack);

            if (value && value->type == const_value::kind::integer) {
                // Store as uint64_t (sign-extension handled by cast)
                analyzed.constant_values[&const_def] = static_cast<uint64_t>(value->int_val);
            }
            // Boolean and string constants not stored (only integers needed for sizes)
        }

        // Evaluate enum item values
        for (const auto& enum_def : mod.enums) {
            int64_t next_value = 0;

            for (const auto& item : enum_def.items) {
                int64_t current_value = next_value;

                if (item.value) {
                    // Explicit value
                    std::set<const ast::constant_def*> evaluation_stack;
                    auto value = evaluate_expr(item.value.value(), analyzed, diags, evaluation_stack);

                    if (value && value->type == const_value::kind::integer) {
                        current_value = value->int_val;
                    }
                }

                // Store the value for this enum item
                analyzed.enum_item_values[&item] = static_cast<uint64_t>(current_value);

                // Next item gets current + 1 (unless it has explicit value)
                next_value = current_value + 1;
            }
        }
    }

// Validate inline choice discriminator types (explicit type required)
static void validate_choice_discriminator_types(
    const ast::module& mod,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    for (const auto& choice : mod.choices) {
        // Skip external discriminator choices
        if (choice.selector.has_value()) {
            continue;
        }

        // Inline discriminator requires explicit type
        if (!choice.inline_discriminator_type.has_value()) {
            diagnostics.push_back(diagnostic{
                diagnostic_level::error,
                "",  // code
                "Inline choice '" + choice.name + "' requires explicit discriminator type (e.g., 'choice " + choice.name + " : uint16 { ... }')",
                choice.pos
            });
            continue;
        }

        // Extract discriminator type and determine its range
        const auto& disc_type = choice.inline_discriminator_type.value();
        const auto& disc_type_variant = disc_type.node;

        // Only primitive types are allowed as discriminators
        if (!std::holds_alternative<ast::primitive_type>(disc_type_variant)) {
            diagnostics.push_back(diagnostic{
                diagnostic_level::error,
                "",  // code
                "Inline choice '" + choice.name + "' discriminator must be a primitive integer type (uint8, uint16, uint32, or uint64)",
                choice.pos
            });
            continue;
        }

        const auto& prim_type = std::get<ast::primitive_type>(disc_type_variant);

        // Only unsigned integer types are allowed
        if (prim_type.is_signed || prim_type.bits == 1) {
            diagnostics.push_back(diagnostic{
                diagnostic_level::error,
                "",  // code
                "Inline choice '" + choice.name + "' discriminator must be an unsigned integer type (uint8, uint16, uint32, or uint64)",
                choice.pos
            });
            continue;
        }

        // Determine maximum value for this type
        uint64_t max_allowed = UINT64_MAX;
        if (prim_type.bits == 8) {
            max_allowed = UINT8_MAX;
        } else if (prim_type.bits == 16) {
            max_allowed = UINT16_MAX;
        } else if (prim_type.bits == 32) {
            max_allowed = UINT32_MAX;
        } else if (prim_type.bits != 64) {
            diagnostics.push_back(diagnostic{
                diagnostic_level::error,
                "",  // code
                "Inline choice '" + choice.name + "' discriminator must be uint8, uint16, uint32, or uint64",
                choice.pos
            });
            continue;
        }

        // Validate all case values fit within discriminator type range
        for (const auto& case_def : choice.cases) {
            for (const auto& case_expr : case_def.case_exprs) {
                std::set<const ast::constant_def*> eval_stack;
                auto value = evaluate_expr(case_expr, analyzed, diagnostics, eval_stack);

                if (value && value->type == const_value::kind::integer) {
                    uint64_t case_val = static_cast<uint64_t>(value->int_val);
                    if (case_val > max_allowed) {
                        diagnostics.push_back(diagnostic{
                            diagnostic_level::error,
                            "",  // code
                            "Case value " + std::to_string(case_val) + " exceeds maximum value " +
                            std::to_string(max_allowed) + " for discriminator type uint" + std::to_string(prim_type.bits),
                            choice.pos
                        });
                    }
                }
            }
        }

        // Store the explicit discriminator type
        analyzed.choice_discriminator_types.emplace(&choice, prim_type);
    }
}

} // anonymous namespace

// ============================================================================
// Public API: Constant Evaluation
// ============================================================================

void evaluate_constants(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Evaluate constants in main module
    evaluate_module_constants(modules.main.module, analyzed, diagnostics);

    // Evaluate constants in all imported modules
    for (const auto& imported : modules.imported) {
        evaluate_module_constants(imported.module, analyzed, diagnostics);
    }

    // Validate alignment directives (must be constant expressions)
    validate_alignment_directives(modules.main.module, analyzed, diagnostics);
    for (const auto& imported : modules.imported) {
        validate_alignment_directives(imported.module, analyzed, diagnostics);
    }

    // Validate discriminator types for inline choices (explicit type required)
    validate_choice_discriminator_types(modules.main.module, analyzed, diagnostics);
    for (const auto& imported : modules.imported) {
        validate_choice_discriminator_types(imported.module, analyzed, diagnostics);
    }
}

std::optional<uint64_t> evaluate_constant_uint(
    const ast::expr& expr,
    const analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Use empty evaluation stack since we're evaluating an expression,
    // not a constant definition (no circular dependency tracking needed)
    std::set<const ast::constant_def*> evaluation_stack;

    // Evaluate the expression
    auto value = evaluate_expr(expr, analyzed, diagnostics, evaluation_stack);

    // Return uint64 value if it's an integer
    if (value && value->type == const_value::kind::integer) {
        return static_cast<uint64_t>(value->int_val);
    }

    // Not a compile-time integer constant
    return std::nullopt;
}

} // namespace datascript::semantic::phases

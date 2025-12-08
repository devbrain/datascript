//
// Phase 6: Constraint Validation
//
// Validates user-defined constraints and detects issues like:
// - Invalid constraint expressions
// - Always-true or always-false constraints
// - Constraint parameter type mismatches
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

    // Helper: add warning diagnostic
    void add_warning(std::vector<diagnostic>& diags,
                    const char* code,
                    const std::string& message,
                    const ast::source_pos& pos) {
        diags.push_back(diagnostic{
            diagnostic_level::warning,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });
    }

    // Helper: add error with related location
    void add_error_related(std::vector<diagnostic>& diags,
                          const char* code,
                          const std::string& message,
                          const ast::source_pos& pos,
                          const ast::source_pos& related_pos,
                          const std::string& related_msg) {
        diags.push_back(diagnostic{
            diagnostic_level::error,
            code,
            message,
            pos,
            related_pos,
            related_msg,
            std::nullopt
        });
    }

    // ========================================================================
    // Forward Declarations
    // ========================================================================

    std::string operator_name(ast::binary_op op);
    void detect_tautologies_and_contradictions(
        const ast::constraint_def& constraint,
        std::vector<diagnostic>& diags);

    // ========================================================================
    // Constraint Expression Analysis
    // ========================================================================

    // Check if constraint condition is a literal boolean
    std::optional<bool> is_literal_bool(const ast::expr& expr) {
        if (auto* lit = std::get_if<ast::literal_bool>(&expr.node)) {
            return lit->value;
        }
        return std::nullopt;
    }

    // Validate constraint condition
    void validate_constraint_condition(
        const ast::expr& condition,
        const ast::constraint_def& constraint,
        std::vector<diagnostic>& diags)
    {
        // Check for always-true constraint
        auto literal_val = is_literal_bool(condition);
        if (literal_val.has_value()) {
            if (*literal_val) {
                add_warning(diags, diag_codes::W_DEPRECATED,
                    "Constraint '" + constraint.name + "' is always true and has no effect",
                    constraint.pos);
            } else {
                add_error(diags, diag_codes::E_CONSTRAINT_VIOLATION,
                    "Constraint '" + constraint.name + "' is always false",
                    constraint.pos);
            }
        }

        // Detect more complex always-true/false patterns
        detect_tautologies_and_contradictions(constraint, diags);
    }

    // Detect tautologies (always true) and contradictions (always false)
    void detect_tautologies_and_contradictions(
        const ast::constraint_def& constraint,
        std::vector<diagnostic>& diags)
    {
        // Check for patterns like: x == x (always true), x != x (always false)
        if (auto* bin = std::get_if<ast::binary_expr>(&constraint.condition.node)) {
            // Check if both sides are the same identifier
            auto* left_id = std::get_if<ast::identifier>(&bin->left->node);
            auto* right_id = std::get_if<ast::identifier>(&bin->right->node);

            if (left_id && right_id && left_id->name == right_id->name) {
                // Same identifier on both sides
                switch (bin->op) {
                    case ast::binary_op::eq:
                    case ast::binary_op::le:
                    case ast::binary_op::ge:
                        add_warning(diags, diag_codes::W_DEPRECATED,
                            "Constraint '" + constraint.name + "' is always true (comparing '" +
                            left_id->name + "' with itself using " + operator_name(bin->op) + ")",
                            constraint.pos);
                        break;

                    case ast::binary_op::ne:
                    case ast::binary_op::lt:
                    case ast::binary_op::gt:
                        add_warning(diags, diag_codes::W_DEPRECATED,
                            "Constraint '" + constraint.name + "' is always false (comparing '" +
                            left_id->name + "' with itself using " + operator_name(bin->op) + ")",
                            constraint.pos);
                        break;

                    default:
                        break;
                }
            }

            // Check for contradictions: x > 10 && x < 5 (likely impossible)
            // This requires more sophisticated analysis - left for future enhancement
        }
    }

    // Helper: Get operator name for diagnostics
    std::string operator_name(ast::binary_op op) {
        switch (op) {
            case ast::binary_op::eq: return "==";
            case ast::binary_op::ne: return "!=";
            case ast::binary_op::lt: return "<";
            case ast::binary_op::gt: return ">";
            case ast::binary_op::le: return "<=";
            case ast::binary_op::ge: return ">=";
            default: return "operator";
        }
    }

    // ========================================================================
    // Field Constraint Validation
    // ========================================================================

    // Validate field conditions in structs
    void validate_struct_field_conditions(
        const ast::struct_def& struct_def,
        std::vector<diagnostic>& diags)
    {
        for (const auto& body_item : struct_def.body) {
            // Only process fields
            if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                if (field->condition) {
                    // Check for literal false (unreachable field)
                    auto literal_val = is_literal_bool(field->condition.value());
                    if (literal_val.has_value() && !*literal_val) {
                        add_warning(diags, diag_codes::W_DEPRECATED,
                            "Field '" + field->name + "' has condition that is always false",
                            field->pos);
                    }
                }
            }
        }
    }

    // Validate field conditions in unions
    void validate_union_field_conditions(
        const ast::union_def& union_def,
        std::vector<diagnostic>& diags)
    {
        for (const auto& union_case : union_def.cases) {
            // Check case condition
            if (union_case.condition) {
                auto literal_val = is_literal_bool(union_case.condition.value());
                if (literal_val.has_value() && !*literal_val) {
                    add_warning(diags, diag_codes::W_DEPRECATED,
                        "Union case '" + union_case.case_name + "' has condition that is always false",
                        union_case.pos);
                }
            }

            // Check field conditions
            for (const auto& item : union_case.items) {
                if (std::holds_alternative<ast::field_def>(item)) {
                    const auto& field = std::get<ast::field_def>(item);
                    if (field.condition) {
                        // Check for literal false (unreachable field)
                        auto literal_val = is_literal_bool(field.condition.value());
                        if (literal_val.has_value() && !*literal_val) {
                            add_warning(diags, diag_codes::W_DEPRECATED,
                                "Union field '" + field.name + "' has condition that is always false",
                                field.pos);
                        }
                    }
                }
            }
        }
    }

    // ========================================================================
    // Choice Validation
    // ========================================================================

    void validate_choice(
        const ast::choice_def& choice_def,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Check for duplicate case values (if they're compile-time constants)
        std::map<uint64_t, const ast::choice_case*> case_values;

        for (const auto& case_def : choice_def.cases) {
            // Skip default case (empty case_exprs)
            if (case_def.is_default || case_def.case_exprs.empty()) {
                continue;
            }

            // Evaluate each case expression
            for (const auto& case_expr : case_def.case_exprs) {
                // Try to evaluate the case expression as a compile-time constant
                std::vector<diagnostic> temp_diags;
                auto const_val = phases::evaluate_constant_uint(case_expr, analyzed, temp_diags);

                if (const_val) {
                    // Check for duplicate
                    auto it = case_values.find(*const_val);
                    if (it != case_values.end()) {
                        // Found duplicate - report with related location
                        add_error_related(diags,
                            diag_codes::E_DUPLICATE_DEFINITION,
                            "Duplicate choice case value: " + std::to_string(*const_val),
                            case_def.pos,
                            it->second->pos,
                            "Previous case with same value here");
                    } else {
                        case_values[*const_val] = &case_def;
                    }
                }
            }

            // Validate case field conditions (after desugaring, items[0] contains the field_def)
            if (!case_def.items.empty() && std::holds_alternative<ast::field_def>(case_def.items[0])) {
                const auto& field = std::get<ast::field_def>(case_def.items[0]);
                if (field.condition) {
                    auto literal_val = is_literal_bool(field.condition.value());
                    if (literal_val.has_value() && !*literal_val) {
                        add_warning(diags, diag_codes::W_DEPRECATED,
                            "Choice case field has condition that is always false (field will never be read)",
                            field.pos);
                    }
                }
            }
        }
    }

    // ========================================================================
    // Module-Level Constraint Validation
    // ========================================================================

    void validate_module_constraints(
        const ast::module& mod,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Validate explicit constraint definitions
        for (const auto& constraint : mod.constraints) {
            validate_constraint_condition(constraint.condition, constraint, diags);
        }

        // Validate struct field conditions
        for (const auto& struct_def : mod.structs) {
            validate_struct_field_conditions(struct_def, diags);
        }

        // Validate union field conditions
        for (const auto& union_def : mod.unions) {
            validate_union_field_conditions(union_def, diags);
        }

        // Validate choices
        for (const auto& choice_def : mod.choices) {
            validate_choice(choice_def, analyzed, diags);
        }
    }

} // anonymous namespace

// ============================================================================
// Public API: Constraint Validation
// ============================================================================

void validate_constraints(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Validate constraints in main module
    validate_module_constraints(modules.main.module, analyzed, diagnostics);

    // Validate constraints in all imported modules
    for (const auto& imported : modules.imported) {
        validate_module_constraints(imported.module, analyzed, diagnostics);
    }
}

} // namespace datascript::semantic::phases

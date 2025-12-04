//
// Phase 2: Name Resolution with Java-Style Scoping
//
// Resolves all type references and identifiers to their definitions.
// Uses symbol tables built in Phase 1 for lookup.
//

#include <datascript/semantic.hh>
#include <algorithm>

namespace datascript::semantic::phases {

namespace {
    // Helper: join vector of strings with separator
    std::string join(const std::vector<std::string>& parts, const std::string& sep) {
        if (parts.empty()) return "";

        std::string result = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            result += sep + parts[i];
        }
        return result;
    }

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

    // Helper: add error with suggestion
    void add_error_suggest(std::vector<diagnostic>& diags,
                          const char* code,
                          const std::string& message,
                          const ast::source_pos& pos,
                          const std::string& suggestion) {
        diags.push_back(diagnostic{
            diagnostic_level::error,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            suggestion
        });
    }

    // ========================================================================
    // Type Resolution
    // ========================================================================

    // Resolve a qualified_name to its definition
    // Returns optional - nullopt if not found (error already reported)
    std::optional<analyzed_module_set::resolved_type> resolve_qualified_name(
        const ast::qualified_name& qname,
        const symbol_table& symbols,
        std::vector<diagnostic>& diags)
    {
        // Try each type kind in order

        // 1. Try struct
        if (auto def = symbols.find_struct_qualified(qname.parts)) {
            return analyzed_module_set::resolved_type{def};
        }

        // 2. Try union
        if (auto def = symbols.find_union_qualified(qname.parts)) {
            return analyzed_module_set::resolved_type{def};
        }

        // 3. Try enum
        if (auto def = symbols.find_enum_qualified(qname.parts)) {
            return analyzed_module_set::resolved_type{def};
        }

        // 4. Try subtype
        if (auto def = symbols.find_subtype_qualified(qname.parts)) {
            return analyzed_module_set::resolved_type{def};
        }

        // 5. Try choice
        if (auto def = symbols.find_choice_qualified(qname.parts)) {
            return analyzed_module_set::resolved_type{def};
        }

        // Not found - report error
        std::string full_name = join(qname.parts, ".");
        add_error_suggest(diags,
            diag_codes::E_UNDEFINED_TYPE,
            "Type '" + full_name + "' not found",
            qname.pos,
            "Check spelling and imports");

        // Return nullopt - error already reported
        return std::nullopt;
    }

    // Forward declaration
    void resolve_type(
        const ast::type& type_node,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags);

    // Resolve array element type (recursive)
    void resolve_array_type(
        const ast::type& element_type,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        resolve_type(element_type, analyzed, diags);
    }

    // Main type resolution function
    void resolve_type(
        const ast::type& type_node,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // qualified_name is the only type that needs resolution
        if (auto* qname = std::get_if<ast::qualified_name>(&type_node.node)) {
            // Resolve to actual definition
            auto resolved = resolve_qualified_name(*qname, analyzed.symbols, diags);

            // Store resolution only if successful
            if (resolved.has_value()) {
                analyzed.resolved_types[qname] = resolved.value();
            }
        }
        // Array types: recursively resolve element type
        else if (auto* arr = std::get_if<ast::array_type_fixed>(&type_node.node)) {
            resolve_array_type(*arr->element_type, analyzed, diags);
        }
        else if (auto* arr = std::get_if<ast::array_type_range>(&type_node.node)) {
            resolve_array_type(*arr->element_type, analyzed, diags);
        }
        else if (auto* arr = std::get_if<ast::array_type_unsized>(&type_node.node)) {
            resolve_array_type(*arr->element_type, analyzed, diags);
        }
        // Primitive types, string, bool, bitfield - no resolution needed
    }

    // ========================================================================
    // Expression Resolution (for identifiers that reference constants)
    // ========================================================================

    // Forward declaration
    void resolve_expr(
        const ast::expr& expr_node,
        const symbol_table& symbols,
        std::vector<diagnostic>& diags);

    void resolve_expr(
        const ast::expr& expr_node,
        const symbol_table& symbols,
        std::vector<diagnostic>& diags)
    {
        // Resolve based on expression type
        if (auto* id = std::get_if<ast::identifier>(&expr_node.node)) {
            // Try to resolve identifier to a constant
            // (In full implementation, also check parameters in scope)
            auto const_def = symbols.find_constant(id->name);
            if (!const_def) {
                // For now, we allow undefined identifiers in expressions
                // (they might be parameters, field references, etc.)
                // Full resolution requires scope tracking
            }
        }
        else if (auto* unary = std::get_if<ast::unary_expr>(&expr_node.node)) {
            resolve_expr(*unary->operand, symbols, diags);
        }
        else if (auto* binary = std::get_if<ast::binary_expr>(&expr_node.node)) {
            resolve_expr(*binary->left, symbols, diags);
            resolve_expr(*binary->right, symbols, diags);
        }
        else if (auto* ternary = std::get_if<ast::ternary_expr>(&expr_node.node)) {
            resolve_expr(*ternary->condition, symbols, diags);
            resolve_expr(*ternary->true_expr, symbols, diags);
            resolve_expr(*ternary->false_expr, symbols, diags);
        }
        else if (auto* field_access = std::get_if<ast::field_access_expr>(&expr_node.node)) {
            resolve_expr(*field_access->object, symbols, diags);
        }
        else if (auto* array_index = std::get_if<ast::array_index_expr>(&expr_node.node)) {
            resolve_expr(*array_index->array, symbols, diags);
            resolve_expr(*array_index->index, symbols, diags);
        }
        else if (auto* func_call = std::get_if<ast::function_call_expr>(&expr_node.node)) {
            resolve_expr(*func_call->function, symbols, diags);
            for (const auto& arg : func_call->arguments) {
                resolve_expr(arg, symbols, diags);
            }
        }
        // Literals don't need resolution
    }

    // ========================================================================
    // Resolve names in a single module
    // ========================================================================

    void resolve_module_names(
        const ast::module& mod,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Resolve type references in constants
        for (const auto& const_def : mod.constants) {
            resolve_type(const_def.ctype, analyzed, diags);
            resolve_expr(const_def.value, analyzed.symbols, diags);
        }

        // Resolve field types and functions in structs
        for (const auto& struct_def : mod.structs) {
            for (const auto& body_item : struct_def.body) {
                // Process fields
                if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                    resolve_type(field->field_type, analyzed, diags);

                    // Resolve condition expression if present
                    if (field->condition) {
                        resolve_expr(field->condition.value(), analyzed.symbols, diags);
                    }
                }
                // Process functions
                else if (auto* func = std::get_if<ast::function_def>(&body_item)) {
                    // Resolve return type
                    resolve_type(func->return_type, analyzed, diags);

                    // Resolve parameter types
                    for (const auto& param : func->parameters) {
                        resolve_type(param.param_type, analyzed, diags);
                    }

                    // Resolve expressions in function body
                    for (const auto& stmt : func->body) {
                        if (auto* ret_stmt = std::get_if<ast::return_statement>(&stmt)) {
                            resolve_expr(ret_stmt->value, analyzed.symbols, diags);
                        } else if (auto* expr_stmt = std::get_if<ast::expression_statement>(&stmt)) {
                            resolve_expr(expr_stmt->expression, analyzed.symbols, diags);
                        }
                    }
                }
            }
        }

        // Resolve field types in unions
        for (const auto& union_def : mod.unions) {
            for (const auto& union_case : union_def.cases) {
                // Resolve each item in the case
                for (const auto& item : union_case.items) {
                    // After desugaring, union cases should only contain field_def
                    // (inline types are converted to named types in phase 0)
                    if (std::holds_alternative<ast::field_def>(item)) {
                        const auto& field = std::get<ast::field_def>(item);
                        resolve_type(field.field_type, analyzed, diags);

                        if (field.condition) {
                            resolve_expr(field.condition.value(), analyzed.symbols, diags);
                        }
                    }
                    // Note: labels, alignments, functions, inline types shouldn't appear here
                    // after desugaring, but we silently skip them if they do
                }

                // Resolve case condition if present
                if (union_case.condition) {
                    resolve_expr(union_case.condition.value(), analyzed.symbols, diags);
                }
            }
        }

        // Resolve base types in enums
        for (const auto& enum_def : mod.enums) {
            resolve_type(enum_def.base_type, analyzed, diags);

            // Resolve enum item value expressions
            for (const auto& item : enum_def.items) {
                if (item.value) {
                    resolve_expr(item.value.value(), analyzed.symbols, diags);
                }
            }
        }

        // Resolve choice selectors and case types
        for (const auto& choice_def : mod.choices) {
            resolve_expr(choice_def.selector, analyzed.symbols, diags);

            for (const auto& case_def : choice_def.cases) {
                // Resolve case expressions
                for (const auto& case_expr : case_def.case_exprs) {
                    resolve_expr(case_expr, analyzed.symbols, diags);
                }

                // Resolve field type
                resolve_type(case_def.field.field_type, analyzed, diags);

                if (case_def.field.condition) {
                    resolve_expr(case_def.field.condition.value(), analyzed.symbols, diags);
                }
            }
        }

        // Resolve constraint parameters and conditions
        for (const auto& constraint_def : mod.constraints) {
            // Resolve parameter types
            for (const auto& param : constraint_def.params) {
                resolve_type(param.param_type, analyzed, diags);
            }

            // Resolve condition expression
            resolve_expr(constraint_def.condition, analyzed.symbols, diags);
        }
    }

} // anonymous namespace

// ============================================================================
// Public API: Name Resolution
// ============================================================================

void resolve_names(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Resolve names in main module
    resolve_module_names(modules.main.module, analyzed, diagnostics);

    // Resolve names in all imported modules
    for (const auto& imported : modules.imported) {
        resolve_module_names(imported.module, analyzed, diagnostics);
    }
}

} // namespace datascript::semantic::phases

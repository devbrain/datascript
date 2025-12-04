//
// Phase 7: Reachability Analysis
//
// Detects unused symbols and provides helpful warnings:
// - Unused constants
// - Unused types (structs, unions, enums, choices)
// - Unused imports
// - Unused constraints
//

#include <datascript/semantic.hh>
#include <algorithm>
#include <set>

namespace datascript::semantic::phases {

namespace {
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

    // ========================================================================
    // Symbol Usage Tracking
    // ========================================================================

    struct usage_tracker {
        std::set<const ast::constant_def*> used_constants;
        std::set<const ast::struct_def*> used_structs;
        std::set<const ast::union_def*> used_unions;
        std::set<const ast::enum_def*> used_enums;
        std::set<const ast::choice_def*> used_choices;
        std::set<const ast::constraint_def*> used_constraints;
        std::set<std::string> used_imports;  // Package names
    };

    // Forward declarations
    void track_expr_usage(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        usage_tracker& tracker);

    void track_type_usage(
        const ast::type& type_node,
        const analyzed_module_set& analyzed,
        usage_tracker& tracker);

    // Track expression usage (identifiers reference constants)
    void track_expr_usage(
        const ast::expr& expr,
        const analyzed_module_set& analyzed,
        usage_tracker& tracker)
    {
        if (auto* id = std::get_if<ast::identifier>(&expr.node)) {
            // Mark constant as used
            if (auto* const_def = analyzed.symbols.find_constant(id->name)) {
                tracker.used_constants.insert(const_def);
            }
        }
        else if (auto* binary = std::get_if<ast::binary_expr>(&expr.node)) {
            track_expr_usage(*binary->left, analyzed, tracker);
            track_expr_usage(*binary->right, analyzed, tracker);
        }
        else if (auto* unary = std::get_if<ast::unary_expr>(&expr.node)) {
            track_expr_usage(*unary->operand, analyzed, tracker);
        }
        else if (auto* ternary = std::get_if<ast::ternary_expr>(&expr.node)) {
            track_expr_usage(*ternary->condition, analyzed, tracker);
            track_expr_usage(*ternary->true_expr, analyzed, tracker);
            track_expr_usage(*ternary->false_expr, analyzed, tracker);
        }
        else if (auto* field_access = std::get_if<ast::field_access_expr>(&expr.node)) {
            track_expr_usage(*field_access->object, analyzed, tracker);
        }
        else if (auto* array_index = std::get_if<ast::array_index_expr>(&expr.node)) {
            track_expr_usage(*array_index->array, analyzed, tracker);
            track_expr_usage(*array_index->index, analyzed, tracker);
        }
        else if (auto* func_call = std::get_if<ast::function_call_expr>(&expr.node)) {
            track_expr_usage(*func_call->function, analyzed, tracker);
            for (const auto& arg : func_call->arguments) {
                track_expr_usage(arg, analyzed, tracker);
            }
        }
    }

    // Track type usage
    void track_type_usage(
        const ast::type& type_node,
        const analyzed_module_set& analyzed,
        usage_tracker& tracker)
    {
        // Track array element types
        if (auto* arr = std::get_if<ast::array_type_fixed>(&type_node.node)) {
            track_type_usage(*arr->element_type, analyzed, tracker);
            track_expr_usage(arr->size, analyzed, tracker);
        }
        else if (auto* arr = std::get_if<ast::array_type_range>(&type_node.node)) {
            track_type_usage(*arr->element_type, analyzed, tracker);
            if (arr->min_size) {
                track_expr_usage(arr->min_size.value(), analyzed, tracker);
            }
            track_expr_usage(arr->max_size, analyzed, tracker);
        }
        else if (auto* arr = std::get_if<ast::array_type_unsized>(&type_node.node)) {
            track_type_usage(*arr->element_type, analyzed, tracker);
        }
        // Track user-defined types
        else if (auto* qname = std::get_if<ast::qualified_name>(&type_node.node)) {
            // Look up resolved type
            auto it = analyzed.resolved_types.find(qname);
            if (it != analyzed.resolved_types.end()) {
                auto& resolved = it->second;

                if (auto* struct_def = std::get_if<const ast::struct_def*>(&resolved)) {
                    tracker.used_structs.insert(*struct_def);
                }
                else if (auto* union_def = std::get_if<const ast::union_def*>(&resolved)) {
                    tracker.used_unions.insert(*union_def);
                }
                else if (auto* enum_def = std::get_if<const ast::enum_def*>(&resolved)) {
                    tracker.used_enums.insert(*enum_def);
                }
                else if (auto* choice_def = std::get_if<const ast::choice_def*>(&resolved)) {
                    tracker.used_choices.insert(*choice_def);
                }

                // Track import usage (if qualified name has package)
                if (qname->parts.size() > 1) {
                    // First part is package name
                    tracker.used_imports.insert(qname->parts[0]);
                }
            }
        }
        // Track bitfield expression types
        else if (auto* bf = std::get_if<ast::bit_field_type_expr>(&type_node.node)) {
            track_expr_usage(bf->width_expr, analyzed, tracker);
        }
    }

    // Track usage in a module
    void track_module_usage(
        const ast::module& mod,
        const analyzed_module_set& analyzed,
        usage_tracker& tracker)
    {
        // Track constant value expressions (constants can reference other constants)
        for (const auto& const_def : mod.constants) {
            track_type_usage(const_def.ctype, analyzed, tracker);
            track_expr_usage(const_def.value, analyzed, tracker);
        }

        // Track struct field types
        for (const auto& struct_def : mod.structs) {
            for (const auto& body_item : struct_def.body) {
                // Only process fields
                if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                    track_type_usage(field->field_type, analyzed, tracker);
                    if (field->condition) {
                        track_expr_usage(field->condition.value(), analyzed, tracker);
                    }
                    if (field->constraint) {
                        track_expr_usage(field->constraint.value(), analyzed, tracker);
                    }
                }
            }
        }

        // Track union field types
        for (const auto& union_def : mod.unions) {
            for (const auto& union_case : union_def.cases) {
                for (const auto& item : union_case.items) {
                    if (std::holds_alternative<ast::field_def>(item)) {
                        const auto& field = std::get<ast::field_def>(item);
                        track_type_usage(field.field_type, analyzed, tracker);
                        if (field.condition) {
                            track_expr_usage(field.condition.value(), analyzed, tracker);
                        }
                        if (field.constraint) {
                            track_expr_usage(field.constraint.value(), analyzed, tracker);
                        }
                    }
                }
                if (union_case.condition) {
                    track_expr_usage(union_case.condition.value(), analyzed, tracker);
                }
            }
        }

        // Track enum base types and item values
        for (const auto& enum_def : mod.enums) {
            track_type_usage(enum_def.base_type, analyzed, tracker);
            for (const auto& item : enum_def.items) {
                if (item.value) {
                    track_expr_usage(item.value.value(), analyzed, tracker);
                }
            }
        }

        // Track choice selector and case types
        for (const auto& choice_def : mod.choices) {
            track_expr_usage(choice_def.selector, analyzed, tracker);
            for (const auto& case_def : choice_def.cases) {
                for (const auto& case_expr : case_def.case_exprs) {
                    track_expr_usage(case_expr, analyzed, tracker);
                }
                track_type_usage(case_def.field.field_type, analyzed, tracker);
                if (case_def.field.condition) {
                    track_expr_usage(case_def.field.condition.value(), analyzed, tracker);
                }
            }
        }

        // Track constraint parameters and conditions
        for (const auto& constraint_def : mod.constraints) {
            for (const auto& param : constraint_def.params) {
                track_type_usage(param.param_type, analyzed, tracker);
            }
            track_expr_usage(constraint_def.condition, analyzed, tracker);
        }
    }

    // ========================================================================
    // Unused Symbol Detection
    // ========================================================================

    void detect_unused_symbols(
        const ast::module& mod,
        const usage_tracker& tracker,
        std::vector<diagnostic>& diags)
    {
        // Check for unused constants
        for (const auto& const_def : mod.constants) {
            if (!tracker.used_constants.contains(&const_def)) {
                add_warning(diags, diag_codes::W_UNUSED_CONSTANT,
                    "Constant '" + const_def.name + "' is declared but never used",
                    const_def.pos);
            }
        }

        // NOTE: We don't warn about unused types (structs, unions, enums, choices)
        // because in a schema language, top-level types often serve as "root" or
        // "entry point" types that aren't referenced by other types.
        // Only warn about unused constants, which are more likely to be dead code.

        // Future enhancement: Could warn about types that are clearly internal
        // (e.g., only used once, or with names suggesting they're helpers)

        // Note: Constraints are not checked for usage - they're meant to be declarative
    }

    void detect_unused_imports(
        const ast::module& mod,
        const usage_tracker& tracker,
        std::vector<diagnostic>& diags)
    {
        for (const auto& import_decl : mod.imports) {
            // Get the imported package name (last part before wildcard)
            std::string package_name;
            if (!import_decl.name_parts.empty()) {
                if (import_decl.is_wildcard) {
                    // For "import foo.bar.*", package is "foo.bar"
                    package_name = import_decl.name_parts.back();
                } else {
                    // For "import foo.bar.Type", package is "foo.bar"
                    if (import_decl.name_parts.size() > 1) {
                        package_name = import_decl.name_parts[import_decl.name_parts.size() - 2];
                    }
                }

                if (!package_name.empty() && !tracker.used_imports.contains(package_name)) {
                    std::string import_str;
                    for (size_t i = 0; i < import_decl.name_parts.size(); ++i) {
                        if (i > 0) import_str += ".";
                        import_str += import_decl.name_parts[i];
                    }
                    if (import_decl.is_wildcard) {
                        import_str += ".*";
                    }

                    add_warning(diags, diag_codes::W_UNUSED_IMPORT,
                        "Import '" + import_str + "' is declared but never used",
                        import_decl.pos);
                }
            }
        }
    }

} // anonymous namespace

// ============================================================================
// Public API: Reachability Analysis
// ============================================================================

void analyze_reachability(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    usage_tracker tracker;

    // Track usage in main module
    track_module_usage(modules.main.module, analyzed, tracker);

    // Track usage in all imported modules
    for (const auto& imported : modules.imported) {
        track_module_usage(imported.module, analyzed, tracker);
    }

    // Detect unused symbols in main module
    detect_unused_symbols(modules.main.module, tracker, diagnostics);
    detect_unused_imports(modules.main.module, tracker, diagnostics);

    // Note: We don't warn about unused symbols in imported modules
    // (they might be used by other importers)
}

} // namespace datascript::semantic::phases

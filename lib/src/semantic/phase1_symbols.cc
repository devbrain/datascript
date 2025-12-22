//
// Phase 1: Symbol Collection Across Modules
//
// Collects all symbols from the main module and all imported modules,
// building a complete symbol table with Java-style scoping.
//

#include <datascript/semantic.hh>
#include <datascript/renderer_registry.hh>
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

    // Helper: get package name from module
    std::string get_package_name(const ast::module& mod) {
        if (mod.package) {
            return join(mod.package->name_parts, ".");
        }
        return "";  // Default package
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
    void add_error(std::vector<diagnostic>& diags,
                  const char* code,
                  const std::string& message,
                  const ast::source_pos& pos,
                  const std::optional<std::string>& suggestion) {
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

    // Helper: add warning with suggestion
    void add_warning(std::vector<diagnostic>& diags,
                    const char* code,
                    const std::string& message,
                    const ast::source_pos& pos,
                    const std::optional<std::string>& suggestion = std::nullopt) {
        diags.push_back(diagnostic{
            diagnostic_level::warning,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            suggestion
        });
    }

    // ========================================================================
    // Standardized Error Message Formatters
    // ========================================================================

    /// Format "already defined" error message
    /// Provides consistent phrasing across all duplicate definition errors
    std::string format_already_defined(const std::string& symbol_type, const std::string& name) {
        return symbol_type + " '" + name + "' is already defined in this module";
    }

    /// Format "not found" error message
    /// Provides consistent phrasing across all "not found" errors
    std::string format_not_found(const std::string& symbol_type, const std::string& name) {
        return symbol_type + " '" + name + "' not found";
    }

    // Helper: validate identifier against target language keywords
    void validate_identifier(const std::string& identifier,
                            const ast::source_pos& pos,
                            const std::string& symbol_type,
                            std::vector<diagnostic>& diags,
                            const std::set<std::string>& target_languages) {
        using namespace datascript::codegen;
        auto& registry = RendererRegistry::instance();

        // Determine which languages to check
        std::vector<std::string> languages_to_check;
        std::vector<std::string> unknown_languages;

        if (target_languages.empty()) {
            // Check all available languages
            languages_to_check = registry.get_available_languages();
        } else {
            // Check only specified target languages
            for (const auto& lang : target_languages) {
                if (registry.has_renderer(lang)) {
                    languages_to_check.push_back(lang);
                } else {
                    // Track unknown languages for error reporting
                    unknown_languages.push_back(lang);
                }
            }

            // Report error for unknown target languages
            if (!unknown_languages.empty()) {
                std::string unknown_list;
                if (unknown_languages.size() == 1) {
                    unknown_list = "'" + unknown_languages[0] + "'";
                } else if (unknown_languages.size() == 2) {
                    unknown_list = "'" + unknown_languages[0] + "' and '" + unknown_languages[1] + "'";
                } else {
                    for (size_t i = 0; i < unknown_languages.size() - 1; ++i) {
                        unknown_list += "'" + unknown_languages[i] + "', ";
                    }
                    unknown_list += "and '" + unknown_languages.back() + "'";
                }

                std::string message = "Unknown target language(s): " + unknown_list;

                // Provide list of available languages
                auto available = registry.get_available_languages();
                std::string suggestion = "Available languages: ";
                if (available.empty()) {
                    suggestion += "none (no renderers registered)";
                } else if (available.size() == 1) {
                    suggestion += "'" + available[0] + "'";
                } else {
                    for (size_t i = 0; i < available.size() - 1; ++i) {
                        suggestion += "'" + available[i] + "', ";
                    }
                    suggestion += "'" + available.back() + "'";
                }

                add_error(diags, diag_codes::E_UNKNOWN_TARGET_LANGUAGE, message, pos, suggestion);
            }
        }

        // Check if identifier conflicts with keywords in any target language
        std::vector<std::string> conflicts;
        for (const auto& lang : languages_to_check) {
            if (registry.is_keyword(lang, identifier)) {
                conflicts.push_back(lang);
            }
        }

        if (!conflicts.empty()) {
            // Build conflict message
            std::string conflict_list;
            if (conflicts.size() == 1) {
                conflict_list = conflicts[0];
            } else if (conflicts.size() == 2) {
                conflict_list = conflicts[0] + " and " + conflicts[1];
            } else {
                for (size_t i = 0; i < conflicts.size() - 1; ++i) {
                    conflict_list += conflicts[i] + ", ";
                }
                conflict_list += "and " + conflicts.back();
            }

            std::string message = symbol_type + " '" + identifier +
                                 "' conflicts with keyword in " + conflict_list;

            // Suggest sanitized version using the first conflicting language's sanitizer
            auto* renderer = registry.get_renderer(conflicts[0]);
            std::string sanitized = renderer ? renderer->sanitize_identifier(identifier) : identifier + "_";
            std::string suggestion = "Consider renaming to '" + sanitized + "' to avoid conflicts";

            add_warning(diags, diag_codes::W_KEYWORD_COLLISION, message, pos, suggestion);
        }
    }

    // ========================================================================
    // Collect symbols from a single module
    // ========================================================================

    void collect_module_symbols(
        const ast::module& mod,
        module_symbols& symbols,
        std::vector<diagnostic>& diags,
        const std::set<std::string>& target_languages = {})
    {
        // Collect constants
        for (const auto& const_def : mod.constants) {
            const std::string& name = const_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, const_def.pos, "Constant", diags, target_languages);

            if (symbols.constants.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Constant", name),
                    const_def.pos,
                    symbols.constants[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.constants[name] = &const_def;
        }

        // Collect structs
        for (const auto& struct_def : mod.structs) {
            const std::string& name = struct_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, struct_def.pos, "Struct", diags, target_languages);

            if (symbols.structs.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Struct", name),
                    struct_def.pos,
                    symbols.structs[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.structs[name] = &struct_def;

            // Validate field names within the struct (iterate through body items)
            for (const auto& item : struct_def.body) {
                if (std::holds_alternative<ast::field_def>(item)) {
                    const auto& field = std::get<ast::field_def>(item);
                    validate_identifier(field.name, field.pos, "Field", diags, target_languages);
                }
            }
        }

        // Collect unions
        for (const auto& union_def : mod.unions) {
            const std::string& name = union_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, union_def.pos, "Union", diags, target_languages);

            if (symbols.unions.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Union", name),
                    union_def.pos,
                    symbols.unions[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.unions[name] = &union_def;

            // Validate case names and field names within the union
            for (const auto& case_def : union_def.cases) {
                validate_identifier(case_def.case_name, case_def.pos, "Union case", diags, target_languages);

                // Validate fields within each case
                for (const auto& item : case_def.items) {
                    if (std::holds_alternative<ast::field_def>(item)) {
                        const auto& field = std::get<ast::field_def>(item);
                        validate_identifier(field.name, field.pos, "Field", diags, target_languages);
                    }
                }
            }
        }

        // Collect enums
        for (const auto& enum_def : mod.enums) {
            const std::string& name = enum_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, enum_def.pos, "Enum", diags, target_languages);

            if (symbols.enums.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Enum", name),
                    enum_def.pos,
                    symbols.enums[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.enums[name] = &enum_def;

            // Validate enum item names
            for (const auto& item : enum_def.items) {
                validate_identifier(item.name, item.pos, "Enum item", diags, target_languages);
            }
        }

        // Collect subtypes
        for (const auto& subtype_def : mod.subtypes) {
            const std::string& name = subtype_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, subtype_def.pos, "Subtype", diags, target_languages);

            if (symbols.subtypes.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Subtype", name),
                    subtype_def.pos,
                    symbols.subtypes[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.subtypes[name] = &subtype_def;
        }

        // Collect type aliases
        for (const auto& alias_def : mod.type_aliases) {
            const std::string& name = alias_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, alias_def.pos, "Type alias", diags, target_languages);

            if (symbols.type_aliases.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Type alias", name),
                    alias_def.pos,
                    symbols.type_aliases[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.type_aliases[name] = &alias_def;
        }

        // Collect choices
        for (const auto& choice_def : mod.choices) {
            const std::string& name = choice_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, choice_def.pos, "Choice", diags, target_languages);

            if (symbols.choices.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Choice", name),
                    choice_def.pos,
                    symbols.choices[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.choices[name] = &choice_def;

            // Validate field names within each choice case
            for (const auto& case_def : choice_def.cases) {
                // After desugaring, items[0] contains the field_def, validate its name
                if (!case_def.items.empty() && std::holds_alternative<ast::field_def>(case_def.items[0])) {
                    const auto& field = std::get<ast::field_def>(case_def.items[0]);
                    validate_identifier(field.name, field.pos, "Field", diags, target_languages);
                }
            }
        }

        // Collect constraints
        for (const auto& constraint_def : mod.constraints) {
            const std::string& name = constraint_def.name;

            // Validate identifier against target language keywords
            validate_identifier(name, constraint_def.pos, "Constraint", diags, target_languages);

            if (symbols.constraints.contains(name)) {
                add_error_related(diags,
                    diag_codes::E_DUPLICATE_DEFINITION,
                    format_already_defined("Constraint", name),
                    constraint_def.pos,
                    symbols.constraints[name]->pos,
                    "Previous definition here");
                continue;
            }

            symbols.constraints[name] = &constraint_def;
        }
    }

    // ========================================================================
    // Process wildcard imports (Java-style)
    // ========================================================================

    void process_wildcard_imports(
        const ast::module& mod,
        symbol_table& symbols,
        std::vector<diagnostic>& diags)
    {
        for (const auto& import : mod.imports) {
            if (!import.is_wildcard) continue;

            // Build package name from parts: ["foo", "bar"] -> "foo.bar"
            std::string pkg_name = join(import.name_parts, ".");

            // Find imported module
            auto it = symbols.imported.find(pkg_name);
            if (it == symbols.imported.end()) {
                // Module loader should have caught this, but report anyway
                add_error(diags,
                    diag_codes::E_UNDEFINED_PACKAGE,
                    format_not_found("Package", pkg_name) + " in imports",
                    import.pos);
                continue;
            }

            const module_symbols& imported_syms = it->second;

            // Add all symbols to wildcard tables (Java-style)

            // Structs
            for (const auto& [name, def] : imported_syms.structs) {
                if (symbols.wildcard_structs.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of struct '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_structs[name] = def;
            }

            // Unions
            for (const auto& [name, def] : imported_syms.unions) {
                if (symbols.wildcard_unions.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of union '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_unions[name] = def;
            }

            // Enums
            for (const auto& [name, def] : imported_syms.enums) {
                if (symbols.wildcard_enums.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of enum '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_enums[name] = def;
            }

            // Subtypes
            for (const auto& [name, def] : imported_syms.subtypes) {
                if (symbols.wildcard_subtypes.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of subtype '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_subtypes[name] = def;
            }

            // Type aliases
            for (const auto& [name, def] : imported_syms.type_aliases) {
                if (symbols.wildcard_type_aliases.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of type alias '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_type_aliases[name] = def;
            }

            // Choices
            for (const auto& [name, def] : imported_syms.choices) {
                if (symbols.wildcard_choices.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of choice '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_choices[name] = def;
            }

            // Constants
            for (const auto& [name, def] : imported_syms.constants) {
                if (symbols.wildcard_constants.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of constant '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_constants[name] = def;
            }

            // Constraints
            for (const auto& [name, def] : imported_syms.constraints) {
                if (symbols.wildcard_constraints.contains(name)) {
                    add_warning(diags,
                        diag_codes::W_WILDCARD_CONFLICT,
                        "Wildcard import of constraint '" + name +
                        "' conflicts with existing symbol",
                        import.pos,
                        "Use qualified name " + pkg_name + "." + name);
                    continue;
                }

                symbols.wildcard_constraints[name] = def;
            }
        }
    }

} // anonymous namespace

// ============================================================================
// Public API: Symbol Collection
// ============================================================================

symbol_table collect_symbols(
    const module_set& modules,
    std::vector<diagnostic>& diagnostics,
    const analysis_options& opts)
{
    symbol_table symbols;

    // Extract target languages for keyword validation
    const auto& target_languages = opts.target_languages;

    // 1. Collect symbols from main module
    std::string main_package = get_package_name(modules.main.module);

    module_symbols main_syms;
    main_syms.package_name = main_package;
    main_syms.source_module = &modules.main.module;

    collect_module_symbols(modules.main.module, main_syms, diagnostics, target_languages);
    symbols.main = std::move(main_syms);

    // 2. Collect symbols from all imported modules
    for (const auto& imported : modules.imported) {
        std::string pkg_name = get_package_name(imported.module);

        module_symbols mod_syms;
        mod_syms.package_name = pkg_name;
        mod_syms.source_module = &imported.module;

        collect_module_symbols(imported.module, mod_syms, diagnostics, target_languages);

        // Check for duplicate package definitions
        // (should be caught by module loader, but validate anyway)
        if (symbols.imported.contains(pkg_name)) {
            // Get the source position of the package declaration
            ast::source_pos pos{"<unknown>", 0, 0};
            if (imported.module.package) {
                pos = imported.module.package->pos;
            }

            ast::source_pos prev_pos{"<unknown>", 0, 0};
            if (symbols.imported[pkg_name].source_module->package) {
                prev_pos = symbols.imported[pkg_name].source_module->package->pos;
            }

            add_error_related(diagnostics,
                diag_codes::E_DUPLICATE_PACKAGE,
                "Package '" + pkg_name + "' defined multiple times",
                pos,
                prev_pos,
                "Previous definition here");
            continue;
        }

        symbols.imported[pkg_name] = std::move(mod_syms);
    }

    // 3. Process wildcard imports from main module (Java-style)
    process_wildcard_imports(modules.main.module, symbols, diagnostics);

    return symbols;
}

} // namespace datascript::semantic::phases

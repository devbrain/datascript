//
// Main semantic analysis pipeline
//

#include <datascript/semantic.hh>

namespace datascript::semantic {
    analysis_result analyze(module_set& modules, const analysis_options& opts) {
        analysis_result result;
        std::vector <diagnostic> diagnostics;

        // Phase 0: Desugar inline types (transforms inline unions/structs to named types)
        phases::desugar_inline_types(modules);

        // Phase 1: Symbol collection (with keyword validation)
        symbol_table symbols = phases::collect_symbols(modules, diagnostics, opts);

        // Check if we should stop (errors and stop_on_first_error)
        bool has_errors = std::any_of(diagnostics.begin(), diagnostics.end(),
                                      [](const auto& d) { return d.level == diagnostic_level::error; });

        if (has_errors && opts.stop_on_first_error) {
            result.diagnostics = std::move(diagnostics);
            return result;
        }

        // Create analyzed module set
        analyzed_module_set analyzed;
        analyzed.original = &modules;
        analyzed.symbols = std::move(symbols);

        // Phase 2: Name resolution
        phases::resolve_names(modules, analyzed, diagnostics);

        // Check for errors after name resolution
        has_errors = std::any_of(diagnostics.begin(), diagnostics.end(),
                                 [](const auto& d) { return d.level == diagnostic_level::error; });

        if (has_errors && opts.stop_on_first_error) {
            result.diagnostics = std::move(diagnostics);
            return result;
        }

        // Phase 3: Type checking
        phases::check_types(modules, analyzed, diagnostics);

        // Phase 4: Constant evaluation
        phases::evaluate_constants(modules, analyzed, diagnostics);

        // Phase 5: Size calculation
        phases::calculate_sizes(modules, analyzed, diagnostics);

        // Phase 6: Constraint validation
        phases::validate_constraints(modules, analyzed, diagnostics);

        // Phase 7: Reachability analysis
        phases::analyze_reachability(modules, analyzed, diagnostics);

        // Filter diagnostics by minimum level and disabled warnings
        std::vector <diagnostic> filtered_diags;
        for (const auto& diag : diagnostics) {
            // Skip if below minimum level
            if (diag.level > opts.min_level) {
                continue;
            }

            // Skip disabled warnings
            if (diag.level == diagnostic_level::warning &&
                opts.disabled_warnings.contains(diag.code)) {
                continue;
            }

            // Treat warnings as errors if requested
            if (opts.warnings_as_errors && diag.level == diagnostic_level::warning) {
                diagnostic error_diag = diag;
                error_diag.level = diagnostic_level::error;
                filtered_diags.push_back(std::move(error_diag));
            } else {
                filtered_diags.push_back(diag);
            }
        }

        result.diagnostics = std::move(filtered_diags);

        // Only set analyzed if no errors
        if (!result.has_errors()) {
            result.analyzed = std::move(analyzed);
        }

        return result;
    }
} // namespace datascript::semantic

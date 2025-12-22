#include "compiler.hh"
#include <datascript/base_renderer.hh>
#include <datascript/ir_builder.hh>
#include <datascript/parser.hh>
#include <datascript/parser_error.hh>
#include <datascript/renderer_registry.hh>
#include <datascript/semantic.hh>
#include <algorithm>
#include <fstream>
#include <iostream>

namespace datascript::driver {

using namespace datascript::codegen;

Compiler::Compiler(const CompilerOptions& options, Logger& logger)
    : options_(options)
    , logger_(logger)
{
}

int Compiler::compile() {
    try {
        logger_.verbose("Starting compilation...");

        // Process each input file
        for (const auto& input_file : options_.input_files) {
            logger_.info("Compiling: " + input_file.string());

            // Stage 1 & 2: Load file and imports
            module_set modules = load_imports(input_file);

            // CMake integration: print imports and exit
            if (options_.output_mode == OutputMode::PrintImports) {
                return print_imports(modules);
            }

            // Stage 3: Semantic analysis
            semantic::analysis_result analysis_result;
            if (!run_semantic_analysis(modules, analysis_result)) {
                return 1;  // Errors occurred
            }

            // Stage 4: Build IR from analyzed modules
            ir::bundle bundle = build_ir(analysis_result);

            // CMake integration: print outputs and exit
            if (options_.output_mode == OutputMode::PrintOutputs) {
                return print_outputs(bundle, modules);
            }

            // Stage 5: Generate code
            generate_code(bundle, modules);
        }

        logger_.success("Compilation successful");
        return 0;

    } catch (const parse_error& e) {
        logger_.error(std::string("Parse error: ") + e.what());
        return 1;
    } catch (const module_load_error& e) {
        logger_.error(std::string("Module load error: ") + e.what());
        return 1;
    } catch (const std::exception& e) {
        logger_.error(std::string("Error: ") + e.what());
        return 1;
    }
}

// ============================================================================
// Pipeline Stages
// ============================================================================

module_set Compiler::load_imports(const std::filesystem::path& main_file) {
    logger_.verbose("Loading: " + main_file.string());

    // Convert include_dirs to vector of strings
    std::vector<std::string> search_paths;
    for (const auto& dir : options_.include_dirs) {
        search_paths.push_back(dir.string());
    }

    return load_modules_with_imports(main_file.string(), search_paths);
}

bool Compiler::run_semantic_analysis(module_set& modules, semantic::analysis_result& out_result) {
    logger_.verbose("Running semantic analysis...");

    // Build analysis options from compiler options
    semantic::analysis_options analysis_opts;
    analysis_opts.warnings_as_errors = options_.warnings_as_errors;
    analysis_opts.disabled_warnings = options_.disabled_warnings;

    if (options_.suppress_all_warnings) {
        analysis_opts.min_level = semantic::diagnostic_level::error;
    }

    // Run 7-phase semantic analysis
    out_result = semantic::analyze(modules, analysis_opts);

    // Print diagnostics
    print_diagnostics(out_result);

    // Return true if successful, false if errors
    return !out_result.has_errors();
}

ir::bundle Compiler::build_ir(const semantic::analysis_result& result) {
    logger_.verbose("Building IR...");

    return ir::build_ir(result.analyzed.value());
}

void Compiler::generate_code(const ir::bundle& bundle, const module_set& modules) {
    logger_.verbose("Generating code for language: " + options_.target_language);

    // Get renderer from registry
    auto& registry = RendererRegistry::instance();
    auto* renderer = registry.get_renderer(options_.target_language);

    if (!renderer) {
        auto available = registry.get_available_languages();
        std::string error_msg = "Renderer not found for language: " + options_.target_language;

        if (!available.empty()) {
            error_msg += "\n\nAvailable languages:";
            for (const auto& lang : available) {
                error_msg += "\n  - " + lang;
            }
        }

        throw std::runtime_error(error_msg);
    }

    // Apply generator-specific options to renderer
    for (const auto& [option_name, option_value] : options_.generator_options) {
        renderer->set_option(option_name, option_value);
    }

    // If --use-input-name is set, derive output name from input filename
    if (options_.use_input_name && !options_.input_files.empty()) {
        std::string input_basename = options_.input_files[0].stem().string();
        renderer->set_option("output-name", input_basename);
    }

    // Determine output directory (use current directory if not specified)
    std::filesystem::path output_dir = options_.output_dir;
    if (output_dir.empty()) {
        output_dir = std::filesystem::current_path();
    }

    // Add package-based subdirectory (unless --flat-output)
    // e.g., "formats.mz" -> "formats/mz/"
    if (!options_.flat_output && !modules.main.package_name.empty()) {
        std::string pkg_path = modules.main.package_name;
        std::replace(pkg_path.begin(), pkg_path.end(), '.',
                     static_cast<char>(std::filesystem::path::preferred_separator));
        output_dir /= pkg_path;
    }

    // Create directories as needed
    std::filesystem::create_directories(output_dir);

    // Generate output files
    auto output_files = renderer->generate_files(bundle, output_dir);

    logger_.verbose("Generated " + std::to_string(output_files.size()) + " file(s)");

    // Write files to disk
    write_output_files(output_files);
}

// ============================================================================
// Output File Writing
// ============================================================================

void Compiler::write_output_files(const std::vector<OutputFile>& files) {
    for (const auto& file : files) {
        logger_.verbose("Writing: " + file.path.string());

        // Create parent directories if needed
        auto parent = file.path.parent_path();
        if (!parent.empty()) {
            std::filesystem::create_directories(parent);
        }

        // Write file
        std::ofstream ofs(file.path);
        if (!ofs) {
            throw std::runtime_error("Failed to open file for writing: " + file.path.string());
        }

        ofs << file.content;

        if (!ofs) {
            throw std::runtime_error("Failed to write file: " + file.path.string());
        }

        logger_.success("Generated: " + file.path.string());
    }
}

// ============================================================================
// Utility Methods
// ============================================================================

void Compiler::print_diagnostics(const semantic::analysis_result& result) {
    // Print individual diagnostics
    for (const auto& diag : result.diagnostics) {
        if (diag.level == semantic::diagnostic_level::error) {
            logger_.error(diag.message);
        } else if (diag.level == semantic::diagnostic_level::warning &&
                   !options_.suppress_all_warnings) {
            logger_.warning(diag.message);
        }
    }

    // Summary
    if (result.has_errors()) {
        logger_.error("Total errors: " + std::to_string(result.error_count()));
    }
    if (result.has_warnings() && !options_.suppress_all_warnings) {
        logger_.warning("Total warnings: " + std::to_string(result.warning_count()));
    }
}

// ============================================================================
// CMake Integration Methods
// ============================================================================

int Compiler::print_imports(const module_set& modules) {
    // Print package names of all imported modules (one per line)
    // This is used by CMake for dependency tracking
    for (const auto& mod : modules.imported) {
        if (!mod.package_name.empty()) {
            std::cout << mod.package_name << "\n";
        }
    }
    return 0;
}

int Compiler::print_outputs(const ir::bundle& bundle, const module_set& modules) {
    // Get renderer from registry
    auto& registry = RendererRegistry::instance();
    auto* renderer = registry.get_renderer(options_.target_language);

    if (!renderer) {
        std::cerr << "Error: Renderer not found for language: " << options_.target_language << "\n";
        return 1;
    }

    // Apply generator-specific options to renderer (needed for mode selection)
    for (const auto& [option_name, option_value] : options_.generator_options) {
        renderer->set_option(option_name, option_value);
    }

    // If --use-input-name is set, derive output name from input filename
    if (options_.use_input_name && !options_.input_files.empty()) {
        std::string input_basename = options_.input_files[0].stem().string();
        renderer->set_option("output-name", input_basename);
    }

    // Derive relative output path from package name (unless --flat-output)
    // e.g., "formats.mz" -> "formats/mz/"
    std::string pkg_path;
    if (!options_.flat_output && !modules.main.package_name.empty()) {
        pkg_path = modules.main.package_name;
        std::replace(pkg_path.begin(), pkg_path.end(), '.', '/');
        pkg_path += "/";
    }

    // Generate files with empty output dir to get just the filenames
    auto files = renderer->generate_files(bundle, "");
    for (const auto& f : files) {
        std::cout << pkg_path << f.path.filename().string() << "\n";
    }
    return 0;
}

}  // namespace datascript::driver

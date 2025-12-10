#pragma once

#include "compiler_options.hh"
#include "logger.hh"
#include <datascript/ast.hh>
#include <datascript/ir.hh>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/codegen/option_description.hh>

namespace datascript::driver {

/// Main compiler driver
class Compiler {
public:
    explicit Compiler(const CompilerOptions& options, Logger& logger);

    /// Compile input files to target language
    /// Returns 0 on success, non-zero on error
    int compile();

private:
    // ========================================================================
    // Compilation Pipeline Stages
    // ========================================================================

    /// Stage 1 & 2: Load file and imports (combined)
    module_set load_imports(const std::filesystem::path& main_file);

    /// Stage 3: Run semantic analysis (7 phases)
    /// Returns true on success, false on error
    bool run_semantic_analysis(module_set& modules, semantic::analysis_result& out_result);

    /// Stage 4: Build IR from analysis result
    ir::bundle build_ir(const semantic::analysis_result& result);

    /// Stage 5: Generate code in target language
    void generate_code(const ir::bundle& bundle, const module_set& modules);

    // ========================================================================
    // Output File Writing
    // ========================================================================

    /// Write output files to disk
    void write_output_files(const std::vector<codegen::OutputFile>& files);

    // ========================================================================
    // Utility Methods
    // ========================================================================

    /// Print diagnostic messages
    void print_diagnostics(const semantic::analysis_result& result);

    // ========================================================================
    // CMake Integration Methods
    // ========================================================================

    /// Print import dependencies (for --print-imports)
    int print_imports(const module_set& modules);

    /// Print output files (for --print-outputs)
    int print_outputs(const ir::bundle& bundle, const module_set& modules);

    // ========================================================================
    // State
    // ========================================================================

    const CompilerOptions& options_;
    Logger& logger_;
};

}  // namespace datascript::driver

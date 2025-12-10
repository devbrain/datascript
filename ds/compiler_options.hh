#pragma once

#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <variant>

// Forward declare OptionValue from codegen (to avoid circular dependency)
namespace datascript::codegen {
    using OptionValue = std::variant<bool, int64_t, std::string>;
}

namespace datascript::driver {

/// Output mode for the compiler
enum class OutputMode {
    Compile,       // Normal compilation (default)
    PrintImports,  // Print imports and exit (for CMake dependency tracking)
    PrintOutputs   // Print output filenames and exit (for CMake dependency tracking)
};

/// Compiler options (driver configuration only)
struct CompilerOptions {
    // ========================================================================
    // Input/Output
    // ========================================================================

    std::vector<std::filesystem::path> input_files;
    std::filesystem::path output_dir;                // Always a directory
    std::vector<std::filesystem::path> include_dirs; // -I paths

    // ========================================================================
    // Target Selection
    // ========================================================================

    std::string target_language = "cpp";             // Language name from registry

    // ========================================================================
    // Generator Options
    // ========================================================================

    /// Generator-specific options (e.g., "enum-to-string" -> true)
    /// Parsed from CLI args like --cpp-enum-to-string=true
    /// Applied to renderer before code generation
    std::map<std::string, datascript::codegen::OptionValue> generator_options;

    // ========================================================================
    // Semantic Analysis Options
    // ========================================================================

    bool warnings_as_errors = false;                 // -Werror
    bool suppress_all_warnings = false;              // -w
    std::set<std::string> disabled_warnings;         // -Wno-W001
    std::map<std::string, bool> warning_overrides;   // -Werror=W001

    // ========================================================================
    // Diagnostic Options
    // ========================================================================

    bool verbose = false;                            // -v, --verbose
    bool quiet = false;                              // -q, --quiet

    // ========================================================================
    // Output Mode (for CMake integration)
    // ========================================================================

    OutputMode output_mode = OutputMode::Compile;    // --print-imports, --print-outputs
    bool flat_output = false;                        // --flat-output (no package subdirs)
};

/// Parse command-line arguments
/// Throws std::runtime_error on invalid arguments
CompilerOptions parse_command_line(int argc, char** argv);

/// Print help message
void print_help(const char* program_name);

/// Print version information
void print_version();

/// Print available code generators
void print_generators();

/// Print search directories
void print_search_dirs(const CompilerOptions& opts);

}  // namespace datascript::driver

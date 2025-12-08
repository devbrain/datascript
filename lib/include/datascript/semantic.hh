//
// Semantic Analysis for DataScript
//
// This module provides a 7-phase semantic analysis pipeline for DataScript schemas.
// Each phase builds on the previous ones to validate, annotate, and analyze the
// parsed AST before code generation.
//
// ANALYSIS PHASES:
//   1. Symbol Collection    - Build global symbol table from all modules
//   2. Name Resolution      - Resolve type and constant references
//   3. Type Checking        - Verify type compatibility and expression types
//   4. Constant Evaluation  - Evaluate constant expressions to concrete values
//   5. Size Calculation     - Calculate struct layouts and type sizes
//   6. Constraint Validation- Check constraints and conditions (always-true/false)
//   7. Reachability Analysis- Detect unused symbols (constants, imports)
//
// USAGE EXAMPLE:
//   auto modules = parse_datascript_modules("schema.ds");
//
//   analysis_options opts;
//   opts.warnings_as_errors = false;
//   opts.disabled_warnings = {"W001", "W005"};  // Disable unused warnings
//
//   auto result = semantic::analyze(modules, opts);
//
//   if (result.has_errors()) {
//       result.print_diagnostics(std::cerr, /* use_color */ true);
//       return 1;
//   }
//
//   // Use analyzed result for code generation
//   const auto& analyzed = result.analyzed.value();
//   auto size = analyzed.type_info_map[&my_struct].size;
//   auto value = analyzed.constant_values[&my_const];
//

#pragma once

#include "ast.hh"
#include "parser.hh"
#include <optional>
#include <vector>
#include <map>
#include <set>
#include <string>
#include <functional>
#include <memory>

namespace datascript::semantic {

// ============================================================================
// Diagnostics
// ============================================================================

/// Severity level for diagnostic messages.
/// Errors prevent successful compilation; warnings indicate potential issues.
enum class diagnostic_level {
    error,      ///< Must fix - prevents compilation
    warning,    ///< Should fix - suspicious code that may cause runtime issues
    note,       ///< Additional context for errors/warnings
    hint        ///< Suggestion for improvement (best practices)
};

/// Diagnostic codes for documentation and selective disabling.
///
/// Each diagnostic has a unique code that can be used to:
/// - Disable specific warnings (analysis_options::disabled_warnings)
/// - Document error messages in user-facing docs
/// - Track diagnostic history across compiler versions
///
/// Code format:
/// - E### for errors (E001-E099)
/// - W### for warnings (W001-W099)
///
/// Categories:
/// - E001-E009: Symbol errors (undefined, duplicate)
/// - E010-E019: Type errors (mismatches, invalid operations)
/// - E020-E029: Value errors (overflow, invalid constants)
/// - E030-E039: Dependency errors (circular references)
/// - E040-E049: Constraint errors (invalid constraints)
/// - E050-E059: Configuration errors (invalid options, unknown languages)
/// - W001-W009: Unused symbol warnings
/// - W010-W019: Naming and style warnings
/// - W020-W029: Size and alignment warnings
/// - W030-W039: Type usage warnings (truncation, sign mismatch)
/// - W040-W049: Best practice warnings
namespace diag_codes {
    // === Errors (E_xxx) ===

    // Symbol errors (E001-E009)
    constexpr const char* E_UNDEFINED_TYPE = "E001";        ///< Referenced type not found in symbol table
    constexpr const char* E_UNDEFINED_CONSTANT = "E002";    ///< Referenced constant not found
    constexpr const char* E_UNDEFINED_FIELD = "E003";       ///< Field not found in struct/union
    constexpr const char* E_UNDEFINED_PACKAGE = "E004";     ///< Import references unknown package
    constexpr const char* E_DUPLICATE_DEFINITION = "E005";  ///< Symbol redefined in same scope
    constexpr const char* E_DUPLICATE_PACKAGE = "E006";     ///< Package declared multiple times
    constexpr const char* E_DUPLICATE_FIELD = "E007";       ///< Field name used twice in struct

    // Type errors (E010-E019)
    constexpr const char* E_TYPE_MISMATCH = "E010";         ///< Expression type doesn't match expected type
    constexpr const char* E_INCOMPATIBLE_TYPES = "E011";    ///< Cannot convert between types (e.g., string + int)
    constexpr const char* E_INVALID_OPERAND_TYPE = "E012";  ///< Operator doesn't support operand type (e.g., "foo" + "bar")
    constexpr const char* E_NOT_A_STRUCT = "E013";          ///< Field access on non-struct type
    constexpr const char* E_NOT_AN_ARRAY = "E014";          ///< Array indexing on non-array type
    constexpr const char* E_NOT_CALLABLE = "E015";          ///< Function call on non-function
    constexpr const char* E_PARAM_COUNT_MISMATCH = "E016";  ///< Wrong number of type parameters in instantiation

    // Value errors (E020-E029)
    constexpr const char* E_OVERFLOW = "E020";              ///< Constant value too large for type (e.g., 256 in uint8)
    constexpr const char* E_UNDERFLOW = "E021";             ///< Constant value too small for type (e.g., -1 in uint8)
    constexpr const char* E_DIVISION_BY_ZERO = "E022";      ///< Division or modulo by zero in constant expression
    constexpr const char* E_NEGATIVE_SIZE = "E023";         ///< Array size or bitfield width is negative
    constexpr const char* E_INVALID_ENUM_VALUE = "E024";    ///< Enum value doesn't fit in base type

    // Dependency errors (E030-E039)
    constexpr const char* E_CIRCULAR_DEPENDENCY = "E030";   ///< General circular dependency detected
    constexpr const char* E_CIRCULAR_TYPE = "E031";         ///< Type references itself (e.g., struct A { A field; })
    constexpr const char* E_CIRCULAR_CONSTANT = "E032";     ///< Constant defined in terms of itself
    constexpr const char* E_UNRESOLVED_DEPENDENCY = "E033"; ///< Dependency cannot be resolved

    // Constraint errors (E040-E049)
    constexpr const char* E_INVALID_CONSTRAINT = "E040";    ///< Constraint expression is malformed
    constexpr const char* E_CONSTRAINT_VIOLATION = "E041";  ///< Constraint is provably false (always fails)

    // Configuration errors (E050-E059)
    constexpr const char* E_UNKNOWN_TARGET_LANGUAGE = "E050";  ///< Unknown/unregistered target language in analysis options

    // === Warnings (W_xxx) ===

    // Unused symbols (W001-W009)
    constexpr const char* W_UNUSED_CONSTANT = "W001";       ///< Constant declared but never used
    constexpr const char* W_UNUSED_TYPE = "W002";           ///< Type declared but never used (disabled by default for root types)
    constexpr const char* W_UNUSED_FIELD = "W003";          ///< Struct field never accessed
    constexpr const char* W_UNUSED_PARAMETER = "W004";      ///< Function parameter not used in body
    constexpr const char* W_UNUSED_IMPORT = "W005";         ///< Import declared but symbols never used

    // Naming and style (W010-W019)
    constexpr const char* W_SHADOWING = "W010";             ///< Local declaration shadows outer scope symbol
    constexpr const char* W_WILDCARD_CONFLICT = "W011";     ///< Wildcard import creates ambiguous symbol
    constexpr const char* W_NAMING_CONVENTION = "W012";     ///< Symbol name violates convention (e.g., UPPER_CASE for constants)
    constexpr const char* W_KEYWORD_COLLISION = "W013";     ///< Identifier conflicts with target language keyword

    // Size and alignment (W020-W029)
    constexpr const char* W_LARGE_TYPE = "W020";            ///< Type is very large (>1MB), may cause performance issues
    constexpr const char* W_SUSPICIOUS_SIZE = "W021";       ///< Size calculation seems wrong (e.g., array[0])
    constexpr const char* W_ALIGNMENT_PADDING = "W022";     ///< Struct has significant padding, could be reordered
    constexpr const char* W_INEFFICIENT_LAYOUT = "W023";    ///< Fields ordered inefficiently (large padding between fields)

    // Type usage (W030-W039)
    constexpr const char* W_TRUNCATION = "W030";            ///< Value may be truncated (e.g., uint32 → uint8)
    constexpr const char* W_SIGN_MISMATCH = "W031";         ///< Mixing signed and unsigned in expression
    constexpr const char* W_IMPLICIT_CONVERSION = "W032";   ///< Implicit type conversion may be unintended

    // Best practices (W040-W049)
    constexpr const char* W_MISSING_DOCSTRING = "W040";     ///< Public symbol lacks documentation
    constexpr const char* W_DEPRECATED = "W041";            ///< Using deprecated feature
    constexpr const char* W_TODO_COMMENT = "W042";          ///< TODO comment found in code
}

/// A single diagnostic message (error, warning, or note).
///
/// Diagnostics are collected during semantic analysis and can be:
/// - Printed to the console with source context
/// - Filtered by level or code
/// - Used for IDE integration (language server protocol)
///
/// Example diagnostic output:
///   schema.ds:12:5: error E001: Undefined type 'Foo'
///       Foo field;
///       ^^^
///   schema.ds:8:8: note: Did you mean 'FooBar'?
struct diagnostic {
    diagnostic_level level;         ///< Severity (error, warning, note, hint)
    std::string code;               ///< Diagnostic code (e.g., "E001", "W042")
    std::string message;            ///< Human-readable error message
    ast::source_pos position;       ///< Primary location in source file

    // Optional related location (e.g., "previous definition here")
    std::optional<ast::source_pos> related_position;  ///< Secondary location for context
    std::optional<std::string> related_message;       ///< Message for related location

    // Optional fix suggestion
    std::optional<std::string> suggestion;  ///< Suggested fix (e.g., "use 'FooBar' instead")

    /// Format diagnostic as human-readable string.
    /// @param show_source_line If true, include source code line with caret indicator
    /// @return Formatted string like "file.ds:12:5: error E001: message"
    std::string format(bool show_source_line = false) const;
};

// ============================================================================
// Symbol Tables
// ============================================================================

/// Symbols defined in a single module.
///
/// A module can declare constants, types, and constraints. All symbols
/// are stored with their names as keys for fast lookup.
///
/// Example module:
///   package foo.bar;
///   const uint32 SIZE = 100;
///   struct Point { uint32 x; uint32 y; }
///
/// Results in module_symbols:
///   package_name = "foo.bar"
///   constants = {"SIZE" → &constant_def}
///   structs = {"Point" → &struct_def}
struct module_symbols {
    std::string package_name;  ///< Fully-qualified package name: "foo.bar.baz"

    // Symbols defined in this module (name → definition)
    std::map<std::string, const ast::constant_def*> constants;      ///< Constant definitions
    std::map<std::string, const ast::struct_def*> structs;          ///< Struct type definitions
    std::map<std::string, const ast::union_def*> unions;            ///< Union type definitions
    std::map<std::string, const ast::enum_def*> enums;              ///< Enum type definitions
    std::map<std::string, const ast::subtype_def*> subtypes;        ///< Subtype definitions
    std::map<std::string, const ast::choice_def*> choices;          ///< Choice (tagged union) definitions
    std::map<std::string, const ast::constraint_def*> constraints;  ///< Named constraint definitions

    const ast::module* source_module;  ///< Pointer to the AST module this came from
};

/// Global symbol table across all modules with Java-style scoping.
///
/// Manages symbol visibility and lookup across multiple modules:
/// - Main module symbols are accessible unqualified
/// - Imported module symbols require qualified names (e.g., foo.bar.Point)
/// - Wildcard imports ("import foo.bar.*;") make symbols accessible unqualified
///
/// Lookup priority (same as Java):
/// 1. Main module symbols
/// 2. Wildcard imports (with conflict detection)
/// 3. Qualified imports
///
/// Example:
///   // main.ds
///   package com.example;
///   import geometry.*;      // Wildcard import
///   import utils.Math;      // Qualified import
///
///   struct Scene {
///       Point origin;       // Found in wildcard (geometry.Point)
///       Math.Vector3 dir;   // Found in qualified (utils.Math.Vector3)
///   }
struct symbol_table {
    module_symbols main;  ///< Symbols from the main module (always unqualified access)

    /// Symbols from imported modules.
    /// Key: fully-qualified package name (e.g., "foo.bar")
    /// Access requires qualified name: foo.bar.Point
    std::map<std::string, module_symbols> imported;

    // Flattened view for wildcard imports (Java-style)
    // After processing "import foo.bar.*;", all symbols from foo.bar
    // are accessible without qualification. Conflicts generate warnings.
    std::map<std::string, const ast::struct_def*> wildcard_structs;       ///< Structs from wildcard imports
    std::map<std::string, const ast::union_def*> wildcard_unions;         ///< Unions from wildcard imports
    std::map<std::string, const ast::enum_def*> wildcard_enums;           ///< Enums from wildcard imports
    std::map<std::string, const ast::subtype_def*> wildcard_subtypes;     ///< Subtypes from wildcard imports
    std::map<std::string, const ast::choice_def*> wildcard_choices;       ///< Choices from wildcard imports
    std::map<std::string, const ast::constant_def*> wildcard_constants;   ///< Constants from wildcard imports
    std::map<std::string, const ast::constraint_def*> wildcard_constraints; ///< Constraints from wildcard imports

    // === Unqualified Lookup Functions ===
    // Search order: main module → wildcard imports → not found

    /// Find struct by simple name (e.g., "Point").
    /// @return Pointer to struct_def or nullptr if not found
    [[nodiscard]] const ast::struct_def* find_struct(const std::string& name) const;

    /// Find union by simple name.
    [[nodiscard]] const ast::union_def* find_union(const std::string& name) const;

    /// Find enum by simple name.
    [[nodiscard]] const ast::enum_def* find_enum(const std::string& name) const;

    /// Find subtype by simple name.
    [[nodiscard]] const ast::subtype_def* find_subtype(const std::string& name) const;

    /// Find choice by simple name.
    [[nodiscard]] const ast::choice_def* find_choice(const std::string& name) const;

    /// Find constant by simple name.
    [[nodiscard]] const ast::constant_def* find_constant(const std::string& name) const;

    /// Find constraint by simple name.
    [[nodiscard]] const ast::constraint_def* find_constraint(const std::string& name) const;

    // === Qualified Lookup Functions ===
    // For names like foo.bar.Point: ["foo", "bar", "Point"]

    /// Find struct using fully-qualified name parts.
    /// @param qualified_name Name parts (e.g., ["geometry", "Point"])
    /// @return Pointer to struct_def or nullptr if not found
    [[nodiscard]] const ast::struct_def* find_struct_qualified(
        const std::vector<std::string>& qualified_name) const;

    /// Find union using fully-qualified name parts.
    [[nodiscard]] const ast::union_def* find_union_qualified(
        const std::vector<std::string>& qualified_name) const;

    /// Find enum using fully-qualified name parts.
    [[nodiscard]] const ast::enum_def* find_enum_qualified(
        const std::vector<std::string>& qualified_name) const;

    /// Find subtype using fully-qualified name parts.
    [[nodiscard]] const ast::subtype_def* find_subtype_qualified(
        const std::vector<std::string>& qualified_name) const;

    /// Find choice using fully-qualified name parts.
    [[nodiscard]] const ast::choice_def* find_choice_qualified(
        const std::vector<std::string>& qualified_name) const;

    /// Get all symbols for a specific package.
    /// @param package_name Fully-qualified package (e.g., "foo.bar")
    /// @return Pointer to module_symbols or nullptr if package not found
    [[nodiscard]] const module_symbols* get_module_symbols(const std::string& package_name) const;
};

// ============================================================================
// Type Information
// ============================================================================

/// Calculated size and alignment information for a type.
///
/// After Phase 5 (Size Calculation), every type has associated type_info
/// describing its memory layout properties.
///
/// Examples:
///   uint32:           size=4, alignment=4, fixed
///   struct Point:     size=8, alignment=4, fixed (two uint32 fields)
///   uint8[]:          size=SIZE_MAX, variable-sized
///   uint8[1..10]:     size=SIZE_MAX, variable, min=1, max=10
///
/// Variable-sized types (arrays with dynamic length) have size=SIZE_MAX
/// and is_variable_size=true. They cannot be used in fixed-size contexts.
struct type_info {
    size_t size;            ///< Size in bytes (SIZE_MAX for variable-sized types)
    size_t alignment;       ///< Alignment requirement in bytes (power of 2)
    bool is_variable_size;  ///< True for arrays with runtime-determined size
    bool is_signed;         ///< True for signed integer types (int8, int16, etc.)
    std::optional<size_t> min_size;  ///< Minimum size for ranged arrays (e.g., uint8[1..10])
    std::optional<size_t> max_size;  ///< Maximum size for ranged arrays
};

// ============================================================================
// Analyzed Module Set
// ============================================================================

/// Complete analysis results for a module set.
///
/// This structure contains all information computed during semantic analysis:
/// - Symbol tables (Phase 1)
/// - Resolved type references (Phase 2)
/// - Expression types (Phase 3)
/// - Constant values (Phase 4)
/// - Type sizes and field offsets (Phase 5)
/// - Constraint validation results (Phase 6)
/// - Reachability information (Phase 7)
///
/// Access patterns:
///   // Get size of a struct
///   auto size = analyzed.type_info_map[&struct_type].size;
///
///   // Get value of a constant
///   auto value = analyzed.constant_values[&const_def];
///
///   // Get field offset
///   auto offset = analyzed.field_offsets[&field_def];
///
///   // Resolve type reference
///   auto* struct_def = std::get<const ast::struct_def*>(
///       analyzed.resolved_types[&qualified_name]);
struct analyzed_module_set {
    const module_set* original;  ///< Pointer to original parsed modules (immutable)

    symbol_table symbols;  ///< Global symbol table (Phase 1)

    /// Type information for all types in all modules (Phase 5).
    /// Maps AST type nodes to their calculated size/alignment.
    std::map<const ast::type*, type_info> type_info_map;

    /// Evaluated constant values (Phase 4).
    /// Maps constant definitions to their computed uint64 values.
    /// Signed values are stored as two's complement.
    std::map<const ast::constant_def*, uint64_t> constant_values;

    /// Evaluated enum item values (Phase 4).
    /// Maps enum items to their computed uint64 values.
    std::map<const ast::enum_item*, uint64_t> enum_item_values;

    /// Field offsets within structs (Phase 5).
    /// Maps field definitions to their byte offset from struct start.
    /// Accounts for alignment and padding.
    std::map<const ast::field_def*, size_t> field_offsets;

    /// Resolved type references (Phase 2).
    /// Maps qualified_name AST nodes to their actual type definitions.
    /// Use std::get<T*> or std::holds_alternative<T*> to access.
    ///
    /// Example:
    ///   // "Point" in "struct S { Point p; }"
    ///   auto& resolved = analyzed.resolved_types[&point_qname];
    ///   if (auto** s = std::get_if<const ast::struct_def*>(&resolved)) {
    ///       // It's a struct
    ///   }
    using resolved_type = std::variant<
        const ast::struct_def*,
        const ast::union_def*,
        const ast::enum_def*,
        const ast::subtype_def*,
        const ast::choice_def*
    >;
    std::map<const ast::qualified_name*, resolved_type> resolved_types;

    /// Expression types (Phase 3).
    /// Maps expression AST nodes to their computed types.
    /// Used for type checking and code generation.
    std::map<const ast::expr*, ast::type> expression_types;

    /// Inferred discriminator types for inline choices (Phase 3).
    /// Maps choice definitions (without explicit selector) to their inferred
    /// discriminator primitive types based on case values.
    /// Only populated for choices where selector is std::nullopt.
    std::map<const ast::choice_def*, ast::primitive_type> choice_discriminator_types;

    /// Get symbols for a specific package.
    /// @param package_name Fully-qualified package name
    /// @return Pointer to module_symbols or nullptr
    const module_symbols* get_module_symbols(const std::string& package_name) const;
};

// ============================================================================
// Analysis Result
// ============================================================================

/// Result of semantic analysis with diagnostics.
///
/// After running analyze(), this structure contains:
/// - analyzed_module_set (if no errors occurred)
/// - All diagnostics (errors, warnings, notes)
///
/// The analyzed field is only populated if there were no errors.
/// Warnings don't prevent analysis from completing.
///
/// Usage:
///   auto result = semantic::analyze(modules);
///
///   if (result.has_errors()) {
///       result.print_diagnostics(std::cerr, true);
///       exit(1);
///   }
///
///   // Warnings present but analysis succeeded
///   if (result.has_warnings()) {
///       std::cerr << "Note: " << result.warning_count() << " warnings\n";
///   }
///
///   // Safe to use analyzed data
///   codegen(result.analyzed.value());
struct analysis_result {
    /// Analyzed module data (only present if no errors).
    /// Check with has_errors() or analyzed.has_value() before accessing.
    std::optional<analyzed_module_set> analyzed;

    /// All diagnostics collected during analysis (errors, warnings, notes).
    /// Ordered by source position.
    std::vector<diagnostic> diagnostics;

    /// Check if any errors occurred.
    /// @return true if at least one error diagnostic is present
    bool has_errors() const;

    /// Check if any warnings occurred.
    /// @return true if at least one warning diagnostic is present
    bool has_warnings() const;

    /// Count error diagnostics.
    /// @return Number of error-level diagnostics
    size_t error_count() const;

    /// Count warning diagnostics.
    /// @return Number of warning-level diagnostics
    size_t warning_count() const;

    /// Pretty-print all diagnostics to an output stream.
    /// Includes source context, colors (if enabled), and suggestions.
    /// @param os Output stream (e.g., std::cerr)
    /// @param use_color Enable ANSI color codes for terminal output
    void print_diagnostics(std::ostream& os, bool use_color = false) const;

    /// Get only error diagnostics.
    /// @return Vector of error-level diagnostics
    std::vector<diagnostic> get_errors() const;

    /// Get only warning diagnostics.
    /// @return Vector of warning-level diagnostics
    std::vector<diagnostic> get_warnings() const;
};

// ============================================================================
// Analysis Options
// ============================================================================

/// Configuration options for semantic analysis.
///
/// Controls warning levels, error handling, and optional checks.
///
/// Example configurations:
///
///   // Strict mode (all warnings, stop on first error)
///   analysis_options strict;
///   strict.warnings_as_errors = true;
///   strict.stop_on_first_error = true;
///   strict.check_naming_conventions = true;
///
///   // Permissive mode (hide unused warnings)
///   analysis_options permissive;
///   permissive.disabled_warnings = {"W001", "W005"};
///   permissive.min_level = diagnostic_level::error;
///
///   // Documentation check
///   analysis_options docs_check;
///   docs_check.check_missing_docstrings = true;
struct analysis_options {
    /// Treat all warnings as errors (fail compilation on any warning).
    bool warnings_as_errors = false;

    /// Stop analysis after the first error (faster failure).
    /// If false, collect all errors before stopping.
    bool stop_on_first_error = false;

    /// Set of warning codes to disable (e.g., {"W001", "W042"}).
    /// Warnings with these codes will not be reported.
    std::set<std::string> disabled_warnings;

    /// Minimum diagnostic level to report.
    /// - error: Only show errors
    /// - warning: Show errors and warnings (default)
    /// - note: Show errors, warnings, and notes
    /// - hint: Show everything including hints
    diagnostic_level min_level = diagnostic_level::warning;

    // === Optional Checks (disabled by default for performance) ===

    /// Check naming conventions (e.g., constants should be UPPER_CASE).
    /// Generates W_NAMING_CONVENTION warnings.
    bool check_naming_conventions = false;

    /// Warn about missing docstrings on public symbols.
    /// Generates W_MISSING_DOCSTRING warnings.
    bool check_missing_docstrings = false;

    /// Analyze struct field ordering for efficiency.
    /// Generates W_INEFFICIENT_LAYOUT and W_ALIGNMENT_PADDING warnings.
    bool check_layout_efficiency = false;

    /// Target programming languages for code generation.
    /// When specified, identifiers are validated against keywords in these languages.
    /// Empty set (default) checks against all registered languages.
    /// Example: {"cpp", "rust", "python"}
    /// Generates W_KEYWORD_COLLISION warnings.
    std::set<std::string> target_languages;
};

// ============================================================================
// Main Analysis Interface
// ============================================================================

/// Perform complete semantic analysis on a module set.
///
/// Runs all 7 phases of semantic analysis:
///   1. Symbol Collection    - Build global symbol table
///   2. Name Resolution      - Resolve all type and constant references
///   3. Type Checking        - Verify type compatibility
///   4. Constant Evaluation  - Compute constant values
///   5. Size Calculation     - Calculate struct layouts and type sizes
///   6. Constraint Validation- Check constraints and conditions
///   7. Reachability Analysis- Detect unused symbols
///
/// The analysis stops after any phase that produces errors (if stop_on_first_error
/// is set), but warnings don't prevent later phases from running.
///
/// @param modules Parsed module set (from parse_datascript_modules)
/// @param opts Analysis configuration (warnings, error handling)
/// @return Analysis result with diagnostics and analyzed data (if successful)
///
/// Example:
///   auto modules = parse_datascript("schema.ds");
///   auto result = semantic::analyze(modules);
///
///   if (result.has_errors()) {
///       std::cerr << "Semantic analysis failed:\n";
///       result.print_diagnostics(std::cerr, true);
///       return 1;
///   }
///
///   // Use analyzed.type_info_map, analyzed.constant_values, etc.
///   generate_code(result.analyzed.value());
///
/// @note This function modifies the module set in-place to desugar inline types
///       before analysis. Pass a copy if you need to preserve the original AST.
analysis_result analyze(module_set& modules,
                        const analysis_options& opts = {});

// ============================================================================
// Individual Phases (for testing and incremental analysis)
// ============================================================================

/// Individual analysis phases that can be run separately.
///
/// These are primarily for:
/// - Unit testing (test each phase independently)
/// - Incremental analysis (re-run only changed phases)
/// - Custom analysis pipelines
///
/// Normal users should use the analyze() function above.
namespace phases {

/// Phase 0: Inline Type Desugaring
///
/// Transforms inline union and struct field definitions into named types.
/// This preprocessing step runs before semantic analysis to simplify
/// downstream processing.
///
/// Example transformation:
///   Container { union { ... } payload; }
/// Becomes:
///   Container__payload__type { ... }  // Generated union
///   Container { Container__payload__type payload; }  // Regular field
///
/// @param modules Module set to desugar (modified in-place)
void desugar_inline_types(module_set& modules);

/// Phase 1: Symbol Collection
///
/// Builds the global symbol table by collecting all symbol definitions
/// from all modules (main + imported).
///
/// Checks:
/// - Duplicate definitions within the same module
/// - Package name conflicts
/// - Invalid symbol names
/// - Keyword collisions with target languages (if target_languages specified)
///
/// @param modules Module set to analyze
/// @param diagnostics Output vector for error/warning messages
/// @param opts Analysis options (for target language keyword validation)
/// @return Global symbol table with main and imported module symbols
symbol_table collect_symbols(
    const module_set& modules,
    std::vector<diagnostic>& diagnostics,
    const analysis_options& opts = {});

/// Phase 2: Name Resolution
///
/// Resolves all type and constant references using Java-style scoping.
/// Maps qualified_name AST nodes to their actual definitions.
///
/// Resolution order:
/// 1. Main module symbols
/// 2. Wildcard imports (with conflict detection)
/// 3. Qualified imports
///
/// Checks:
/// - Undefined type errors
/// - Undefined constant errors
/// - Ambiguous wildcard imports (warnings)
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires symbols from Phase 1)
/// @param diagnostics Output vector for error/warning messages
void resolve_names(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Phase 3: Type Checking
///
/// Verifies type compatibility for all expressions and validates
/// that operations are performed on compatible types.
///
/// Checks:
/// - Binary operator type compatibility (e.g., int + int, not string + int)
/// - Unary operator type compatibility (e.g., -int, not -string)
/// - Field access on struct/union types
/// - Array indexing on array types
/// - Assignment type compatibility
///
/// Populates: analyzed.expression_types
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires Phase 2)
/// @param diagnostics Output vector for error/warning messages
void check_types(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Phase 4: Constant Evaluation
///
/// Evaluates all constant expressions to concrete values using
/// compile-time evaluation. Handles arithmetic, bitwise operations,
/// and constant folding.
///
/// Checks:
/// - Division by zero
/// - Overflow/underflow
/// - Type overflow (value too large for type)
/// - Circular constant dependencies
///
/// Populates: analyzed.constant_values
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires Phase 3)
/// @param diagnostics Output vector for error/warning messages
void evaluate_constants(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Utility: Evaluate Constant Expression to uint64
///
/// Evaluates a compile-time constant expression and returns its uint64 value.
/// This is used by Phase 5 (size calculation) to evaluate:
/// - Bitfield widths: bit<WIDTH> where WIDTH is a constant
/// - Fixed array sizes: uint32 data[SIZE] where SIZE is a constant
/// - Array range bounds: uint8 arr[MIN..MAX]
///
/// Supports:
/// - Integer literals
/// - Constant references (identifiers)
/// - Arithmetic operators (+, -, *, /, %, <<, >>, &, |, ^)
/// - Unary operators (-, +, ~, !)
/// - Ternary operator (? :)
///
/// Requirements:
/// - Must run after Phase 4 (constant evaluation)
/// - analyzed.constant_values must be populated
///
/// @param expr Expression to evaluate
/// @param analyzed Analysis result with evaluated constants
/// @param diagnostics Output vector for error messages
/// @return Optional uint64 value if expression is a compile-time constant
std::optional<uint64_t> evaluate_constant_uint(
    const ast::expr& expr,
    const analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Phase 5: Size and Layout Calculation
///
/// Calculates memory layout for all types:
/// - Primitive type sizes (uint8=1, uint16=2, uint32=4, etc.)
/// - Struct field offsets with proper alignment
/// - Union sizes (max of all fields)
/// - Array sizes (element_size * count)
///
/// Alignment rules:
/// - uint8: 1-byte aligned
/// - uint16: 2-byte aligned
/// - uint32, uint64, uint128: 4-byte aligned
/// - Structs: Aligned to largest field alignment
///
/// Checks:
/// - Circular type dependencies
/// - Variable-sized types in fixed contexts
/// - Extremely large types (warnings)
///
/// Populates: analyzed.type_info_map, analyzed.field_offsets
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires Phase 4)
/// @param diagnostics Output vector for error/warning messages
void calculate_sizes(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Phase 6: Constraint Validation
///
/// Validates constraint expressions and field conditions.
/// Detects constraints that are always true (useless) or
/// always false (schema error).
///
/// Checks:
/// - Always-true constraints (warning W_DEPRECATED)
/// - Always-false constraints (error E_CONSTRAINT_VIOLATION)
/// - Always-false field conditions (warning)
/// - Invalid constraint expressions
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires Phase 5)
/// @param diagnostics Output vector for error/warning messages
void validate_constraints(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

/// Phase 7: Reachability and Dependency Analysis
///
/// Detects unused symbols by tracking all references through:
/// - Expressions (constant references)
/// - Types (type references in fields, arrays)
/// - Imports (qualified type references)
///
/// Current implementation warns about:
/// - Unused constants (W_UNUSED_CONSTANT)
/// - Unused imports (W_UNUSED_IMPORT)
///
/// Note: Does NOT warn about unused types (structs, unions, enums, choices)
/// because in a schema language, top-level types often serve as "root" or
/// "entry point" types that aren't referenced by other types.
///
/// @param modules Module set being analyzed
/// @param analyzed Analysis result to populate (requires Phase 6)
/// @param diagnostics Output vector for error/warning messages
void analyze_reachability(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics);

} // namespace phases

} // namespace datascript::semantic

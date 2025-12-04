#pragma once

#include <datascript/codegen_commands.hh>
#include <datascript/codegen/option_description.hh>
#include <datascript/ir.hh>
#include <filesystem>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace datascript::codegen {

// ============================================================================
// Expression Rendering Context
// ============================================================================

/**
 * Context information for expression rendering.
 *
 * **UNIVERSAL CONCEPT** - Shared across all language renderers
 *
 * This context tracks the state needed for rendering expressions in different
 * situations (e.g., inside struct methods vs. standalone functions). While the
 * specific syntax varies between languages, all renderers need these concepts:
 *
 * **Multi-Language Usage Examples:**
 * - C++: obj.field_name         (object_name = "obj")
 * - Python: self.field_name     (object_name = "self")
 * - Rust: self.field_name       (object_name = "self")
 * - Go: m.FieldName             (object_name = "m", varies by convention)
 * - TypeScript: this.field_name (object_name = "this")
 *
 * **Why This is Universal:**
 * - All languages distinguish between field access in methods vs functions
 * - All languages have a receiver/object/self reference in methods
 * - All languages need error handling mode selection (exceptions vs results)
 * - All languages need variable name tracking for temporaries
 * - All languages need to distinguish constants from field references
 *
 * **Language-Specific Defaults:**
 * Each renderer should initialize object_name to its language convention
 * via the get_default_object_name() virtual method in BaseRenderer.
 */
struct ExprContext {
    /// Object/receiver name for field references
    /// Language-specific: "obj" (C++), "self" (Python/Rust), "this" (TS), etc.
    std::string object_name = "obj";

    /// Are we inside a struct/class method?
    /// Universal concept: affects field access syntax in all languages
    bool in_struct_method = false;

    /// Are we in a union field reader where field refs should use parent context?
    /// For union constraints that reference parent struct fields: parent->field_name
    bool use_parent_context = false;

    /// Name of the field currently being read (for union field readers)
    /// This field should NOT use parent context (it's the local variable)
    std::string current_field_name;

    /// Use safe reads (return errors) vs exceptions?
    /// Universal concept: Result<T> vs throw (C++/Python/TS), always Result (Rust/Go)
    bool use_safe_reads = true;

    /// Variable name mappings (e.g., temporary variables, loop counters)
    /// Universal: all languages need to track temporaries and parameters
    std::map<std::string, std::string> variable_names;

    /// Module constants (for distinguishing constant refs from field refs)
    /// Universal: distinguish HEADER_SIZE (constant) from length (field)
    const std::map<std::string, uint64_t>* module_constants = nullptr;

    /// Add a variable to context (used for temporaries, parameters)
    void add_variable(const std::string& name, const std::string& mapped_name) {
        variable_names[name] = mapped_name;
    }

    /// Look up a variable (returns original name if not found)
    std::string get_variable(const std::string& name) const {
        auto it = variable_names.find(name);
        return it != variable_names.end() ? it->second : name;
    }
};

// ============================================================================
// Rendering Configuration
// ============================================================================

/// Configuration options for code generation
struct RenderOptions {
    bool use_exceptions = true;           ///< Exception vs safe error handling mode
    bool generate_comments = true;        ///< Include source comments in generated code
    bool generate_documentation = true;   ///< Generate doc comments from docstrings
    int indent_size = 4;                  ///< Number of spaces per indent level
};

// ============================================================================
// Language Metadata
// ============================================================================

/// Language-specific metadata and capabilities
struct LanguageMetadata {
    std::string name;                     ///< Language name (e.g., "C++", "Rust")
    std::string version;                  ///< Language version (e.g., "C++20", "Rust 2021")
    std::string file_extension;           ///< File extension (e.g., ".cc", ".rs", ".py")
    bool is_case_sensitive;               ///< True for C/C++/Rust, false for Pascal/Basic
    bool supports_generics;               ///< Language has generic/template types
    bool supports_exceptions;             ///< Language has exception handling
};

// ============================================================================
// Abstract Base Renderer
// ============================================================================

/// Abstract base class for all language-specific code renderers
///
/// Each target language (C++, Rust, Python, etc.) implements this interface
/// to generate code from the language-agnostic IR and command stream.
///
/// **Design Principles:**
/// - Pure virtual interface for language-independent operations
/// - Each renderer owns its expression rendering logic
/// - Keyword validation integrated for early error detection
/// - Metadata-driven for registry queries during semantic analysis
///
/// **Example Usage:**
/// \code
///   auto& registry = RendererRegistry::instance();
///   auto* renderer = registry.get_renderer("cpp");
///
///   RenderOptions opts;
///   opts.use_exceptions = true;
///
///   std::string code = renderer->render_module(ir_module, opts);
/// \endcode
class BaseRenderer {
public:
    virtual ~BaseRenderer() = default;

    // ========================================================================
    // Core Rendering Methods (must be implemented)
    // ========================================================================

    /// Render a complete IR bundle to source code
    /// @param bundle The IR bundle to render
    /// @param options Rendering configuration options
    /// @return Generated source code as string
    virtual std::string render_module(const ir::bundle& bundle,
                                      const RenderOptions& options) = 0;

    /// Render a single command to output
    /// @param cmd The command to render
    /// @note Called by derived class during command stream processing
    virtual void render_command(const Command& cmd) = 0;

    /// Render an IR expression to a code string
    /// @param expr The IR expression to render
    /// @return Expression as code string (e.g., "(a + b)", "obj.field")
    /// @note Expression rendering is internal to each renderer implementation
    virtual std::string render_expression(const ir::expr* expr) = 0;

    // ========================================================================
    // Language Metadata & Capabilities
    // ========================================================================

    /// Get complete language metadata
    /// @return Language metadata structure
    [[nodiscard]] virtual LanguageMetadata get_metadata() const = 0;

    /// Get the language name
    /// @return Language name (e.g., "C++", "Rust", "Python")
    [[nodiscard]] virtual std::string get_language_name() const = 0;

    /// Get the file extension for generated files
    /// @return File extension (e.g., ".cc", ".rs", ".py")
    [[nodiscard]] virtual std::string get_file_extension() const = 0;

    /// Check if the language is case-sensitive for identifiers
    /// @return true for C/C++/Rust/Python, false for Pascal/Basic/SQL
    [[nodiscard]] virtual bool is_case_sensitive() const = 0;

    /// Get the default object/receiver name for this language
    ///
    /// Returns the idiomatic name for the object/receiver reference used in
    /// method contexts. This is used to initialize ExprContext.object_name
    /// with language-appropriate defaults.
    ///
    /// @return Default object name
    ///
    /// **Language-Specific Conventions:**
    /// - C++: "obj" (by convention, no language requirement)
    /// - Python: "self" (required by convention, not enforced)
    /// - Rust: "self" (language keyword for method receiver)
    /// - Go: varies by project ("m", "p", "msg", etc.)
    /// - TypeScript/JavaScript: "this" (language keyword)
    ///
    /// @note This is a default - code may override for specific contexts
    [[nodiscard]] virtual std::string get_default_object_name() const {
        return "obj";  // Reasonable default for most languages
    }

    // ========================================================================
    // Keyword & Identifier Validation
    // ========================================================================

    /// Check if an identifier is a reserved keyword in this language
    ///
    /// Used during semantic analysis to detect conflicts before code generation.
    ///
    /// @param identifier The identifier to check (e.g., "class", "struct")
    /// @return true if identifier is a reserved keyword
    ///
    /// @note Implementation should respect language case sensitivity:
    ///       - Case-sensitive: "Class" != "class"
    ///       - Case-insensitive: "Class" == "class"
    [[nodiscard]] virtual bool is_reserved_keyword(const std::string& identifier) const = 0;

    /// Sanitize an identifier to make it safe for this language
    ///
    /// Converts conflicting identifiers to safe alternatives using language-specific
    /// conventions (e.g., trailing underscore, prefix, raw identifiers).
    ///
    /// @param identifier The original identifier that may conflict
    /// @return Safe identifier
    ///
    /// @note Returns original identifier if already safe
    ///
    /// **Language-specific strategies:**
    /// - C++/Python: `class` → `class_` (trailing underscore)
    /// - Rust: `class` → `r#class` (raw identifier)
    /// - Pascal: `class` → `_class` (prefix)
    [[nodiscard]] virtual std::string sanitize_identifier(const std::string& identifier) const = 0;

    /// Suggest alternative identifiers when there's a keyword collision
    ///
    /// Provides multiple suggestions ranked by idiomatic usage.
    ///
    /// @param identifier The conflicting identifier
    /// @return Vector of suggestions (e.g., ["class_", "klass", "class_field"])
    ///
    /// @note Default implementation returns single suggestion from sanitize_identifier()
    [[nodiscard]] virtual std::vector<std::string> suggest_alternatives(
        const std::string& identifier) const {
        // Default: return sanitized version
        return {sanitize_identifier(identifier)};
    }

    /// Get all reserved keywords for this language
    /// @return Complete list of keywords
    [[nodiscard]] virtual std::vector<std::string> get_all_keywords() const = 0;

    // ========================================================================
    // Type Name Generation
    // ========================================================================

    /// Get language-specific type name for IR type
    ///
    /// Converts IR types to target language type names.
    ///
    /// @param type The IR type reference
    /// @return Type name in target language
    ///
    /// **Examples:**
    /// - C++: uint32 → `uint32_t`, struct Foo → `Foo`
    /// - Rust: uint32 → `u32`, struct Foo → `Foo`
    /// - Python: uint32 → `int`, struct Foo → `Foo`
    [[nodiscard]] virtual std::string get_type_name(const ir::type_ref& type) const = 0;

    // ========================================================================
    // Feature Support Queries (optional)
    // ========================================================================

    /// Check if renderer supports a specific feature
    ///
    /// Query optional language features for conditional code generation.
    ///
    /// @param feature Feature name (e.g., "exceptions", "generics", "async")
    /// @return true if feature is supported
    ///
    /// @note Default implementation returns false for all features
    [[nodiscard]] virtual bool supports_feature(const std::string& feature) const {
        (void)feature;  // Suppress unused parameter warning
        return false;
    }

    // ========================================================================
    // Generator Metadata & Options (for CLI driver)
    // ========================================================================

    /// Get command-line option prefix for this generator
    ///
    /// The prefix is used for generator-specific command-line options:
    /// --<prefix>-<option>=<value>
    ///
    /// @return Option prefix (e.g., "cpp", "py", "rs", "java", "ds")
    ///
    /// **Examples:**
    /// - C++: "cpp" → --cpp-exceptions=false
    /// - Python: "py" → --py-typing=strict
    /// - Rust: "rs" → --rs-edition=2021
    [[nodiscard]] virtual std::string get_option_prefix() const = 0;

    /// Get list of generator-specific command-line options
    ///
    /// Describes all options that this generator accepts, allowing the CLI
    /// driver to parse, validate, and show help for generator-specific flags.
    ///
    /// @return Vector of option descriptions (empty if no custom options)
    ///
    /// **Example:**
    /// \code
    ///   {
    ///     {"exceptions", OptionType::Bool, "Enable exception handling", "true"},
    ///     {"namespace", OptionType::String, "C++ namespace", std::nullopt}
    ///   }
    /// \endcode
    [[nodiscard]] virtual std::vector<OptionDescription> get_options() const {
        return {};
    }

    /// Set a generator-specific option value
    ///
    /// Called by the CLI driver during command-line parsing when it encounters
    /// a generator-specific option: --<prefix>-<name>=<value>
    ///
    /// @param name Option name (e.g., "exceptions", "namespace")
    /// @param value Type-safe option value (bool, int64_t, or string)
    /// @throws std::invalid_argument if option name is unknown or value is invalid
    ///
    /// **Default behavior:** Throws for any option (generator has no options)
    virtual void set_option(const std::string& name, const OptionValue& value) {
        (void)value;  // Suppress unused parameter warning
        throw std::invalid_argument(
            get_option_prefix() + " generator has no option: " + name
        );
    }

    // ========================================================================
    // Multi-File Output Interface (for CLI driver)
    // ========================================================================

    /// Generate output files from IR bundle
    ///
    /// Generators decide file structure, names, and directory layout.
    /// The driver creates output_dir and writes files returned by this method.
    ///
    /// @param bundle The IR bundle to generate code from
    /// @param output_dir Base output directory (guaranteed to exist)
    /// @return Vector of output files with full paths and content
    ///
    /// **Default implementation:**
    /// - Single file using legacy render_module() method
    /// - Filename derived from first struct/choice name or "generated"
    /// - Extension derived from get_file_extension()
    ///
    /// **Generators should override for:**
    /// - Multi-file output (e.g., header + implementation)
    /// - Package directory structure (e.g., Java package paths)
    /// - Multiple modules in one bundle
    virtual std::vector<OutputFile> generate_files(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir);
};

} // namespace datascript::codegen

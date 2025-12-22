#pragma once

#include <string>
#include <vector>
#include <set>
#include <map>
#include <sstream>
#include <optional>
#include <datascript/base_renderer.hh>
#include <datascript/codegen_commands.hh>
#include <datascript/codegen/cpp/cpp_writer_context.hh>

namespace datascript::codegen {

// ============================================================================
// Bitfield Constants
// ============================================================================

/// Maximum number of bits that fit in each integer type
namespace bitfield_limits {
    constexpr size_t UINT8_MAX_BITS = 8;
    constexpr size_t UINT16_MAX_BITS = 16;
    constexpr size_t UINT32_MAX_BITS = 32;
    constexpr size_t UINT64_MAX_BITS = 64;
}

// ============================================================================
// C++ Code Renderer
// ============================================================================

/**
 * Renders command streams as C++ code.
 *
 * Implements the BaseRenderer interface for C++20 code generation.
 * Generates C++ code with proper indentation, includes, and structure.
 *
 * Example usage:
 *   CppRenderer renderer;
 *   renderer.set_namespace("MyProtocol");
 *   renderer.render_commands(commands);
 *   std::string code = renderer.get_output();
 */
class CppRenderer : public BaseRenderer {
public:
    CppRenderer();
    ~CppRenderer() = default;

    // ========================================================================
    // Configuration
    // ========================================================================

    /**
     * Set the module context for type name resolution.
     */
    void set_module(const ir::bundle* mod);

    /**
     * Set the C++ namespace for generated code.
     */
    void set_namespace(const std::string& ns);

    /**
     * Get the output name override if set.
     */
    std::optional<std::string> get_output_name_override() const { return output_name_override_; }

    /**
     * Enable/disable safe read mode (returns bool vs exceptions).
     */
    void set_safe_read_mode(bool enabled);

    /**
     * Set error handling mode for code generation.
     */
    void set_error_handling_mode(cpp_options::error_style mode);

    // ========================================================================
    // BaseRenderer Interface Implementation
    // ========================================================================

    /// Get option prefix for C++ generator: "cpp"
    std::string get_option_prefix() const override;

    /// Get list of C++ generator options
    std::vector<OptionDescription> get_options() const override;

    /// Set a C++ generator option
    void set_option(const std::string& name, const OptionValue& value) override;

    /// Generate C++ output files (single header file)
    std::vector<OutputFile> generate_files(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir) override;

    /// Render complete IR bundle to C++ code
    std::string render_module(const ir::bundle& bundle,
                             const RenderOptions& options) override;

    /// Render IR expression to C++ expression string
    std::string render_expression(const ir::expr* expr) override;

    /// Get language metadata for C++
    LanguageMetadata get_metadata() const override;

    /// Get language name: "C++"
    std::string get_language_name() const override;

    /// Get file extension: ".cc"
    std::string get_file_extension() const override;

    /// C++ is case-sensitive
    bool is_case_sensitive() const override;

    /// C++ uses "obj" by convention for object instances
    std::string get_default_object_name() const override;

    /// Check if identifier is C++ reserved keyword
    bool is_reserved_keyword(const std::string& identifier) const override;

    /// Sanitize conflicting identifier (append underscore)
    std::string sanitize_identifier(const std::string& identifier) const override;

    /// Get all C++ keywords (77 total)
    std::vector<std::string> get_all_keywords() const override;

    /// Get C++ type name for IR type
    std::string get_type_name(const ir::type_ref& type) const override;

    // ========================================================================
    // Main Rendering
    // ========================================================================

    /**
     * Render a vector of commands to C++ code.
     */
    void render_commands(const std::vector<CommandPtr>& commands);

    /**
     * Get the generated C++ code.
     */
    std::string get_output() const;

    /**
     * Clear the output buffer.
     */
    void clear();

    // ========================================================================
    // Individual Command Rendering
    // ========================================================================

    void render_command(const Command& cmd) override;

    // Module-level commands
    void render_module_start(const ModuleStartCommand& cmd);
    void render_module_end(const ModuleEndCommand& cmd);
    void render_namespace_start(const NamespaceStartCommand& cmd);
    void render_namespace_end(const NamespaceEndCommand& cmd);
    void render_include_directive(const IncludeDirectiveCommand& cmd);

    // Constants
    void render_declare_constant(const DeclareConstantCommand& cmd);

    // Enums
    void render_start_enum(const StartEnumCommand& cmd);
    void render_declare_enum_item(const DeclareEnumItemCommand& cmd);
    void render_end_enum(const EndEnumCommand& cmd);

    // Subtypes
    void render_subtype(const SubtypeCommand& cmd);

    // Structures
    void render_start_struct(const StartStructCommand& cmd);
    void render_end_struct(const EndStructCommand& cmd);
    void render_declare_field(const DeclareFieldCommand& cmd);

    // Unions
    void render_start_union(const StartUnionCommand& cmd);
    void render_end_union(const EndUnionCommand& cmd);

    // Choices
    void render_start_choice(const StartChoiceCommand& cmd);
    void render_end_choice(const EndChoiceCommand& cmd);
    void render_start_choice_case(const StartChoiceCaseCommand& cmd);
    void render_end_choice_case(const EndChoiceCaseCommand& cmd);

    void render_start_method(const StartMethodCommand& cmd);
    void render_end_method(const EndMethodCommand& cmd);
    void render_return_value(const ReturnValueCommand& cmd);
    void render_return_expression(const ReturnExpressionCommand& cmd);

    void render_read_field(const ReadFieldCommand& cmd);
    void render_read_array_element(const ReadArrayElementCommand& cmd);
    void render_seek_to_label(const SeekToLabelCommand& cmd);
    void render_align_pointer(const AlignPointerCommand& cmd);
    void render_resize_array(const ResizeArrayCommand& cmd);
    void render_append_to_array(const AppendToArrayCommand& cmd);

    void render_start_loop(const StartLoopCommand& cmd);
    void render_start_while_loop(const StartWhileLoopCommand& cmd);
    void render_end_loop(const EndLoopCommand& cmd);

    void render_if(const IfCommand& cmd);
    void render_else(const ElseCommand& cmd);
    void render_end_if(const EndIfCommand& cmd);

    void render_start_scope(const StartScopeCommand& cmd);
    void render_end_scope(const EndScopeCommand& cmd);

    // Trial-and-error decoding (for unions)
    void render_save_position(const SavePositionCommand& cmd);
    void render_restore_position(const RestorePositionCommand& cmd);
    void render_start_try_branch(const StartTryBranchCommand& cmd);
    void render_end_try_branch(const EndTryBranchCommand& cmd);

    void render_bounds_check(const BoundsCheckCommand& cmd);
    void render_constraint_check(const ConstraintCheckCommand& cmd);

    void render_declare_variable(const DeclareVariableCommand& cmd);
    void render_assign_variable(const AssignVariableCommand& cmd);
    void render_assign_choice_wrapper(const AssignChoiceWrapperCommand& cmd);

    void render_set_error_message(const SetErrorMessageCommand& cmd);
    void render_throw_exception(const ThrowExceptionCommand& cmd);
    void render_declare_result_variable(const DeclareResultVariableCommand& cmd);
    void render_set_result_success(const SetResultSuccessCommand& cmd);

    void render_comment(const CommentCommand& cmd);
    void render_read_primitive_to_variable(const ReadPrimitiveToVariableCommand& cmd);
    void render_extract_bitfield(const ExtractBitfieldCommand& cmd);

protected:
    // ========================================================================
    // Expression Rendering (Internal - accessible for testing)
    // ========================================================================

    /**
     * Render an IR expression (internal implementation).
     * Called by public render_expression() method.
     * Protected to allow test access via subclassing.
     */
    std::string render_expr_internal(const ir::expr& expr, const ExprContext& ctx);

private:
    // ========================================================================
    // Code Generation Helpers
    // ========================================================================

    /**
     * Generate stream read call for a type.
     */
    std::string generate_read_call(const ir::type_ref* type, bool use_exceptions);

    /**
     * Get read function name for primitive types (e.g., "read_uint8", "read_uint32_le").
     */
    std::string get_read_function_name(ir::type_kind type);

    /**
     * Generate error handling code.
     */
    void generate_error_check(const std::string& error_condition,
                             const std::string& error_message,
                             bool use_exceptions);

    /**
     * Generate includes section.
     */
    void generate_includes();

    /**
     * Generate namespace opening.
     */
    void generate_namespace_open();

    /**
     * Generate namespace closing.
     */
    void generate_namespace_close();

    /**
     * Convert IR type to C++ type string.
     */
    std::string ir_type_to_cpp(const ir::type_ref* type) const;

    /**
     * Convert IR primitive type to C++ type name.
     */
    std::string get_primitive_cpp_type(const ir::type_ref* type) const;

    /**
     * Get C++ type name for bitfield based on bit count.
     * Returns smallest integer type that can hold the given number of bits.
     */
    std::string get_bitfield_cpp_type(size_t bits) const;

    /**
     * Get read function name for bitfield based on bit count.
     * Returns appropriate read function (read_uint8, read_uint16, etc.).
     */
    std::string get_bitfield_read_function(size_t bits) const;

    /**
     * Emit helper functions (read_uint8, read_uint16, ReadResult, etc.)
     */
    void emit_helper_functions();

    // Note: Expression rendering implementation delegated to CppExpressionRenderer

    // ========================================================================
    // File Generation Modes
    // ========================================================================

    /**
     * Generate library mode files (three headers).
     * Delegates to CppLibraryModeGenerator.
     */
    std::vector<OutputFile> generate_library_mode(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir);

    /**
     * Generate single header mode file (current behavior).
     */
    std::vector<OutputFile> generate_single_header_mode(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir);

    // ========================================================================
    // C++ Keywords (77 total)
    // ========================================================================

    /// Complete set of C++20 reserved keywords for validation
    static const std::set<std::string> cpp_keywords_;

    // ========================================================================
    // State
    // ========================================================================

    std::ostringstream output_;   // Output buffer backing the context
    CppWriterContext ctx_;        // High-level code generation context with block management
    ExprContext expr_context_;

    const ir::bundle* module_;  // For type name resolution
    std::string namespace_;
    bool safe_read_mode_;
    cpp_options::error_style error_handling_mode_;

    // CLI driver options
    std::optional<std::string> output_name_override_;  // Override output filename
    bool generate_enum_to_string_ = false;  // Generate enum-to-string conversion functions
    std::string output_mode_ = "single-header";  // "single-header" or "library"

    // Type name cache for performance (30-50% faster rendering for complex types)
    mutable std::map<const ir::type_ref*, std::string> type_name_cache_;

    // Track context for proper code generation
    bool in_struct_;
    bool in_method_;
    std::string current_struct_name_;

    // Track current enum context for bitmask operator generation
    std::string current_enum_name_;
    bool current_enum_is_bitmask_;
    std::vector<std::string> current_enum_items_;  // Track enum item names for to_string generation

    // Track choice case context for if/else-if chain
    bool in_choice_;
    bool first_choice_case_;

    // Track label counter for unique label variable names
    int label_counter_;

    // Track current method context for proper return value formatting
    StartMethodCommand::MethodKind current_method_kind_;
    const ir::struct_def* current_method_target_struct_;
    bool current_method_use_exceptions_;
};

} // namespace datascript::codegen

#pragma once

#include <vector>
#include <memory>
#include <string>
#include <datascript/ir.hh>
#include <datascript/codegen_commands.hh>
#include <datascript/base_renderer.hh>  // For ExprContext
#include <datascript/codegen.hh>  // For cpp_options

namespace datascript::codegen {

// ============================================================================
// Internal Helper: BuilderScope (Exception Safety)
// ============================================================================

/**
 * RAII guard for exception-safe CommandBuilder operations.
 *
 * Ensures that CommandBuilder state is properly cleaned up even if an
 * exception is thrown during a build_*() operation. This prevents memory
 * leaks in owned_expressions_ and owned_types_, and ensures consistent state.
 *
 * Usage:
 *   std::vector<CommandPtr> CommandBuilder::build_something(...) {
 *       BuilderScope scope{*this};  // RAII guard
 *       clear();
 *       // ... emit commands (may throw) ...
 *       return scope.take_commands();  // Releases ownership on success
 *   }
 *
 * If an exception is thrown before take_commands(), the destructor will
 * automatically call clear() to clean up the builder state.
 */
class CommandBuilder;  // Forward declaration

class BuilderScope {
public:
    explicit BuilderScope(CommandBuilder& builder);
    ~BuilderScope();

    // Take ownership of commands (called on success path)
    std::vector<CommandPtr> take_commands();

    // Non-copyable, non-movable
    BuilderScope(const BuilderScope&) = delete;
    BuilderScope& operator=(const BuilderScope&) = delete;
    BuilderScope(BuilderScope&&) = delete;
    BuilderScope& operator=(BuilderScope&&) = delete;

private:
    CommandBuilder& builder_;
    bool commands_taken_ = false;  // Tracks if take_commands() was called
};

// ============================================================================
// Command Builder
// ============================================================================

/**
 * Builds command streams from IR structures.
 *
 * This is the layer between IR (language-independent representation) and
 * renderers (language-specific code generation). It converts IR structures
 * into sequences of commands that can be rendered to any target language.
 *
 * Example:
 *   CommandBuilder builder;
 *   auto commands = builder.build_struct_reader(my_struct, true);
 *
 *   CppRenderer renderer;
 *   for (const auto& cmd : commands) {
 *       renderer.render(cmd);
 *   }
 */
class CommandBuilder {
public:
    CommandBuilder() = default;
    ~CommandBuilder() = default;

    // ========================================================================
    // Main Entry Points
    // ========================================================================

    /**
     * Build a complete C++ header module from IR.
     *
     * Generates all commands for a complete module including:
     * - Module start (#pragma once)
     * - Include directives
     * - Namespace declaration
     * - Constants
     * - Enums
     * - Structs with read methods
     * - Namespace close
     *
     * @param module The IR module
     * @param namespace_name The C++ namespace
     * @param opts Code generation options (for error handling mode, etc.)
     * @param use_exceptions Error handling strategy (deprecated, use opts.error_handling)
     * @return Vector of commands to render
     */
    std::vector<CommandPtr> build_module(
        const ir::bundle& module,
        const std::string& namespace_name,
        const codegen::cpp_options& opts,
        bool use_exceptions = true
    );

    /**
     * Build a complete struct reader method.
     *
     * Generates all commands needed to read a struct from a stream,
     * including method declaration, field reads, and validation.
     *
     * @param struct_def The struct definition from IR
     * @param use_exceptions If true, generate throwing code; if false, return errors
     * @return Vector of commands to render
     */
    std::vector<CommandPtr> build_struct_reader(
        const ir::struct_def& struct_def,
        bool use_exceptions
    );

    /**
     * Build standalone reader functions (not methods).
     *
     * For use in non-OOP contexts or when static functions are preferred.
     */
    std::vector<CommandPtr> build_standalone_reader(
        const ir::struct_def& struct_def,
        bool use_exceptions
    );

    /**
     * Build struct declaration (class/struct definition).
     *
     * Generates commands for the struct type declaration with all fields.
     */
    std::vector<CommandPtr> build_struct_declaration(
        const ir::struct_def& struct_def
    );

    /**
     * Build union declaration with field readers.
     *
     * Generates commands for the union type declaration with all fields
     * and read_as_<field>() methods for each field.
     */
    std::vector<CommandPtr> build_union_declaration(
        const ir::union_def& union_def,
        const codegen::cpp_options& opts
    );

    /**
     * Build choice declaration (tagged union with std::variant).
     *
     * Generates commands for the choice type declaration with:
     * - std::variant field for all case types
     * - Accessor methods for each case
     * - Templated read method with selector parameter
     */
    std::vector<CommandPtr> build_choice_declaration(
        const ir::choice_def& choice_def,
        const codegen::cpp_options& opts
    );

    // ========================================================================
    // Component Builders (used internally and by tests)
    // ========================================================================

    // Module-level commands
    void emit_module_start();
    void emit_module_end();
    void emit_namespace_start(const std::string& namespace_name);
    void emit_namespace_end(const std::string& namespace_name);
    void emit_include(const std::string& header, bool system_header = true);

    // Constants
    void emit_constant(const std::string& name, const ir::type_ref* type, const ir::expr* value);

    // Enums
    void emit_enum_start(const std::string& name, const ir::type_ref* base_type, bool is_bitmask = false);
    void emit_enum_item(const std::string& name, int64_t value, bool is_last = false);
    void emit_enum_end();

    void emit_subtype(const ir::subtype_def& subtype, const cpp_options& opts);

    /**
     * Emit struct start command.
     */
    void emit_struct_start(const std::string& name, const std::string& doc);

    /**
     * Emit struct end command.
     */
    void emit_struct_end();

    /**
     * Emit union start command with variant types.
     * @param is_optional If true, the union can be empty (all branches have conditions).
     *                    Generates std::variant<std::monostate, ...> instead of std::variant<...>
     */
    void emit_union_start(const std::string& name, const std::string& doc,
                         const std::vector<const ir::type_ref*>& case_types = {},
                         const std::vector<std::string>& case_field_names = {},
                         bool is_optional = false);

    /**
     * Emit union end command.
     */
    void emit_union_end();

    /**
     * Emit choice start command.
     */
    void emit_choice_start(const std::string& name, const std::string& doc,
                           std::vector<const ir::type_ref*> case_types,
                           std::vector<std::string> case_field_names);

    /**
     * Emit choice end command.
     */
    void emit_choice_end();

    /**
     * Emit choice case start command.
     */
    void emit_choice_case_start(std::vector<const ir::expr*> case_values,
                                const ir::field* case_field);

    /**
     * Emit choice case end command.
     */
    void emit_choice_case_end();

    /**
     * Emit method start command (language-agnostic).
     */
    void emit_method_start(
        const std::string& name,
        StartMethodCommand::MethodKind kind,
        const ir::struct_def* target_struct,
        bool use_exceptions,
        bool is_static
    );

    void emit_method_start_choice(
        const std::string& name,
        const ir::choice_def* target_choice,
        bool use_exceptions,
        bool is_static
    );

    /**
     * Emit method end command.
     */
    void emit_method_end();

    /**
     * Emit field declaration command.
     */
    void emit_field_declaration(
        const std::string& name,
        const ir::type_ref* type,
        const std::string& doc
    );

    /**
     * Emit field read command for a single field.
     *
     * Handles all field types: primitives, strings, bools, arrays, etc.
     */
    void emit_field_read(const ir::field& field, bool use_exceptions);

    /**
     * Emit optimized reading for a sequence of consecutive bitfields.
     * Returns the index of the first non-bitfield after the sequence.
     */
    size_t emit_bitfield_sequence(const std::vector<ir::field>& fields,
                                   size_t start_index,
                                   bool use_exceptions);

    /**
     * Emit variable declaration command for struct instance.
     */
    void emit_variable_declaration(
        const std::string& name,
        const ir::struct_def* struct_type
    );

    /**
     * Emit variable declaration command for regular variable with IR type.
     */
    void emit_variable_declaration(
        const std::string& name,
        const ir::type_ref* type,
        const ir::expr* init_expr = nullptr
    );

    /**
     * Emit variable assignment command.
     */
    void emit_variable_assignment(
        const std::string& name,
        const ir::expr* value_expr
    );

    /**
     * Emit bounds check command.
     */
    void emit_bounds_check(
        const std::string& value_name,
        const ir::expr* value_expr,
        const ir::expr* min_expr,
        const ir::expr* max_expr,
        const std::string& error_message,
        bool use_exceptions
    );

    /**
     * Emit constraint check command.
     */
    void emit_constraint_check(
        const ir::expr* condition,
        const std::string& error_message,
        bool use_exceptions
    );

    /**
     * Emit loop start command.
     */
    void emit_loop_start(
        const std::string& index_var,
        const ir::expr* count_expr,
        bool use_size_method = false
    );

    /**
     * Emit loop end command.
     */
    void emit_loop_end();

    /**
     * Emit return value command.
     */
    void emit_return_value(const std::string& value);

    /**
     * Emit return expression command (for user-defined functions).
     */
    void emit_return_expr(const ir::expr* expression);

    /**
     * Emit scope start command (for local variable scoping).
     */
    void emit_scope_start();

    /**
     * Emit scope end command.
     */
    void emit_scope_end();

    /**
     * Emit if statement with condition.
     */
    void emit_if(const ir::expr* condition);

    /**
     * Emit else statement.
     */
    void emit_else();

    /**
     * Emit end of if statement.
     */
    void emit_end_if();

    /**
     * Emit result variable declaration for safe mode.
     * Declares a ReadResult<T> result; variable.
     */
    void emit_result_declaration(const std::string& result_name, const ir::struct_def* struct_type);

    /**
     * Emit set result.success = true for safe mode.
     * Indicates successful read completion.
     */
    void emit_set_result_success(const std::string& result_name);

    /**
     * Emit comment.
     */
    void emit_comment(const std::string& text);

    /**
     * Get accumulated commands and clear internal state.
     */
    std::vector<CommandPtr> take_commands();

    /**
     * Clear accumulated commands.
     */
    void clear();

private:
    // ========================================================================
    // Helper Methods
    // ========================================================================

    /**
     * Emit commands to read a primitive field (uint8, int32, etc.).
     */
    void emit_primitive_field_read(
        const std::string& field_name,
        const ir::type_ref& field_type,
        bool use_exceptions
    );

    /**
     * Emit commands to read a string field.
     */
    void emit_string_field_read(
        const std::string& field_name,
        bool use_exceptions
    );

    /**
     * Emit commands to read a boolean field.
     */
    void emit_bool_field_read(
        const std::string& field_name,
        bool use_exceptions
    );

    /**
     * Emit constraint validation for a field.
     */
    void emit_field_constraints(
        const ir::field& field,
        bool use_exceptions
    );

    /**
     * Emit commands to read a choice-typed field.
     */
    void emit_choice_field_read(
        const ir::field& field,
        bool use_exceptions
    );

    /**
     * Emit commands to read an array field.
     */
    void emit_array_field_read(
        const std::string& field_name,
        const ir::type_ref& field_type,
        bool use_exceptions
    );

    /**
     * Emit commands to read a fixed-size array.
     */
    void emit_fixed_array_read(
        const std::string& field_name,
        const ir::type_ref& element_type,
        uint64_t array_size,
        bool use_exceptions
    );

    /**
     * Emit commands to read a fixed-size array with size expression (preserves constant names).
     */
    void emit_fixed_array_read_expr(
        const std::string& field_name,
        const ir::type_ref& element_type,
        const ir::expr* size_expr,
        bool use_exceptions
    );

    /**
     * Emit commands to read a variable-size array.
     */
    void emit_variable_array_read(
        const std::string& field_name,
        const ir::type_ref& element_type,
        const ir::expr* size_expr,
        bool use_exceptions
    );

    /**
     * Emit commands to read a ranged array with bounds checking.
     */
    void emit_ranged_array_read(
        const std::string& field_name,
        const ir::type_ref& element_type,
        const ir::expr* size_expr,
        const ir::expr* min_expr,
        const ir::expr* max_expr,
        bool use_exceptions
    );

    /**
     * Emit commands to read an unbounded array (reads until end of data).
     */
    void emit_unbounded_array_read(
        const std::string& field_name,
        const ir::type_ref& element_type,
        bool use_exceptions
    );

    /**
     * Emit commands to read a single array element.
     */
    void emit_array_element_read(
        const std::string& element_var,
        const ir::type_ref& element_type,
        bool use_exceptions
    );

    // ========================================================================
    // Module Building Helpers (for decomposed build_module)
    // ========================================================================

    /**
     * Emit module constants section.
     */
    void emit_module_constants(const ir::bundle& module);

    /**
     * Emit module enums section.
     */
    void emit_module_enums(const ir::bundle& module);

    /**
     * Emit module subtypes section.
     */
    void emit_module_subtypes(const ir::bundle& module, const cpp_options& opts);

    /**
     * Emit module structs and choices in topologically sorted order.
     */
    void emit_module_structs_and_choices(const ir::bundle& module, const cpp_options& opts);

    /**
     * Emit module unions section.
     */
    void emit_module_unions(const ir::bundle& module, const cpp_options& opts);

    // ========================================================================
    // State
    // ========================================================================

    // Accumulated commands
    std::vector<CommandPtr> commands_;

    // Owned expressions (for temporary expressions created by builder)
    std::vector<std::unique_ptr<ir::expr>> owned_expressions_;

    // Owned type refs (for temporary types created by builder)
    std::vector<std::unique_ptr<ir::type_ref>> owned_types_;

    // Current expression context (for rendering expressions)
    ExprContext expr_context_;

    // Nesting depth (for proper scope handling)
    int scope_depth_ = 0;

    // Module constraints (for constraint validation)
    const std::vector<ir::constraint_def>* constraints_ = nullptr;

    // Module choices (for choice field reading)
    const std::vector<ir::choice_def>* choices_ = nullptr;

    // ========================================================================
    // Expression Ownership
    // ========================================================================

    /**
     * Create and own an expression (for temporary expressions).
     * Returns a pointer to the owned expression.
     */
    const ir::expr* create_expression(ir::expr&& expr);

    /**
     * Create and own a type_ref (for temporary types).
     * Returns a pointer to the owned type.
     */
    const ir::type_ref* create_type(ir::type_ref&& type);
};

} // namespace datascript::codegen

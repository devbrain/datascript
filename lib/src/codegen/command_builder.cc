//
// Command Builder Implementation
//
// Converts IR structures to command streams for language-specific rendering.
//

#include <datascript/command_builder.hh>
#include <sstream>
#include <iostream>

namespace datascript::codegen {

// ============================================================================
// BuilderScope Implementation (Exception Safety)
// ============================================================================

BuilderScope::BuilderScope(CommandBuilder& builder)
    : builder_(builder), commands_taken_(false) {
}

BuilderScope::~BuilderScope() {
    if (!commands_taken_) {
        // Exception was thrown, clean up builder state
        builder_.clear();
    }
}

std::vector<CommandPtr> BuilderScope::take_commands() {
    commands_taken_ = true;
    return builder_.take_commands();
}

// ============================================================================
// Helper Functions for Type Dispatch (language-agnostic)
// ============================================================================

namespace {
    /**
     * Helper struct to determine which error handling modes to generate.
     *
     * This eliminates code duplication where the same boolean checks appear
     * multiple times throughout the codebase.
     */
    struct ErrorHandlingModes {
        bool generate_safe;   // Generate ReadResult<T> safe mode methods
        bool generate_throw;  // Generate exception-based methods

        static ErrorHandlingModes from_options(const cpp_options& opts) {
            return {
                .generate_safe = (opts.error_handling == cpp_options::both ||
                                 opts.error_handling == cpp_options::results_only),
                .generate_throw = (opts.error_handling == cpp_options::both ||
                                  opts.error_handling == cpp_options::exceptions_only)
            };
        }
    };

    bool is_array_type(const ir::type_ref& type) {
        return type.element_type != nullptr;
    }

    bool is_primitive_type(const ir::type_ref& type) {
        switch (type.kind) {
            case ir::type_kind::uint8:
            case ir::type_kind::uint16:
            case ir::type_kind::uint32:
            case ir::type_kind::uint64:
            case ir::type_kind::uint128:
            case ir::type_kind::int8:
            case ir::type_kind::int16:
            case ir::type_kind::int32:
            case ir::type_kind::int64:
            case ir::type_kind::int128:
                return true;
            default:
                return false;
        }
    }

}

// ============================================================================
// Main Entry Points
// ============================================================================

std::vector<CommandPtr> CommandBuilder::build_struct_reader(
    const ir::struct_def& struct_def,
    bool use_exceptions
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    clear();

    // Set up expression context for struct method
    expr_context_.in_struct_method = true;
    expr_context_.object_name = "obj";
    expr_context_.use_safe_reads = !use_exceptions;

    // Start struct declaration
    emit_struct_start(struct_def.name, struct_def.documentation);

    // Emit field declarations
    for (const auto& field : struct_def.fields) {
        emit_field_declaration(field.name, &field.type, "");  // Pass IR type, not C++ string
    }

    // Start read() method - renderer will format signature based on method kind and error handling
    // Methods are static class methods
    emit_method_start(use_exceptions ? "read" : "read_safe",
                     StartMethodCommand::MethodKind::StructReader,
                     &struct_def, use_exceptions, true);  // true = static

    // Declare local variables
    if (!use_exceptions) {
        // Safe mode: ReadResult<StructName> result;
        emit_result_declaration("result", &struct_def);
    }

    // Declare object variable: StructName obj;
    emit_variable_declaration("obj", &struct_def);

    // Emit field reads (with special handling for consecutive bitfields)
    size_t i = 0;
    while (i < struct_def.fields.size()) {
        const auto& field = struct_def.fields[i];

        // Initialize field with default value if specified
        if (field.default_value) {
            emit_comment("Initialize field '" + field.name + "' with default value");
            emit_variable_assignment("obj." + field.name, &field.default_value.value());
        }

        // Check if this starts a sequence of bitfields
        if (field.type.kind == ir::type_kind::bitfield && field.type.bit_width.has_value()) {
            // Batch consecutive bitfields together
            i = emit_bitfield_sequence(struct_def.fields, i, use_exceptions);
        } else {
            // Normal field read
            emit_field_read(field, use_exceptions);
            emit_field_constraints(field, use_exceptions);
            i++;
        }
    }

    // Return the object
    if (use_exceptions) {
        emit_return_value("obj");
    } else {
        // Safe mode: assign to result.value, set success, and return
        // result.value = obj;
        ir::expr obj_expr;
        obj_expr.type = ir::expr::parameter_ref;
        obj_expr.ref_name = "obj";
        const ir::expr* obj_ref = create_expression(std::move(obj_expr));
        emit_variable_assignment("result.value", obj_ref);

        // result.success = true;
        emit_set_result_success("result");

        // return result;
        emit_return_value("result");
    }

    emit_method_end();
    emit_struct_end();

    return scope.take_commands();  // Success: transfer command ownership
}

std::vector<CommandPtr> CommandBuilder::build_standalone_reader(
    const ir::struct_def& struct_def,
    bool use_exceptions
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    clear();

    // Set up expression context for standalone function
    expr_context_.in_struct_method = false;
    expr_context_.use_safe_reads = !use_exceptions;

    // Start function - renderer will format signature based on method kind and error handling
    emit_method_start("read_" + struct_def.name,
                     StartMethodCommand::MethodKind::StandaloneReader,
                     &struct_def, use_exceptions, true);

    // Declare result variable - renderer will format type name
    emit_variable_declaration("result", &struct_def);

    // Emit field reads (using result.field)
    expr_context_.object_name = "result";
    for (const auto& field : struct_def.fields) {
        // Handle conditional fields
        if (field.condition == ir::field::runtime && field.runtime_condition.has_value()) {
            emit_comment("Conditional field: " + field.name);
            emit_if(&field.runtime_condition.value());
            emit_field_read(field, use_exceptions);
            emit_field_constraints(field, use_exceptions);
            emit_end_if();
        } else if (field.condition == ir::field::always) {
            emit_field_read(field, use_exceptions);
            emit_field_constraints(field, use_exceptions);
        }
        // Skip fields with condition == never
    }

    // Return result - renderer will wrap in optional/result if needed based on use_exceptions
    emit_return_value("result");
    emit_method_end();

    return scope.take_commands();  // Success: transfer command ownership
}

std::vector<CommandPtr> CommandBuilder::build_struct_declaration(
    const ir::struct_def& struct_def
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    clear();

    emit_struct_start(struct_def.name, struct_def.documentation);

    for (const auto& field : struct_def.fields) {
        emit_field_declaration(field.name, &field.type, "");  // Pass IR type, not C++ string
    }

    emit_struct_end();

    return scope.take_commands();  // Success: transfer command ownership
}

std::vector<CommandPtr> CommandBuilder::build_union_declaration(
    const ir::union_def& union_def,
    const codegen::cpp_options& opts
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    clear();

    // Collect case types and field names for std::variant
    std::vector<const ir::type_ref*> case_types;
    std::vector<std::string> case_field_names;

    for (const auto& union_case : union_def.cases) {
        for (const auto& field : union_case.fields) {
            case_types.push_back(&field.type);
            case_field_names.push_back(field.name);
        }
    }

    // Detect optional unions: all cases have runtime conditions
    // According to DataScript spec, if all branches have constraints and none match,
    // the union should be allowed to be empty (represented as std::monostate)
    bool is_optional = true;
    for (const auto& union_case : union_def.cases) {
        if (!union_case.condition.has_value()) {
            // Found an unconditional branch (default case)
            is_optional = false;
            break;
        }
    }

    // Emit union declaration with variant information
    // Note: Wrapper structs for anonymous blocks are already in the IR's structs vector
    // and will be emitted separately by the codegen driver
    emit_union_start(union_def.name, union_def.documentation, case_types, case_field_names, is_optional);

    // Note: We don't emit individual field declarations anymore since
    // the union is now a std::variant wrapped in a struct

    // Generate read_as_<field>() methods for each union case field
    auto modes = ErrorHandlingModes::from_options(opts);

    for (const auto& union_case : union_def.cases) {
        for (const auto& field : union_case.fields) {
            // Generate safe mode reader: static ReadResult<field_type> read_as_<field>_safe(...)
            if (modes.generate_safe) {
                std::string method_name = "read_as_" + field.name + "_safe";
                commands_.push_back(std::make_unique<StartMethodCommand>(
                    method_name, &field.type, false, true  // false=safe, true=static
                ));

                // Read the field and return it
                emit_field_read(field, false);
                emit_field_constraints(field, false);
                emit_return_value("result");
                emit_method_end();
            }

            // Generate exception mode reader: static field_type read_as_<field>(...)
            if (modes.generate_throw) {
                std::string method_name = "read_as_" + field.name;
                commands_.push_back(std::make_unique<StartMethodCommand>(
                    method_name, &field.type, true, true  // true=exceptions, true=static
                ));

                // Read the field and return it directly
                emit_field_read(field, true);
                emit_field_constraints(field, true);
                emit_return_value(field.name);
                emit_method_end();
            }
        }
    }

    emit_union_end();

    return scope.take_commands();  // Success: transfer command ownership
}

std::vector<CommandPtr> CommandBuilder::build_choice_declaration(
    const ir::choice_def& choice_def,
    const codegen::cpp_options& opts
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    // Don't clear() here - we're building commands to be appended to existing list
    // (BuilderScope still provides exception safety without clear())

    // Collect all case types and field names for the std::variant
    std::vector<const ir::type_ref*> case_types;
    std::vector<std::string> case_field_names;
    for (const auto& case_item : choice_def.cases) {
        case_types.push_back(&case_item.case_field.type);
        case_field_names.push_back(case_item.case_field.name);
    }

    emit_choice_start(choice_def.name, choice_def.documentation, case_types, case_field_names);

    // Generate read methods
    auto modes = ErrorHandlingModes::from_options(opts);

    // Generate safe mode reader: template<typename SelectorType> static ReadResult<Choice> read_safe(...)
    if (modes.generate_safe) {
        emit_method_start("read_safe", StartMethodCommand::MethodKind::ChoiceReader,
                         nullptr, false, true);

        // Declare the choice object
        // TODO: Declare choice object for safe mode
        // (For now, safe mode is not fully implemented - exception mode is the primary path)

        // Generate if/else-if chain for each case
        size_t case_index = 0;
        for (const auto& case_item : choice_def.cases) {
            std::vector<const ir::expr*> case_vals;
            for (const auto& val : case_item.case_values) {
                case_vals.push_back(&val);
            }

            emit_choice_case_start(case_vals, &case_item.case_field);

            // Declare local variable for the field
            emit_variable_declaration(case_item.case_field.name, &case_item.case_field.type);

            // Read the field for this case
            emit_field_read(case_item.case_field, false);
            emit_field_constraints(case_item.case_field, false);

            // Assign to variant using wrapper struct: obj.data = Case{i}_{field_name}{field_value};
            std::string wrapper_name = "Case" + std::to_string(case_index) + "_" + case_item.case_field.name;
            commands_.push_back(std::make_unique<AssignChoiceWrapperCommand>(
                "obj.data", wrapper_name, case_item.case_field.name
            ));

            emit_choice_case_end();
            case_index++;
        }

        // Add else clause for invalid selector
        emit_else();

        // Create a string literal expression for the error message
        ir::expr error_msg;
        error_msg.type = ir::expr::literal_string;
        error_msg.string_value = "Invalid selector value for choice " + choice_def.name;
        const ir::expr* error_expr = create_expression(std::move(error_msg));

        commands_.push_back(std::make_unique<SetErrorMessageCommand>(error_expr));
        emit_return_value("result");
        emit_end_if();

        // Mark success and return
        emit_set_result_success("result");
        emit_return_value("result");
        emit_method_end();
    }

    // Generate exception mode reader (if needed)
    if (modes.generate_throw) {
        emit_method_start("read", StartMethodCommand::MethodKind::ChoiceReader,
                         nullptr, true, true);

        // Declare the choice object
        commands_.push_back(std::make_unique<DeclareVariableCommand>("obj", choice_def.name));

        // Generate if/else-if chain for each case
        size_t case_index = 0;
        for (const auto& case_item : choice_def.cases) {
            std::vector<const ir::expr*> case_vals;
            for (const auto& val : case_item.case_values) {
                case_vals.push_back(&val);
            }

            emit_choice_case_start(case_vals, &case_item.case_field);

            // Declare local variable for the field
            emit_variable_declaration(case_item.case_field.name, &case_item.case_field.type);

            // Read the field for this case
            emit_field_read(case_item.case_field, true);  // true = throw mode
            emit_field_constraints(case_item.case_field, true);

            // Assign to variant using wrapper struct: obj.data = Case{i}_{field_name}{field_value};
            std::string wrapper_name = "Case" + std::to_string(case_index) + "_" + case_item.case_field.name;
            commands_.push_back(std::make_unique<AssignChoiceWrapperCommand>(
                "obj.data", wrapper_name, case_item.case_field.name
            ));

            emit_choice_case_end();
            case_index++;
        }

        // Add else clause for invalid selector
        emit_else();

        // Create error message and throw
        ir::expr error_msg;
        error_msg.type = ir::expr::literal_string;
        error_msg.string_value = "Invalid selector value for choice " + choice_def.name;
        const ir::expr* error_expr = create_expression(std::move(error_msg));
        commands_.push_back(std::make_unique<ThrowExceptionCommand>("std::runtime_error", error_expr));

        emit_end_if();

        // Return the populated object
        emit_return_value("obj");
        emit_method_end();
    }

    emit_choice_end();

    return scope.take_commands();  // Success: transfer command ownership
}

// ============================================================================
// Component Builders
// ============================================================================

void CommandBuilder::emit_struct_start(const std::string& name, const std::string& doc) {
    commands_.push_back(std::make_unique<StartStructCommand>(name, doc));
}

void CommandBuilder::emit_struct_end() {
    commands_.push_back(std::make_unique<EndStructCommand>());
}

void CommandBuilder::emit_union_start(const std::string& name, const std::string& doc,
                                      const std::vector<const ir::type_ref*>& case_types,
                                      const std::vector<std::string>& case_field_names,
                                      bool is_optional) {
    commands_.push_back(std::make_unique<StartUnionCommand>(name, doc, case_types, case_field_names, is_optional));
}

void CommandBuilder::emit_union_end() {
    commands_.push_back(std::make_unique<EndUnionCommand>());
}

void CommandBuilder::emit_choice_start(const std::string& name, const std::string& doc,
                                       std::vector<const ir::type_ref*> case_types,
                                       std::vector<std::string> case_field_names) {
    commands_.push_back(std::make_unique<StartChoiceCommand>(name, doc, std::move(case_types), std::move(case_field_names)));
}

void CommandBuilder::emit_choice_end() {
    commands_.push_back(std::make_unique<EndChoiceCommand>());
}

void CommandBuilder::emit_choice_case_start(std::vector<const ir::expr*> case_values,
                                            const ir::field* case_field) {
    commands_.push_back(std::make_unique<StartChoiceCaseCommand>(std::move(case_values), case_field));
}

void CommandBuilder::emit_choice_case_end() {
    commands_.push_back(std::make_unique<EndChoiceCaseCommand>());
}

void CommandBuilder::emit_method_start(
    const std::string& name,
    StartMethodCommand::MethodKind kind,
    const ir::struct_def* target_struct,
    bool use_exceptions,
    bool is_static
) {
    commands_.push_back(std::make_unique<StartMethodCommand>(
        name, kind, target_struct, use_exceptions, is_static
    ));
}

void CommandBuilder::emit_method_end() {
    commands_.push_back(std::make_unique<EndMethodCommand>());
}

void CommandBuilder::emit_field_declaration(
    const std::string& name,
    const ir::type_ref* type,
    const std::string& doc
) {
    commands_.push_back(std::make_unique<DeclareFieldCommand>(name, type, doc));
}

size_t CommandBuilder::emit_bitfield_sequence(
    const std::vector<ir::field>& fields,
    size_t start_index,
    bool use_exceptions
) {
    // Group consecutive bitfields and read them efficiently
    emit_comment("Reading bitfield sequence");

    // Find all consecutive bitfields
    size_t bit_offset = 0;
    size_t end_index = start_index;

    while (end_index < fields.size() &&
           fields[end_index].type.kind == ir::type_kind::bitfield &&
           fields[end_index].type.bit_width.has_value()) {
        bit_offset += *fields[end_index].type.bit_width;
        end_index++;
    }

    // Calculate how many bytes we need
    size_t num_bytes = (bit_offset + 7) / 8;

    // Read the bytes containing all bitfields
    for (size_t byte_idx = 0; byte_idx < num_bytes; byte_idx++) {
        std::string var_name = "_bitfield_byte" + std::to_string(byte_idx);

        // Declare variable: uint8_t _bitfield_byte0;
        ir::type_ref uint8_type;
        uint8_type.kind = ir::type_kind::uint8;
        const ir::type_ref* type_ptr = create_type(std::move(uint8_type));
        emit_variable_declaration(var_name, type_ptr);

        // Read byte into variable using language-agnostic command
        commands_.push_back(std::make_unique<ReadPrimitiveToVariableCommand>(
            var_name, ir::type_kind::uint8, use_exceptions
        ));
    }

    // Extract each bitfield from the bytes
    bit_offset = 0;
    for (size_t i = start_index; i < end_index; i++) {
        const auto& field = fields[i];
        size_t bit_width = *field.type.bit_width;

        // Calculate which byte and bit offset within that byte
        size_t byte_index = bit_offset / 8;
        size_t bit_in_byte = bit_offset % 8;

        // Determine target field name
        std::string target = expr_context_.in_struct_method
            ? expr_context_.object_name + "." + field.name
            : field.name;

        // Generate source variable name
        std::string source_var = "_bitfield_byte" + std::to_string(byte_index);

        // Calculate mask
        uint64_t mask = (1ULL << bit_width) - 1;

        // Check if bitfield spans multiple bytes
        if (bit_in_byte + bit_width > 8) {
            // Multi-byte bitfield
            size_t bits_in_first_byte = 8 - bit_in_byte;
            uint64_t first_mask = (1ULL << bits_in_first_byte) - 1;
            uint64_t second_mask = (1ULL << (bit_width - bits_in_first_byte)) - 1;
            std::string source_var2 = "_bitfield_byte" + std::to_string(byte_index + 1);

            commands_.push_back(std::make_unique<ExtractBitfieldCommand>(
                target, source_var, source_var2,
                bit_in_byte, bit_width, mask,
                bits_in_first_byte, first_mask, second_mask
            ));
        } else {
            // Single-byte bitfield
            commands_.push_back(std::make_unique<ExtractBitfieldCommand>(
                target, source_var, bit_in_byte, bit_width, mask
            ));
        }

        bit_offset += bit_width;
    }

    return end_index;
}

void CommandBuilder::emit_field_read(const ir::field& field, bool use_exceptions) {
    // Handle label directive - seek to position before reading field
    if (field.label.has_value()) {
        emit_comment("Seek to labeled position for field '" + field.name + "'");
        commands_.push_back(std::make_unique<SeekToLabelCommand>(
            &field.label.value(), use_exceptions
        ));
    }

    // Handle alignment directive - align pointer before reading field
    if (field.alignment.has_value()) {
        emit_comment("Align to " + std::to_string(field.alignment.value()) + "-byte boundary");
        commands_.push_back(std::make_unique<AlignPointerCommand>(
            field.alignment.value(), use_exceptions
        ));
    }

    // Dispatch based on field type
    // Check arrays first since they may have primitive element types
    if (is_array_type(field.type)) {
        emit_array_field_read(field.name, field.type, use_exceptions);
    } else if (is_primitive_type(field.type)) {
        emit_primitive_field_read(field.name, field.type, use_exceptions);
    } else if (field.type.kind == ir::type_kind::string) {
        emit_string_field_read(field.name, use_exceptions);
    } else if (field.type.kind == ir::type_kind::boolean) {
        emit_bool_field_read(field.name, use_exceptions);
    } else if (field.type.kind == ir::type_kind::choice_type) {
        // Choice type - needs selector evaluation
        emit_choice_field_read(field, use_exceptions);
    } else {
        // User-defined type or unknown - pass IR type
        commands_.push_back(std::make_unique<ReadFieldCommand>(
            field.name, &field.type, use_exceptions
        ));
    }

    // Note: Inline constraint validation is handled by emit_field_constraints()
    // which is called after emit_field_read() in build_struct_reader()
}

void CommandBuilder::emit_variable_declaration(
    const std::string& name,
    const ir::struct_def* struct_type
) {
    commands_.push_back(std::make_unique<DeclareVariableCommand>(
        name, struct_type
    ));
}

void CommandBuilder::emit_variable_declaration(
    const std::string& name,
    const ir::type_ref* type,
    const ir::expr* init_expr
) {
    commands_.push_back(std::make_unique<DeclareVariableCommand>(
        name, type, init_expr
    ));
}

void CommandBuilder::emit_variable_assignment(
    const std::string& name,
    const ir::expr* value_expr
) {
    commands_.push_back(std::make_unique<AssignVariableCommand>(
        name, value_expr
    ));
}

void CommandBuilder::emit_bounds_check(
    const std::string& value_name,
    const ir::expr* value_expr,
    const ir::expr* min_expr,
    const ir::expr* max_expr,
    const std::string& error_message,
    bool use_exceptions
) {
    commands_.push_back(std::make_unique<BoundsCheckCommand>(
        value_name, value_expr, min_expr, max_expr,
        error_message, use_exceptions
    ));
}

void CommandBuilder::emit_constraint_check(
    const ir::expr* condition,
    const std::string& error_message,
    bool use_exceptions
) {
    commands_.push_back(std::make_unique<ConstraintCheckCommand>(
        condition, error_message, use_exceptions, expr_context_.current_field_name
    ));
}

void CommandBuilder::emit_field_constraints(
    const ir::field& field,
    bool use_exceptions
) {
    // Check inline constraint first (field: constraint_expr)
    if (field.inline_constraint) {
        emit_comment("Validate inline constraint for field '" + field.name + "'");
        std::string error_msg = "Constraint violation for field '" + field.name + "'";
        emit_constraint_check(&field.inline_constraint.value(), error_msg, use_exceptions);
    }

    // Then check named constraints
    if (!constraints_ || field.constraints.empty()) {
        return;
    }

    // Process each constraint application
    for (const auto& app : field.constraints) {
        if (app.constraint_index >= constraints_->size()) {
            continue;  // Invalid index, skip
        }

        const auto& constraint = (*constraints_)[app.constraint_index];

        // Emit comment for constraint
        emit_comment("Validate constraint: " + constraint.name);

        // Build error message
        std::string error_msg = "Constraint '" + constraint.name + "' violated: " +
                                constraint.error_message_template;

        // Emit the constraint check with the condition
        // Note: The constraint condition may reference parameters which need to be
        // substituted with actual field values or argument expressions
        emit_constraint_check(&constraint.condition, error_msg, use_exceptions);
    }
}

void CommandBuilder::emit_choice_field_read(
    const ir::field& field,
    bool use_exceptions
) {
    // Get the choice definition
    if (!choices_ || !field.type.type_index.has_value()) {
        // Fall back to generic read if we don't have the choice info
        commands_.push_back(std::make_unique<ReadFieldCommand>(
            field.name, &field.type, use_exceptions
        ));
        return;
    }

    size_t choice_idx = field.type.type_index.value();
    if (choice_idx >= choices_->size()) {
        // Invalid index, fall back
        commands_.push_back(std::make_unique<ReadFieldCommand>(
            field.name, &field.type, use_exceptions
        ));
        return;
    }

    const auto& choice_def = (*choices_)[choice_idx];

    // Only generate selector evaluation if the field doesn't have explicit selector arguments
    // (parameterized choice instantiation provides selector args directly)
    if (field.type.choice_selector_args.empty()) {
        // Extract the field name from the selector expression
        std::string selector_field_name;
        if (choice_def.selector.type == ir::expr::field_ref) {
            selector_field_name = choice_def.selector.ref_name;
        } else if (choice_def.selector.type == ir::expr::parameter_ref) {
            selector_field_name = choice_def.selector.ref_name;
        } else {
            // Unknown selector type - use a placeholder
            selector_field_name = "unknown_selector";
        }

        // Emit comment about evaluating selector
        std::string selector_comment = "Evaluate choice selector: " +
            expr_context_.object_name + "." + selector_field_name;
        emit_comment(selector_comment);

        // Declare selector_value variable with initialization
        // Create a field reference expression for the selector field
        ir::expr selector_expr;
        selector_expr.type = ir::expr::field_ref;
        selector_expr.ref_name = selector_field_name;  // Field name (e.g., "msg_type")
        const ir::expr* selector_ptr = create_expression(std::move(selector_expr));

        // Emit: auto selector_value = obj.msg_type;
        // Use nullptr for type (will render as "auto")
        commands_.push_back(std::make_unique<DeclareVariableCommand>("selector_value", nullptr, selector_ptr));
    }

    // For the choice field read, use ReadFieldCommand
    // The renderer will handle calling the choice's static read method
    commands_.push_back(std::make_unique<ReadFieldCommand>(
        field.name, &field.type, use_exceptions
    ));
}

void CommandBuilder::emit_loop_start(
    const std::string& index_var,
    const ir::expr* count_expr,
    bool use_size_method
) {
    commands_.push_back(std::make_unique<StartLoopCommand>(
        index_var, count_expr, use_size_method
    ));
    scope_depth_++;
}

void CommandBuilder::emit_loop_end() {
    commands_.push_back(std::make_unique<EndLoopCommand>());
    scope_depth_--;
}

void CommandBuilder::emit_return_value(const std::string& value) {
    commands_.push_back(std::make_unique<ReturnValueCommand>(value));
}

void CommandBuilder::emit_return_expr(const ir::expr* expression) {
    commands_.push_back(std::make_unique<ReturnExpressionCommand>(expression));
}

void CommandBuilder::emit_scope_start() {
    commands_.push_back(std::make_unique<StartScopeCommand>());
    scope_depth_++;
}

void CommandBuilder::emit_scope_end() {
    commands_.push_back(std::make_unique<EndScopeCommand>());
    scope_depth_--;
}

void CommandBuilder::emit_if(const ir::expr* condition) {
    commands_.push_back(std::make_unique<IfCommand>(condition));
}

void CommandBuilder::emit_else() {
    commands_.push_back(std::make_unique<ElseCommand>());
}

void CommandBuilder::emit_end_if() {
    commands_.push_back(std::make_unique<EndIfCommand>());
}

void CommandBuilder::emit_result_declaration(const std::string& result_name, const ir::struct_def* struct_type) {
    commands_.push_back(std::make_unique<DeclareResultVariableCommand>(result_name, struct_type));
}

void CommandBuilder::emit_set_result_success(const std::string& result_name) {
    commands_.push_back(std::make_unique<SetResultSuccessCommand>(result_name));
}

void CommandBuilder::emit_comment(const std::string& text) {
    commands_.push_back(std::make_unique<CommentCommand>(text));
}

std::vector<CommandPtr> CommandBuilder::take_commands() {
    std::vector<CommandPtr> result = std::move(commands_);
    commands_.clear();
    return result;
}

void CommandBuilder::clear() {
    commands_.clear();
    owned_expressions_.clear();
    owned_types_.clear();
    scope_depth_ = 0;
}

// ============================================================================
// Helper Methods - Field Type Reads
// ============================================================================

void CommandBuilder::emit_primitive_field_read(
    const std::string& field_name,
    const ir::type_ref& field_type,
    bool use_exceptions
) {
    // Pass IR type, not C++ type string
    commands_.push_back(std::make_unique<ReadFieldCommand>(
        field_name, &field_type, use_exceptions
    ));
}

void CommandBuilder::emit_string_field_read(
    const std::string& field_name,
    bool use_exceptions
) {
    // Create and own a string type_ref
    ir::type_ref string_type;
    string_type.kind = ir::type_kind::string;
    const ir::type_ref* type_ptr = create_type(std::move(string_type));

    commands_.push_back(std::make_unique<ReadFieldCommand>(
        field_name, type_ptr, use_exceptions
    ));
}

void CommandBuilder::emit_bool_field_read(
    const std::string& field_name,
    bool use_exceptions
) {
    // Create and own a boolean type_ref
    ir::type_ref bool_type;
    bool_type.kind = ir::type_kind::boolean;
    const ir::type_ref* type_ptr = create_type(std::move(bool_type));

    commands_.push_back(std::make_unique<ReadFieldCommand>(
        field_name, type_ptr, use_exceptions
    ));
}

void CommandBuilder::emit_array_field_read(
    const std::string& field_name,
    const ir::type_ref& field_type,
    bool use_exceptions
) {
    // Determine array type based on available information
    // Support both explicit kind checking and legacy detection based on field presence

    if (field_type.kind == ir::type_kind::array_ranged) {
        // Ranged array: T[count in min..max]
        emit_ranged_array_read(
            field_name,
            *field_type.element_type,
            field_type.array_size_expr.get(),
            field_type.min_size_expr.get(),
            field_type.max_size_expr.get(),
            use_exceptions
        );
    } else if (field_type.kind == ir::type_kind::array_fixed || field_type.array_size.has_value()) {
        // Fixed-size array: T[N] or T[CONSTANT]
        // Prefer using the expression if available (preserves constant names)
        if (field_type.array_size_expr) {
            emit_fixed_array_read_expr(
                field_name,
                *field_type.element_type,
                field_type.array_size_expr.get(),
                use_exceptions
            );
        } else {
            emit_fixed_array_read(
                field_name,
                *field_type.element_type,
                field_type.array_size.value(),
                use_exceptions
            );
        }
    } else if ((field_type.kind == ir::type_kind::array_variable && field_type.array_size_expr) ||
               (field_type.kind != ir::type_kind::array_variable && field_type.array_size_expr)) {
        // Variable-size array: T[count]
        // Both cases: kind is array_variable with size expr, or legacy with size expr
        emit_variable_array_read(
            field_name,
            *field_type.element_type,
            field_type.array_size_expr.get(),
            use_exceptions
        );
    } else {
        // Unbounded array: T[] - read until end of data
        // This handles: array_variable kind with no size expr
        emit_unbounded_array_read(
            field_name,
            *field_type.element_type,
            use_exceptions
        );
    }
}

void CommandBuilder::emit_fixed_array_read(
    const std::string& field_name,
    const ir::type_ref& element_type,
    uint64_t array_size,
    bool use_exceptions
) {
    // Create owned expression for array size
    ir::expr size_literal;
    size_literal.type = ir::expr::literal_int;
    size_literal.int_value = static_cast<int64_t>(array_size);
    const ir::expr* size_expr = create_expression(std::move(size_literal));

    emit_fixed_array_read_expr(field_name, element_type, size_expr, use_exceptions);
}

void CommandBuilder::emit_fixed_array_read_expr(
    const std::string& field_name,
    const ir::type_ref& element_type,
    const ir::expr* size_expr,
    bool use_exceptions
) {
    // Fixed-size arrays (std::array) don't need resize - they're already the right size
    // No resize command needed

    // Loop to read elements
    std::string index_var = "i";
    emit_loop_start(index_var, size_expr);

    // Read array element - qualify with object name if in struct context
    std::string qualified_field = expr_context_.object_name.empty()
        ? field_name
        : expr_context_.object_name + "." + field_name;
    std::string element_expr = qualified_field + "[" + index_var + "]";
    emit_array_element_read(element_expr, element_type, use_exceptions);

    emit_loop_end();
}

void CommandBuilder::emit_variable_array_read(
    const std::string& field_name,
    const ir::type_ref& element_type,
    const ir::expr* size_expr,
    bool use_exceptions
) {
    // Resize array to size_expr
    commands_.push_back(std::make_unique<ResizeArrayCommand>(
        field_name, size_expr
    ));

    // Loop to read elements using the same size expression
    std::string index_var = "i";
    emit_loop_start(index_var, size_expr, false);  // false = don't use .size(), use expression directly

    // Read array element - qualify with object name if in struct context
    std::string qualified_field = expr_context_.object_name.empty()
        ? field_name
        : expr_context_.object_name + "." + field_name;
    std::string element_expr = qualified_field + "[" + index_var + "]";
    emit_array_element_read(element_expr, element_type, use_exceptions);

    emit_loop_end();
}

void CommandBuilder::emit_ranged_array_read(
    const std::string& field_name,
    const ir::type_ref& element_type,
    const ir::expr* size_expr,
    const ir::expr* min_expr,
    const ir::expr* max_expr,
    bool use_exceptions
) {
    // Ranged array: T[min..max] where size = max - min
    // 1. Declare variable for array size (size_expr is already max - min from IR builder)
    // 2. Bounds check (ensure size is within valid range)
    // 3. Resize array
    // 4. Read elements in loop

    // Create size_t type for the array_size variable
    ir::type_ref size_t_type;
    size_t_type.kind = ir::type_kind::uint64;  // size_t maps to uint64 conceptually
    const ir::type_ref* size_t_ptr = create_type(std::move(size_t_type));

    // Declare array_size variable
    emit_variable_declaration("array_size", size_t_ptr, size_expr);

    // Build error message dynamically (will be rendered with expressions)
    std::string error_msg = "Array size out of range";

    // Create variable reference for array_size in the bounds check
    // Use parameter_ref (not field_ref) since array_size is a local variable
    ir::expr array_size_ref;
    array_size_ref.type = ir::expr::parameter_ref;
    array_size_ref.ref_name = "array_size";
    const ir::expr* array_size_ptr = create_expression(std::move(array_size_ref));

    emit_bounds_check("array_size", array_size_ptr, min_expr, max_expr, error_msg, use_exceptions);

    // Create expression for array_size variable reference (for resize)
    ir::expr resize_expr;
    resize_expr.type = ir::expr::parameter_ref;
    resize_expr.ref_name = "array_size";
    const ir::expr* resize_expr_ptr = create_expression(std::move(resize_expr));

    // Resize array
    commands_.push_back(std::make_unique<ResizeArrayCommand>(
        field_name, resize_expr_ptr
    ));

    // Loop to read elements
    std::string index_var = "i";
    emit_loop_start(index_var, resize_expr_ptr, false);  // false = don't use .size()

    // Read array element - qualify with object name if in struct context
    std::string qualified_field = expr_context_.object_name.empty()
        ? field_name
        : expr_context_.object_name + "." + field_name;
    std::string element_expr = qualified_field + "[" + index_var + "]";
    emit_array_element_read(element_expr, element_type, use_exceptions);

    emit_loop_end();
}

void CommandBuilder::emit_unbounded_array_read(
    const std::string& field_name,
    const ir::type_ref& element_type,
    bool use_exceptions
) {
    // Unbounded array: T[] - read elements until end of data
    emit_comment("Read until end of data");

    // Emit while loop: while (data < end)
    commands_.push_back(std::make_unique<StartWhileLoopCommand>("data < end"));

    // Qualify field name with object if in struct context
    std::string qualified_field = expr_context_.object_name.empty()
        ? field_name
        : expr_context_.object_name + "." + field_name;

    // Append element using push_back
    commands_.push_back(std::make_unique<AppendToArrayCommand>(
        qualified_field, &element_type, use_exceptions
    ));

    emit_loop_end();
}

void CommandBuilder::emit_array_element_read(
    const std::string& element_var,
    const ir::type_ref& element_type,
    bool use_exceptions
) {
    // Pass IR type directly - no C++-specific conversion here
    commands_.push_back(std::make_unique<ReadArrayElementCommand>(
        element_var, &element_type, use_exceptions
    ));
}

// ============================================================================
// Module-Level Component Builders
// ============================================================================

void CommandBuilder::emit_module_start() {
    commands_.push_back(std::make_unique<ModuleStartCommand>());
}

void CommandBuilder::emit_module_end() {
    commands_.push_back(std::make_unique<ModuleEndCommand>());
}

void CommandBuilder::emit_namespace_start(const std::string& namespace_name) {
    commands_.push_back(std::make_unique<NamespaceStartCommand>(namespace_name));
}

void CommandBuilder::emit_namespace_end(const std::string& namespace_name) {
    commands_.push_back(std::make_unique<NamespaceEndCommand>(namespace_name));
}

void CommandBuilder::emit_include(const std::string& header, bool system_header) {
    commands_.push_back(std::make_unique<IncludeDirectiveCommand>(header, system_header));
}

void CommandBuilder::emit_constant(const std::string& name, const ir::type_ref* type, const ir::expr* value) {
    commands_.push_back(std::make_unique<DeclareConstantCommand>(name, type, value));
}

void CommandBuilder::emit_enum_start(const std::string& name, const ir::type_ref* base_type, bool is_bitmask) {
    commands_.push_back(std::make_unique<StartEnumCommand>(name, base_type, is_bitmask));
}

void CommandBuilder::emit_enum_item(const std::string& name, int64_t value, bool is_last) {
    commands_.push_back(std::make_unique<DeclareEnumItemCommand>(name, value, is_last));
}

void CommandBuilder::emit_enum_end() {
    commands_.push_back(std::make_unique<EndEnumCommand>());
}

void CommandBuilder::emit_subtype(const ir::subtype_def& subtype, const cpp_options& opts) {
    commands_.push_back(std::make_unique<SubtypeCommand>(&subtype, opts));
}

// ============================================================================
// Module Building Helpers
// ============================================================================

void CommandBuilder::emit_module_constants(const ir::bundle& module) {
    // Constants (map of name -> value, infer type from magnitude)
    for (const auto& [name, value] : module.constants) {
        // Determine type based on value magnitude (same logic as old generator)
        ir::type_ref type;
        if (value <= UINT8_MAX) {
            type.kind = ir::type_kind::uint8;
        } else if (value <= UINT16_MAX) {
            type.kind = ir::type_kind::uint16;
        } else if (value <= UINT32_MAX) {
            type.kind = ir::type_kind::uint32;
        } else {
            type.kind = ir::type_kind::uint64;
        }
        const ir::type_ref* type_ptr = create_type(std::move(type));

        // Create integer literal expression
        ir::expr value_expr;
        value_expr.type = ir::expr::literal_int;
        value_expr.int_value = static_cast<int64_t>(value);
        const ir::expr* value_ptr = create_expression(std::move(value_expr));

        emit_constant(name, type_ptr, value_ptr);
    }
}

void CommandBuilder::emit_module_enums(const ir::bundle& module) {
    // Enums
    for (const auto& enum_def : module.enums) {
        emit_enum_start(enum_def.name, &enum_def.base_type, enum_def.is_bitmask);
        for (size_t i = 0; i < enum_def.items.size(); ++i) {
            const auto& item = enum_def.items[i];
            bool is_last = (i == enum_def.items.size() - 1);
            emit_enum_item(item.name, item.value, is_last);
        }
        emit_enum_end();
    }
}

void CommandBuilder::emit_module_subtypes(const ir::bundle& module, const cpp_options& opts) {
    // Subtypes
    for (const auto& subtype_def : module.subtypes) {
        emit_subtype(subtype_def, opts);
    }
}

void CommandBuilder::emit_module_structs_and_choices(const ir::bundle& module, const cpp_options& opts) {
    // Helper lambda to emit a single struct with all its methods
    auto emit_single_struct = [&](const ir::struct_def& struct_def) {
        // Struct declaration
        emit_struct_start(struct_def.name, struct_def.documentation);

        for (const auto& field : struct_def.fields) {
            emit_field_declaration(field.name, &field.type, "");
        }

        // Generate read methods based on error handling mode
        auto modes = ErrorHandlingModes::from_options(opts);

        if (modes.generate_safe) {
            // Generate read_safe() method
            emit_method_start("read_safe", StartMethodCommand::MethodKind::StructReader,
                             &struct_def, false, true);  // false = safe mode, true = static
            emit_variable_declaration("obj", &struct_def);

            expr_context_.in_struct_method = true;
            expr_context_.object_name = "obj";

            // Emit field reads (with special handling for consecutive bitfields)
            size_t i = 0;
            while (i < struct_def.fields.size()) {
                const auto& field = struct_def.fields[i];

                // Initialize field with default value if specified
                if (field.default_value) {
                    emit_comment("Initialize field '" + field.name + "' with default value");
                    emit_variable_assignment("obj." + field.name, &field.default_value.value());
                }

                // Check if this starts a sequence of bitfields
                if (field.type.kind == ir::type_kind::bitfield && field.type.bit_width.has_value()) {
                    // Batch consecutive bitfields together
                    i = emit_bitfield_sequence(struct_def.fields, i, false);
                } else {
                    // Handle conditional fields
                    if (field.condition == ir::field::runtime && field.runtime_condition.has_value()) {
                        emit_comment("Conditional field: " + field.name);
                        emit_if(&field.runtime_condition.value());
                        emit_field_read(field, false);
                        emit_field_constraints(field, false);
                        emit_end_if();
                    } else if (field.condition == ir::field::always) {
                        emit_field_read(field, false);
                        emit_field_constraints(field, false);
                    }
                    // Skip fields with condition == never
                    i++;
                }
            }

            emit_return_value("result");
            emit_method_end();
        }

        if (modes.generate_throw) {
            // Generate read() method
            emit_method_start("read", StartMethodCommand::MethodKind::StructReader,
                             &struct_def, true, true);  // true = exception mode, true = static
            emit_variable_declaration("obj", &struct_def);

            expr_context_.in_struct_method = true;
            expr_context_.object_name = "obj";

            // Emit field reads (with special handling for consecutive bitfields)
            size_t i = 0;
            while (i < struct_def.fields.size()) {
                const auto& field = struct_def.fields[i];

                // Initialize field with default value if specified
                if (field.default_value) {
                    emit_comment("Initialize field '" + field.name + "' with default value");
                    emit_variable_assignment("obj." + field.name, &field.default_value.value());
                }

                // Check if this starts a sequence of bitfields
                if (field.type.kind == ir::type_kind::bitfield && field.type.bit_width.has_value()) {
                    // Batch consecutive bitfields together
                    i = emit_bitfield_sequence(struct_def.fields, i, true);
                } else {
                    // Handle conditional fields
                    if (field.condition == ir::field::runtime && field.runtime_condition.has_value()) {
                        emit_comment("Conditional field: " + field.name);
                        emit_if(&field.runtime_condition.value());
                        emit_field_read(field, true);
                        emit_field_constraints(field, true);
                        emit_end_if();
                    } else if (field.condition == ir::field::always) {
                        emit_field_read(field, true);
                        emit_field_constraints(field, true);
                    }
                    // Skip fields with condition == never
                    i++;
                }
            }

            emit_return_value("obj");
            emit_method_end();
        }

        // Generate user-defined functions
        for (const auto& func : struct_def.functions) {
            emit_comment("User-defined function: " + func.name);

            // Create StartMethodCommand for user function with return type and parameters
            auto cmd = std::make_unique<StartMethodCommand>(
                func.name,
                &func.return_type,
                &func.parameters,  // Pointer to parameters vector (not copied)
                false  // is_static = false (member function, not static)
            );
            commands_.push_back(std::move(cmd));

            // Generate function body
            for (const auto& stmt : func.body) {
                if (auto* ret_stmt = std::get_if<ir::return_statement>(&stmt)) {
                    emit_return_expr(&ret_stmt->value);
                } else if (auto* expr_stmt = std::get_if<ir::expression_statement>(&stmt)) {
                    // Expression statements (for side effects, though rare in DataScript)
                    emit_comment("Expression statement");
                    // Just emit the expression (it will be discarded)
                }
            }

            emit_method_end();
        }

        emit_struct_end();
    };

    // Helper lambda to emit a single choice
    auto emit_single_choice = [&](const ir::choice_def& choice_def) {
        auto choice_commands = build_choice_declaration(choice_def, opts);
        for (auto& cmd : choice_commands) {
            commands_.push_back(std::move(cmd));
        }
    };

    // Helper lambda to emit a single union
    auto emit_single_union = [&](const ir::union_def& union_def) {
        // Collect case types and field names for std::variant
        std::vector<const ir::type_ref*> case_types;
        std::vector<std::string> case_field_names;

        for (const auto& union_case : union_def.cases) {
            for (const auto& field : union_case.fields) {
                case_types.push_back(&field.type);
                case_field_names.push_back(field.name);
            }
        }

        // Detect optional unions: all cases have runtime conditions
        // According to DataScript spec, if all branches have constraints and none match,
        // the union should be allowed to be empty (represented as std::monostate)
        bool is_optional = true;
        for (const auto& union_case : union_def.cases) {
            if (!union_case.condition.has_value()) {
                // Found an unconditional branch (default case)
                is_optional = false;
                break;
            }
        }

        // Emit union start with variant information
        emit_union_start(union_def.name, union_def.documentation, case_types, case_field_names, is_optional);

        // Note: We don't emit individual field declarations anymore since
        // the union is now a std::variant wrapped in a struct

        // Generate read_as_<field>() methods for each union case field
        auto modes = ErrorHandlingModes::from_options(opts);

        for (const auto& union_case : union_def.cases) {
            for (const auto& field : union_case.fields) {
                // Generate safe mode reader
                if (modes.generate_safe) {
                    std::string method_name = "read_as_" + field.name + "_safe";
                    commands_.push_back(std::make_unique<StartMethodCommand>(
                        method_name, &field.type, false, true  // false=safe, true=static
                    ));

                    emit_variable_declaration(field.name, &field.type);
                    expr_context_.object_name = "";  // No object context for union readers
                    emit_field_read(field, false);
                    emit_return_value("result");
                    emit_method_end();
                }

                // Generate exception mode reader
                if (modes.generate_throw) {
                    std::string method_name = "read_as_" + field.name;
                    commands_.push_back(std::make_unique<StartMethodCommand>(
                        method_name, &field.type, true, true  // true=exceptions, true=static
                    ));

                    // Note: Variable declaration now happens in render_read_field() using auto
                    // This produces: auto field_name = FieldType::read(data, end);
                    expr_context_.object_name = "";  // No object context for union readers
                    expr_context_.current_field_name = field.name;  // Track current field for self-references
                    emit_field_read(field, true);
                    expr_context_.current_field_name = "";  // Clear after use
                    emit_return_value(field.name);
                    emit_method_end();
                }
            }
        }

        // Generate unified read() method with trial-and-error decoding
        if (modes.generate_throw && !union_def.cases.empty()) {
            emit_comment("Unified read() method with trial-and-error decoding");

            // Start the read() method with UnionReader kind
            commands_.push_back(std::make_unique<StartMethodCommand>(
                "read",
                StartMethodCommand::MethodKind::UnionReader,
                nullptr,  // target_struct (not used for unions)
                true,     // use_exceptions
                true      // is_static
            ));

            // Declare result variable (renderer will emit: UnionName result;)
            commands_.push_back(std::make_unique<DeclareVariableCommand>(
                "result", union_def.name  // Custom type name
            ));

            // Save initial position for backtracking
            std::string pos_var = "union_pos";
            commands_.push_back(std::make_unique<SavePositionCommand>(pos_var));

            // Iterate through all union cases and try each branch
            size_t branch_index = 0;
            for (const auto& union_case : union_def.cases) {
                for (const auto& field : union_case.fields) {
                    bool is_last = (branch_index == case_types.size() - 1);

                    // Start try-branch block (renderer will emit variant assignment)
                    commands_.push_back(std::make_unique<StartTryBranchCommand>(field.name));

                    // If successful, return immediately
                    commands_.push_back(std::make_unique<ReturnValueCommand>("result"));

                    // End try-branch (catch on failure unless last)
                    commands_.push_back(std::make_unique<EndTryBranchCommand>(is_last, is_optional));

                    // If not last branch, restore position for next attempt
                    if (!is_last) {
                        commands_.push_back(std::make_unique<RestorePositionCommand>(pos_var));
                    }

                    branch_index++;
                }
            }

            // For optional unions, if all branches fail, the union remains empty (std::monostate)
            // For non-optional unions, the last branch rethrows the exception
            if (is_optional) {
                emit_comment("All union branches failed - union remains empty (std::monostate)");
                emit_return_value("result");  // Return with std::monostate
            } else {
                emit_comment("All union branches failed - unreachable with exception mode");
            }

            emit_method_end();
        }

        emit_union_end();
    };

    // Emit structs, unions, and choices in topologically sorted order
    // type_emission_order encoding: 0=struct, 1=union, 2=choice
    for (const auto& [type_kind, index] : module.type_emission_order) {
        if (type_kind == 0) {
            // Struct
            emit_single_struct(module.structs[index]);
        } else if (type_kind == 1) {
            // Union
            emit_single_union(module.unions[index]);
        } else {
            // Choice (type_kind == 2)
            emit_single_choice(module.choices[index]);
        }
    }
}

void CommandBuilder::emit_module_unions(const ir::bundle& module, const cpp_options& opts) {
    // Unions
    for (const auto& union_def : module.unions) {
        emit_union_start(union_def.name, union_def.documentation);

        // Emit field declarations for all fields in all cases
        for (const auto& union_case : union_def.cases) {
            for (const auto& field : union_case.fields) {
                emit_field_declaration(field.name, &field.type, "");
            }
        }

        // Generate read_as_<field>() methods for each field
        auto modes = ErrorHandlingModes::from_options(opts);

        for (const auto& union_case : union_def.cases) {
            for (const auto& field : union_case.fields) {
            // Generate safe mode reader: static ReadResult<field_type> read_as_<field>_safe(...)
            if (modes.generate_safe) {
                std::string method_name = "read_as_" + field.name + "_safe";
                commands_.push_back(std::make_unique<StartMethodCommand>(
                    method_name, &field.type, false, true  // false=safe, true=static
                ));

                // Declare variable for the field value
                emit_variable_declaration(field.name, &field.type);

                // Read the field
                expr_context_.object_name = "";  // No object context for union readers
                emit_field_read(field, false);

                // Return result
                emit_return_value("result");
                emit_method_end();
            }

            // Generate exception mode reader: static field_type read_as_<field>(...)
            if (modes.generate_throw) {
                std::string method_name = "read_as_" + field.name;
                commands_.push_back(std::make_unique<StartMethodCommand>(
                    method_name, &field.type, true, true  // true=exceptions, true=static
                ));

                // Declare variable for the field value
                emit_variable_declaration(field.name, &field.type);

                // Read the field
                expr_context_.object_name = "";  // No object context for union readers
                emit_field_read(field, true);

                // Return the field value
                emit_return_value(field.name);
                emit_method_end();
            }
            }
        }

        emit_union_end();
    }
}

// ============================================================================
// Module Builder
// ============================================================================

std::vector<CommandPtr> CommandBuilder::build_module(
    const ir::bundle& module,
    const std::string& namespace_name,
    const codegen::cpp_options& opts,
    bool use_exceptions
) {
    BuilderScope scope{*this};  // RAII guard for exception safety
    (void)use_exceptions;  // Deprecated parameter, use opts.error_handling instead
    clear();

    // Store constraints for constraint validation
    constraints_ = &module.constraints;

    // Store choices for choice field reading
    choices_ = &module.choices;

    // Module start
    emit_module_start();

    // Namespace start
    emit_namespace_start(namespace_name);

    // Constants
    emit_module_constants(module);

    // Enums
    emit_module_enums(module);

    // Subtypes
    emit_module_subtypes(module, opts);

    // Structs, Unions, and Choices (topologically sorted)
    emit_module_structs_and_choices(module, opts);

    // NOTE: Unions are now emitted within emit_module_structs_and_choices via topological sort
    // to ensure correct dependency ordering (inline unions before structs that use them)

    // Namespace end
    emit_namespace_end(namespace_name);

    // Module end
    emit_module_end();

    return scope.take_commands();  // Success: transfer command ownership
}

// ============================================================================
// Expression Ownership
// ============================================================================

const ir::expr* CommandBuilder::create_expression(ir::expr&& expr) {
    owned_expressions_.push_back(std::make_unique<ir::expr>(std::move(expr)));
    return owned_expressions_.back().get();
}

const ir::type_ref* CommandBuilder::create_type(ir::type_ref&& type) {
    owned_types_.push_back(std::make_unique<ir::type_ref>(std::move(type)));
    return owned_types_.back().get();
}

} // namespace datascript::codegen

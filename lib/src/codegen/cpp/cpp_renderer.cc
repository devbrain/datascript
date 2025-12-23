//
// C++ Code Renderer Implementation
//
// Converts command streams to formatted C++ code.
//

#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <datascript/codegen/cpp/cpp_library_mode.hh>
#include <datascript/codegen/cpp/cpp_helper_generator.hh>
#include <datascript/codegen/cpp/cpp_expression_renderer.hh>
#include <datascript/codegen.hh>
#include <datascript/command_builder.hh>
#include <sstream>
#include <algorithm>

namespace datascript::codegen {

// ============================================================================
// C++ Keywords - Static Member Definition
// ============================================================================

const std::set<std::string> CppRenderer::cpp_keywords_ = {
    // C++20 reserved keywords (77 total)
    "alignas", "alignof", "and", "and_eq", "asm", "auto",
    "bitand", "bitor", "bool", "break",
    "case", "catch", "char", "char8_t", "char16_t", "char32_t",
    "class", "compl", "concept", "const", "consteval", "constexpr",
    "constinit", "const_cast", "continue", "co_await", "co_return", "co_yield",
    "decltype", "default", "delete", "do", "double", "dynamic_cast",
    "else", "enum", "explicit", "export", "extern",
    "false", "float", "for", "friend",
    "goto",
    "if", "inline", "int",
    "long",
    "mutable",
    "namespace", "new", "noexcept", "not", "not_eq", "nullptr",
    "operator", "or", "or_eq",
    "private", "protected", "public",
    "register", "reinterpret_cast", "requires", "return",
    "short", "signed", "sizeof", "static", "static_assert", "static_cast",
    "struct", "switch",
    "template", "this", "thread_local", "throw", "true", "try",
    "typedef", "typeid", "typename",
    "union", "unsigned", "using",
    "virtual", "void", "volatile",
    "wchar_t", "while",
    "xor", "xor_eq"
};

// ============================================================================
// Construction
// ============================================================================

CppRenderer::CppRenderer()
    : ctx_(output_),  // Initialize context with output stream
      module_(nullptr),
      safe_read_mode_(true),
      error_handling_mode_(cpp_options::both),
      in_struct_(false),
      in_method_(false),
      current_enum_is_bitmask_(false),
      in_choice_(false),
      first_choice_case_(false),
      label_counter_(0),
      current_method_kind_(StartMethodCommand::MethodKind::Custom),
      current_method_target_struct_(nullptr),
      current_method_use_exceptions_(false)
{
    expr_context_.in_struct_method = false;
    expr_context_.use_safe_reads = true;
}

// ============================================================================
// Configuration
// ============================================================================

void CppRenderer::set_module(const ir::bundle* mod) {
    module_ = mod;
    if (mod) {
        expr_context_.module_constants = &mod->constants;
    }
    // Clear type name cache when module changes (old pointers may be invalid)
    type_name_cache_.clear();
}

void CppRenderer::set_namespace(const std::string& ns) {
    namespace_ = ns;
}

void CppRenderer::set_safe_read_mode(bool enabled) {
    safe_read_mode_ = enabled;
    expr_context_.use_safe_reads = enabled;
}

void CppRenderer::set_error_handling_mode(cpp_options::error_style mode) {
    error_handling_mode_ = mode;
}

// (Library mode helpers removed - now in CppLibraryModeGenerator)

// ============================================================================
// BaseRenderer Interface Implementation
// ============================================================================

std::string CppRenderer::render_module(const ir::bundle& bundle,
                                       const RenderOptions& options) {
    // Convert generic RenderOptions to C++-specific options
    cpp_options cpp_opts;

    // Map error handling mode
    if (options.use_exceptions) {
        cpp_opts.error_handling = cpp_options::exceptions_only;
    } else {
        cpp_opts.error_handling = cpp_options::results_only;
    }

    // Map documentation options
    cpp_opts.include_source_refs = options.generate_comments;
    cpp_opts.include_field_docs = options.generate_documentation;

    // Build commands using CommandBuilder
    CommandBuilder builder;
    bool use_exceptions = (cpp_opts.error_handling != cpp_options::results_only);

    // Determine namespace: use package name from IR if namespace is default "generated"
    std::string namespace_name = cpp_opts.namespace_name;
    if (namespace_name == "generated" && !bundle.name.empty()) {
        // Convert package name (com.example.foo) to C++ namespace (com::example::foo)
        namespace_name = bundle.name;
        std::replace(namespace_name.begin(), namespace_name.end(), '.', ':');
        // Convert single : to :: for C++ namespace separator
        std::string result;
        for (char c : namespace_name) {
            result += c;
            if (c == ':') result += ':';
        }
        namespace_name = result;
    }

    auto commands = builder.build_module(bundle, namespace_name, cpp_opts, use_exceptions);

    // Use THIS renderer (which has options set) instead of creating a new one
    set_module(&bundle);  // Set module for type name resolution
    set_error_handling_mode(cpp_opts.error_handling);  // Set error handling mode
    render_commands(commands);

    return get_output();
}

std::string CppRenderer::render_expression(const ir::expr* expr) {
    if (!expr) {
        throw invalid_ir_error("Null expression pointer");
    }
    return render_expr_internal(*expr, expr_context_);
}

// ============================================================================
// Expression Rendering Implementation
// ============================================================================

std::string CppRenderer::render_expr_internal(const ir::expr& expr, const ExprContext& ctx) {
    // Delegate to CppExpressionRenderer for cleaner separation of concerns
    CppExpressionRenderer expr_renderer(ctx, module_);
    return expr_renderer.render(expr);
}


LanguageMetadata CppRenderer::get_metadata() const {
    LanguageMetadata meta;
    meta.name = "C++";
    meta.version = "C++20";
    meta.file_extension = ".cc";
    meta.is_case_sensitive = true;
    meta.supports_generics = true;   // Templates
    meta.supports_exceptions = true;
    return meta;
}

std::string CppRenderer::get_option_prefix() const {
    return "cpp";
}

std::vector<OptionDescription> CppRenderer::get_options() const {
    return {
        {
            "exceptions",
            OptionType::Bool,
            "Use exceptions for error handling",
            "true",
            {}  // choices (not applicable for Bool)
        },
        {
            "output-name",
            OptionType::String,
            "Override output filename (default: derive from struct name)",
            std::nullopt,
            {}  // choices (not applicable for String)
        },
        {
            "enum-to-string",
            OptionType::Bool,
            "Generate enum-to-string conversion functions",
            "false",
            {}  // choices (not applicable for Bool)
        },
        {
            "mode",
            OptionType::Choice,
            "Output mode: single-header (all-in-one) or library (separate public/impl headers)",
            "single-header",
            {"single-header", "library"}
        }
    };
}

void CppRenderer::set_option(const std::string& name, const OptionValue& value) {
    if (name == "exceptions") {
        // Note: This sets the legacy safe_read_mode_ which is inverse of exceptions
        // TODO: Refactor to use RenderOptions directly in future
        bool use_exceptions = std::get<bool>(value);
        safe_read_mode_ = !use_exceptions;
    } else if (name == "output-name") {
        output_name_override_ = std::get<std::string>(value);
    } else if (name == "enum-to-string") {
        generate_enum_to_string_ = std::get<bool>(value);
    } else if (name == "mode") {
        output_mode_ = std::get<std::string>(value);
    } else {
        throw std::invalid_argument("Unknown cpp option: " + name);
    }
}

std::vector<OutputFile> CppRenderer::generate_files(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Check if library mode is enabled
    if (output_mode_ == "library") {
        return generate_library_mode(bundle, output_dir);
    } else {
        return generate_single_header_mode(bundle, output_dir);
    }
}

std::vector<OutputFile> CppRenderer::generate_single_header_mode(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Determine filename: use override or derive from first struct/choice name
    std::string filename;
    if (output_name_override_) {
        filename = *output_name_override_;
        // Ensure .hh extension if not provided
        if (!filename.ends_with(".hh") && !filename.ends_with(".hpp") &&
            !filename.ends_with(".h")) {
            filename += ".hh";
        }
    } else if (!bundle.structs.empty()) {
        filename = bundle.structs[0].name + ".hh";
    } else if (!bundle.choices.empty()) {
        filename = bundle.choices[0].name + ".hh";
    } else if (!bundle.enums.empty()) {
        filename = bundle.enums[0].name + ".hh";
    } else {
        filename = "generated.hh";
    }

    // C++ generates a single header file
    std::filesystem::path output_path = output_dir / filename;

    // Generate code using existing render_module() method
    RenderOptions default_options;
    std::string content = render_module(bundle, default_options);

    return {{output_path, content}};
}

std::vector<OutputFile> CppRenderer::generate_library_mode(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Delegate to CppLibraryModeGenerator
    CppLibraryModeGenerator generator(*this);
    return generator.generate(bundle, output_dir);
}

std::string CppRenderer::get_language_name() const {
    return "C++";
}

std::string CppRenderer::get_file_extension() const {
    return ".cc";
}

bool CppRenderer::is_case_sensitive() const {
    return true;
}

std::string CppRenderer::get_default_object_name() const {
    return "obj";  // C++ convention for object instances in static methods
}

bool CppRenderer::is_reserved_keyword(const std::string& identifier) const {
    return cpp_keywords_.contains(identifier);
}

std::string CppRenderer::sanitize_identifier(const std::string& identifier) const {
    if (!is_reserved_keyword(identifier)) {
        return identifier;
    }
    // Default strategy: append underscore
    return identifier + "_";
}

std::vector<std::string> CppRenderer::get_all_keywords() const {
    return std::vector<std::string>(cpp_keywords_.begin(), cpp_keywords_.end());
}

std::string CppRenderer::get_type_name(const ir::type_ref& type) const {
    // Delegate to existing helper
    return ir_type_to_cpp(&type);
}

// ============================================================================
// Main Rendering
// ============================================================================

void CppRenderer::render_commands(const std::vector<CommandPtr>& commands) {
    for (const auto& cmd : commands) {
        render_command(*cmd);
    }
}

std::string CppRenderer::get_output() const {
    return output_.str();
}

void CppRenderer::clear() {
    output_.str("");
    output_.clear();
    in_struct_ = false;
    in_method_ = false;
    current_struct_name_.clear();
}

// ============================================================================
// Individual Command Rendering
// ============================================================================

void CppRenderer::render_command(const Command& cmd) {
    switch (cmd.type) {
        case Command::ModuleStart:
            render_module_start(static_cast<const ModuleStartCommand&>(cmd));
            break;
        case Command::ModuleEnd:
            render_module_end(static_cast<const ModuleEndCommand&>(cmd));
            break;
        case Command::NamespaceStart:
            render_namespace_start(static_cast<const NamespaceStartCommand&>(cmd));
            break;
        case Command::NamespaceEnd:
            render_namespace_end(static_cast<const NamespaceEndCommand&>(cmd));
            break;
        case Command::IncludeDirective:
            render_include_directive(static_cast<const IncludeDirectiveCommand&>(cmd));
            break;

        case Command::DeclareConstant:
            render_declare_constant(static_cast<const DeclareConstantCommand&>(cmd));
            break;

        case Command::StartEnum:
            render_start_enum(static_cast<const StartEnumCommand&>(cmd));
            break;
        case Command::DeclareEnumItem:
            render_declare_enum_item(static_cast<const DeclareEnumItemCommand&>(cmd));
            break;
        case Command::EndEnum:
            render_end_enum(static_cast<const EndEnumCommand&>(cmd));
            break;

        case Command::Subtype:
            render_subtype(static_cast<const SubtypeCommand&>(cmd));
            break;

        case Command::StartStruct:
            render_start_struct(static_cast<const StartStructCommand&>(cmd));
            break;
        case Command::EndStruct:
            render_end_struct(static_cast<const EndStructCommand&>(cmd));
            break;
        case Command::DeclareField:
            render_declare_field(static_cast<const DeclareFieldCommand&>(cmd));
            break;

        case Command::StartUnion:
            render_start_union(static_cast<const StartUnionCommand&>(cmd));
            break;
        case Command::EndUnion:
            render_end_union(static_cast<const EndUnionCommand&>(cmd));
            break;

        case Command::StartChoice:
            render_start_choice(static_cast<const StartChoiceCommand&>(cmd));
            break;
        case Command::EndChoice:
            render_end_choice(static_cast<const EndChoiceCommand&>(cmd));
            break;
        case Command::StartChoiceCase:
            render_start_choice_case(static_cast<const StartChoiceCaseCommand&>(cmd));
            break;
        case Command::EndChoiceCase:
            render_end_choice_case(static_cast<const EndChoiceCaseCommand&>(cmd));
            break;

        case Command::StartMethod:
            render_start_method(static_cast<const StartMethodCommand&>(cmd));
            break;
        case Command::EndMethod:
            render_end_method(static_cast<const EndMethodCommand&>(cmd));
            break;
        case Command::ReturnValue:
            render_return_value(static_cast<const ReturnValueCommand&>(cmd));
            break;
        case Command::ReturnExpression:
            render_return_expression(static_cast<const ReturnExpressionCommand&>(cmd));
            break;

        case Command::ReadField:
            render_read_field(static_cast<const ReadFieldCommand&>(cmd));
            break;
        case Command::ReadArrayElement:
            render_read_array_element(static_cast<const ReadArrayElementCommand&>(cmd));
            break;
        case Command::SeekToLabel:
            render_seek_to_label(static_cast<const SeekToLabelCommand&>(cmd));
            break;
        case Command::AlignPointer:
            render_align_pointer(static_cast<const AlignPointerCommand&>(cmd));
            break;
        case Command::ResizeArray:
            render_resize_array(static_cast<const ResizeArrayCommand&>(cmd));
            break;
        case Command::AppendToArray:
            render_append_to_array(static_cast<const AppendToArrayCommand&>(cmd));
            break;

        case Command::StartLoop:
            render_start_loop(static_cast<const StartLoopCommand&>(cmd));
            break;
        case Command::StartWhileLoop:
            render_start_while_loop(static_cast<const StartWhileLoopCommand&>(cmd));
            break;
        case Command::EndLoop:
            render_end_loop(static_cast<const EndLoopCommand&>(cmd));
            break;

        case Command::If:
            render_if(static_cast<const IfCommand&>(cmd));
            break;
        case Command::Else:
            render_else(static_cast<const ElseCommand&>(cmd));
            break;
        case Command::EndIf:
            render_end_if(static_cast<const EndIfCommand&>(cmd));
            break;

        case Command::StartScope:
            render_start_scope(static_cast<const StartScopeCommand&>(cmd));
            break;
        case Command::EndScope:
            render_end_scope(static_cast<const EndScopeCommand&>(cmd));
            break;

        case Command::SavePosition:
            render_save_position(static_cast<const SavePositionCommand&>(cmd));
            break;
        case Command::RestorePosition:
            render_restore_position(static_cast<const RestorePositionCommand&>(cmd));
            break;
        case Command::StartTryBranch:
            render_start_try_branch(static_cast<const StartTryBranchCommand&>(cmd));
            break;
        case Command::EndTryBranch:
            render_end_try_branch(static_cast<const EndTryBranchCommand&>(cmd));
            break;

        case Command::BoundsCheck:
            render_bounds_check(static_cast<const BoundsCheckCommand&>(cmd));
            break;
        case Command::ConstraintCheck:
            render_constraint_check(static_cast<const ConstraintCheckCommand&>(cmd));
            break;

        case Command::DeclareVariable:
            render_declare_variable(static_cast<const DeclareVariableCommand&>(cmd));
            break;
        case Command::AssignVariable:
            render_assign_variable(static_cast<const AssignVariableCommand&>(cmd));
            break;
        case Command::AssignChoiceWrapper:
            render_assign_choice_wrapper(static_cast<const AssignChoiceWrapperCommand&>(cmd));
            break;

        case Command::SetErrorMessage:
            render_set_error_message(static_cast<const SetErrorMessageCommand&>(cmd));
            break;
        case Command::ThrowException:
            render_throw_exception(static_cast<const ThrowExceptionCommand&>(cmd));
            break;

        case Command::DeclareResultVariable:
            render_declare_result_variable(static_cast<const DeclareResultVariableCommand&>(cmd));
            break;
        case Command::SetResultSuccess:
            render_set_result_success(static_cast<const SetResultSuccessCommand&>(cmd));
            break;

        case Command::Comment:
            render_comment(static_cast<const CommentCommand&>(cmd));
            break;

        case Command::ReadPrimitiveToVariable:
            render_read_primitive_to_variable(static_cast<const ReadPrimitiveToVariableCommand&>(cmd));
            break;

        case Command::ExtractBitfield:
            render_extract_bitfield(static_cast<const ExtractBitfieldCommand&>(cmd));
            break;
    }
}

// ============================================================================
// Module-Level Commands
// ============================================================================

void CppRenderer::render_module_start(const ModuleStartCommand& cmd) {
    (void)cmd;
    ctx_ << "#pragma once" << endl;
    ctx_ << blank;
    ctx_ << "// Suppress warnings in generated code" << endl;
    ctx_ << "// Note: __clang__ must be checked before _MSC_VER because clang-cl defines both" << endl;
    ctx_ << "#if defined(__clang__)" << endl;
    ctx_ << "#pragma clang diagnostic push" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wunused-variable\"" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wunused-but-set-variable\"" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wunused-parameter\"" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wparentheses-equality\"" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wsign-conversion\"" << endl;
    ctx_ << "#pragma clang diagnostic ignored \"-Wimplicit-int-conversion\"" << endl;
    ctx_ << "#elif defined(_MSC_VER)" << endl;
    ctx_ << "#pragma warning(push)" << endl;
    ctx_ << "#pragma warning(disable: 4189)  // local variable initialized but not referenced" << endl;
    ctx_ << "#pragma warning(disable: 4100)  // unreferenced formal parameter" << endl;
    ctx_ << "#pragma warning(disable: 4244)  // conversion from 'type1' to 'type2', possible loss of data" << endl;
    ctx_ << "#pragma warning(disable: 4267)  // conversion from 'size_t' to 'type', possible loss of data" << endl;
    ctx_ << "#elif defined(__GNUC__)" << endl;
    ctx_ << "#pragma GCC diagnostic push" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wunused-variable\"" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wunused-but-set-variable\"" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wunused-parameter\"" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wstringop-overflow\"" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wsign-conversion\"" << endl;
    ctx_ << "#pragma GCC diagnostic ignored \"-Wconversion\"" << endl;
    ctx_ << "#endif" << endl;
    ctx_ << blank;

    // Emit C++-specific includes
    ctx_ << "#include <array>" << endl;
    ctx_ << "#include <cstdint>" << endl;
    ctx_ << "#include <string>" << endl;
    ctx_ << "#include <vector>" << endl;
    ctx_ << "#include <variant>" << endl;  // For choice types
    ctx_ << "#include <stdexcept>" << endl;
}

void CppRenderer::render_module_end(const ModuleEndCommand& cmd) {
    (void)cmd;
    // No output needed - namespace end already handled
}

void CppRenderer::render_namespace_start(const NamespaceStartCommand& cmd) {
    ctx_ << blank;
    ctx_.start_namespace(cmd.namespace_name);

    // Emit helper functions (C++-specific implementation detail)
    emit_helper_functions();
}

void CppRenderer::render_namespace_end(const NamespaceEndCommand& cmd) {
    (void)cmd;
    ctx_.end_namespace();
}

void CppRenderer::render_include_directive(const IncludeDirectiveCommand& cmd) {
    if (cmd.system_header) {
        ctx_ << "#include <" + cmd.header + ">" << endl;
    } else {
        ctx_ << "#include \"" + cmd.header + "\"" << endl;
    }
}

// ============================================================================
// Constant Commands
// ============================================================================

void CppRenderer::render_declare_constant(const DeclareConstantCommand& cmd) {
    std::string type_str = ir_type_to_cpp(cmd.type);
    std::string value_str = render_expression(cmd.value);
    ctx_ << "constexpr " + type_str + " " + cmd.name + " = " + value_str + ";" << endl;
}

// ============================================================================
// Enum Commands
// ============================================================================

void CppRenderer::render_start_enum(const StartEnumCommand& cmd) {
    std::string base_type_str = ir_type_to_cpp(cmd.base_type);
    ctx_.start_enum(cmd.enum_name, base_type_str);
    // Store for later use in render_end_enum
    current_enum_name_ = cmd.enum_name;
    current_enum_is_bitmask_ = cmd.is_bitmask;
    current_enum_items_.clear();  // Clear items for this enum
}

void CppRenderer::render_declare_enum_item(const DeclareEnumItemCommand& cmd) {
    std::string line = cmd.item_name + " = " + std::to_string(cmd.value);
    if (!cmd.is_last) {
        line += ",";
    }
    ctx_ << line << endl;
    // Track enum item for to_string generation
    current_enum_items_.push_back(cmd.item_name);
}

void CppRenderer::render_end_enum(const EndEnumCommand& cmd) {
    (void)cmd;
    ctx_.end_enum();

    // Always generate bitwise operators for enum class (needed for bitwise operations)
    if (true) {  // Always generate, not just for bitmasks
        ctx_ << "// Bitwise operators for " + current_enum_name_ << endl;
        std::string type = current_enum_name_;

        // operator|
        ctx_.start_inline_function(type, "operator|", type + " a, " + type + " b");
        ctx_ << "return static_cast<" + type + ">(static_cast<std::underlying_type_t<" + type + ">>(a) | static_cast<std::underlying_type_t<" + type + ">>(b));" << endl;
        ctx_.end_inline_function();

        // operator&
        ctx_.start_inline_function(type, "operator&", type + " a, " + type + " b");
        ctx_ << "return static_cast<" + type + ">(static_cast<std::underlying_type_t<" + type + ">>(a) & static_cast<std::underlying_type_t<" + type + ">>(b));" << endl;
        ctx_.end_inline_function();

        // operator^
        ctx_.start_inline_function(type, "operator^", type + " a, " + type + " b");
        ctx_ << "return static_cast<" + type + ">(static_cast<std::underlying_type_t<" + type + ">>(a) ^ static_cast<std::underlying_type_t<" + type + ">>(b));" << endl;
        ctx_.end_inline_function();

        // operator~
        ctx_.start_inline_function(type, "operator~", type + " a");
        ctx_ << "return static_cast<" + type + ">(~static_cast<std::underlying_type_t<" + type + ">>(a));" << endl;
        ctx_.end_inline_function();

        // operator|=
        ctx_.start_inline_function(type + "&", "operator|=", type + "& a, " + type + " b");
        ctx_ << "return a = a | b;" << endl;
        ctx_.end_inline_function();

        // operator&=
        ctx_.start_inline_function(type + "&", "operator&=", type + "& a, " + type + " b");
        ctx_ << "return a = a & b;" << endl;
        ctx_.end_inline_function();

        // operator^=
        ctx_.start_inline_function(type + "&", "operator^=", type + "& a, " + type + " b");
        ctx_ << "return a = a ^ b;" << endl;
        ctx_.end_inline_function();

        // Comparison operators with int (for comparing with 0)
        ctx_.start_inline_function("bool", "operator==", type + " a, int b");
        ctx_ << "return static_cast<std::underlying_type_t<" + type + ">>(a) == b;" << endl;
        ctx_.end_inline_function();

        ctx_.start_inline_function("bool", "operator!=", type + " a, int b");
        ctx_ << "return static_cast<std::underlying_type_t<" + type + ">>(a) != b;" << endl;
        ctx_.end_inline_function();
    }

    // Generate enum-to-string conversion function if enabled
    if (generate_enum_to_string_ && !current_enum_items_.empty()) {
        ctx_ << blank;
        ctx_ << "// Enum to string conversion for " + current_enum_name_ << endl;
        std::string type = current_enum_name_;

        ctx_.start_inline_function("const char*", type + "_to_string", type + " value");
        ctx_ << "switch (value) {" << endl;

        for (const auto& item : current_enum_items_) {
            ctx_ << "    case " + type + "::" + item + ": return \"" + item + "\";" << endl;
        }

        ctx_ << "    default: return \"Unknown\";" << endl;
        ctx_ << "}" << endl;
        ctx_.end_inline_function();
    }

    ctx_ << blank;
}

// ============================================================================
// Subtype Commands
// ============================================================================

void CppRenderer::render_subtype(const SubtypeCommand& cmd) {
    const auto& subtype = *cmd.subtype;

    // Generate documentation comment if present
    if (!subtype.documentation.empty()) {
        ctx_ << "/**" << endl;
        ctx_ << " * " + subtype.documentation << endl;
        ctx_ << " */" << endl;
    }

    // Get C++ type name for base type
    std::string cpp_base_type = ir_type_to_cpp(&subtype.base_type);

    // Generate type alias
    ctx_ << "using " + subtype.name + " = " + cpp_base_type + ";" << endl;
    ctx_ << blank;

    // Generate validation function
    ctx_ << "// Validation function for " + subtype.name << endl;
    ctx_.start_inline_function("bool", "validate_" + subtype.name, cpp_base_type + " value");

    // Render constraint expression with 'this' -> 'value' substitution
    // Save current context
    ExprContext saved_context = expr_context_;

    // Set up context for rendering: map 'this' to 'value'
    expr_context_.add_variable("this", "value");

    std::string constraint_expr = render_expression(&subtype.constraint);

    // Restore context
    expr_context_ = saved_context;

    ctx_ << "return " + constraint_expr + ";" << endl;
    ctx_.end_inline_function();
    ctx_ << blank;

    // Generate reader function(s) based on error handling mode
    bool generate_exception_reader = (cmd.opts.error_handling == cpp_options::exceptions_only ||
                                      cmd.opts.error_handling == cpp_options::both);
    bool generate_safe_reader = (cmd.opts.error_handling == cpp_options::results_only ||
                                 cmd.opts.error_handling == cpp_options::both);

    if (generate_exception_reader) {
        // Exception-based reader
        ctx_ << "// Reader function for " + subtype.name + " (exception mode)" << endl;
        ctx_.start_inline_function(subtype.name, "read_" + subtype.name, "const uint8_t*& data, const uint8_t* end");

        ctx_ << cpp_base_type + " value = " + generate_read_call(&subtype.base_type, true) + ";" << endl;
        ctx_.start_if("!validate_" + subtype.name + "(value)");
        ctx_ << "throw std::runtime_error(\"Validation failed for " + subtype.name + "\");" << endl;
        ctx_.end_if();
        ctx_ << "return static_cast<" + subtype.name + ">(value);" << endl;

        ctx_.end_inline_function();
        ctx_ << blank;
    }

    if (generate_safe_reader) {
        // Safe mode reader (returns ReadResult)
        ctx_ << "// Reader function for " + subtype.name + " (safe mode)" << endl;
        ctx_.start_inline_function("ReadResult<" + subtype.name + ">", "read_" + subtype.name + "_safe", "const uint8_t*& data, const uint8_t* end");

        ctx_ << "auto result = " + generate_read_call(&subtype.base_type, false) + ";" << endl;
        ctx_.start_if("!result.success");
        ctx_ << "return ReadResult<" + subtype.name + ">::error(result.error);" << endl;
        ctx_.end_if();
        ctx_.start_if("!validate_" + subtype.name + "(result.value)");
        ctx_ << "return ReadResult<" + subtype.name + ">::error(\"Validation failed for " +
                   subtype.name + "\");" << endl;
        ctx_.end_if();
        ctx_ << "return ReadResult<" + subtype.name + ">::ok(static_cast<" +
                   subtype.name + ">(result.value));" << endl;

        ctx_.end_inline_function();
        ctx_ << blank;
    }
}

// ============================================================================
// Structure Commands
// ============================================================================

void CppRenderer::render_start_struct(const StartStructCommand& cmd) {
    in_struct_ = true;
    current_struct_name_ = cmd.struct_name;
    ctx_.start_struct(cmd.struct_name, cmd.doc_comment);
}

void CppRenderer::render_end_struct(const EndStructCommand& cmd) {
    (void)cmd;
    ctx_.end_struct();
    in_struct_ = false;
    current_struct_name_.clear();
}

void CppRenderer::render_declare_field(const DeclareFieldCommand& cmd) {
    if (!cmd.doc_comment.empty()) {
        ctx_ << "// " + cmd.doc_comment << endl;
    }
    std::string cpp_type = ir_type_to_cpp(cmd.field_type);
    ctx_ << cpp_type + " " + cmd.field_name + ";" << endl;
}

// ============================================================================
// Union Commands
// ============================================================================

void CppRenderer::render_start_union(const StartUnionCommand& cmd) {
    in_struct_ = true;  // Unions use similar context to structs
    current_struct_name_ = cmd.union_name;

    // Emit as struct with std::variant (DataScript unions are discriminated unions)
    ctx_.start_struct(cmd.union_name, cmd.doc_comment);

    // Generate std::variant field with all case types
    if (!cmd.case_types.empty()) {
        ctx_.writer() << "    std::variant<";

        // If optional union (all branches have constraints), prepend std::monostate
        // This allows the union to be "empty" when no constraint matches
        if (cmd.is_optional) {
            ctx_.writer() << "std::monostate, ";
        }

        for (size_t i = 0; i < cmd.case_types.size(); ++i) {
            if (i > 0) ctx_.writer() << ", ";
            std::string type_str = ir_type_to_cpp(cmd.case_types[i]);
            ctx_.writer() << type_str;
        }
        ctx_.writer() << "> value;\n";
        ctx_ << blank;

        // For optional unions, add is_empty() helper method
        if (cmd.is_optional) {
            ctx_ << "// Check if union is empty (no branch was matched)" << endl;
            ctx_ << "bool is_empty() const { return std::holds_alternative<std::monostate>(value); }" << endl;
            ctx_ << blank;
        }

        // Generate accessor methods: TypeN* as_fieldN() { return std::get_if<TypeN>(&value); }
        for (size_t i = 0; i < cmd.case_types.size(); ++i) {
            std::string type_str = ir_type_to_cpp(cmd.case_types[i]);
            std::string field_name = cmd.case_field_names[i];
            ctx_ << type_str + "* as_" + field_name + "() { return std::get_if<" + type_str + ">(&value); }" << endl;
            ctx_ << "const " + type_str + "* as_" + field_name + "() const { return std::get_if<" + type_str + ">(&value); }" << endl;
        }
        ctx_ << blank;

        // Note: Unified read() method is now emitted via commands (trial-and-error decoding)
    }
}

void CppRenderer::render_end_union(const EndUnionCommand& cmd) {
    (void)cmd;
    ctx_.end_struct();
    in_struct_ = false;
    current_struct_name_.clear();
}

// ============================================================================
// Choice Commands
// ============================================================================

void CppRenderer::render_start_choice(const StartChoiceCommand& cmd) {
    in_struct_ = true;
    current_struct_name_ = cmd.choice_name;
    ctx_.start_struct(cmd.choice_name, cmd.doc_comment);

    // Generate wrapper structs for each case (ensures unique variant types)
    for (size_t i = 0; i < cmd.case_types.size(); ++i) {
        std::string type_str = ir_type_to_cpp(cmd.case_types[i]);
        std::string field_name = cmd.case_field_names[i];
        std::string wrapper_name = "Case" + std::to_string(i) + "_" + field_name;
        ctx_ << "struct " + wrapper_name + " { " + type_str + " value; };" << endl;
    }
    ctx_ << blank;

    // Generate std::variant field with wrapper types
    ctx_.writer() << "    std::variant<";
    for (size_t i = 0; i < cmd.case_types.size(); ++i) {
        if (i > 0) ctx_.writer() << ",  // case\n        ";
        else ctx_.writer() << "\n        ";
        std::string field_name = cmd.case_field_names[i];
        std::string wrapper_name = "Case" + std::to_string(i) + "_" + field_name;
        ctx_.writer() << wrapper_name;
        if (i == cmd.case_types.size() - 1) {
            ctx_.writer() << "  // case";
        }
    }
    ctx_.writer() << "\n    > data;\n";
    ctx_ << blank;

    // Generate accessor methods: WrapperT* as_<field>() { return std::get_if<WrapperT>(&data); }
    for (size_t i = 0; i < cmd.case_types.size(); ++i) {
        std::string field_name = cmd.case_field_names[i];
        std::string wrapper_name = "Case" + std::to_string(i) + "_" + field_name;
        ctx_ << wrapper_name + "* as_" + field_name + "() { return std::get_if<" + wrapper_name + ">(&data); }" << endl;
        ctx_ << "const " + wrapper_name + "* as_" + field_name + "() const { return std::get_if<" + wrapper_name + ">(&data); }" << endl;
    }
    ctx_ << blank;
}

void CppRenderer::render_end_choice(const EndChoiceCommand& cmd) {
    (void)cmd;
    ctx_.end_struct();
    in_struct_ = false;
    current_struct_name_.clear();
}

void CppRenderer::render_start_choice_case(const StartChoiceCaseCommand& cmd) {
    // Render if/else-if condition for case matching
    std::string condition;

    // Check for range-based selector mode
    if (cmd.selector_mode != ir::case_selector_mode::exact) {
        // Range-based case: generate comparison like "selector_value >= 0x80"
        if (cmd.range_bound) {
            std::string op;
            switch (cmd.selector_mode) {
                case ir::case_selector_mode::ge: op = ">="; break;
                case ir::case_selector_mode::gt: op = ">"; break;
                case ir::case_selector_mode::le: op = "<="; break;
                case ir::case_selector_mode::lt: op = "<"; break;
                case ir::case_selector_mode::ne: op = "!="; break;
                default: op = "=="; break;  // Should not happen
            }
            condition = "selector_value " + op + " (" + render_expression(cmd.range_bound) + ")";
        }
    } else if (!cmd.case_values.empty()) {
        // Exact match case: generate condition like "selector_value == (value1) || ..."
        for (size_t i = 0; i < cmd.case_values.size(); ++i) {
            if (i > 0) condition += " || ";
            condition += "selector_value == (" + render_expression(cmd.case_values[i]) + ")";
        }
    }

    if (!condition.empty()) {
        // Use "if" for first case, "else if" for subsequent cases
        if (in_choice_ && first_choice_case_) {
            ctx_.start_if(condition);
            first_choice_case_ = false;
        } else if (in_choice_) {
            ctx_.start_else_if(condition);
        }
    } else if (in_choice_ && !first_choice_case_) {
        // Default case (empty case_values and not range) - generate "else" block
        ctx_.start_else();
    }
}

void CppRenderer::render_end_choice_case(const EndChoiceCaseCommand& cmd) {
    (void)cmd;
    // No-op: the closing brace is handled by next start_else_if or final end_if/else
}

// ============================================================================
// Method Commands
// ============================================================================

void CppRenderer::render_start_method(const StartMethodCommand& cmd) {
    in_method_ = true;
    label_counter_ = 0;  // Reset for each method to ensure unique label variable names
    // For struct reader methods, we always use obj. prefix since we declare a local obj variable
    expr_context_.in_struct_method = (cmd.kind == StartMethodCommand::MethodKind::StructReader);
    expr_context_.object_name = "obj";

    // For union field readers, field references should use parent-> prefix
    expr_context_.use_parent_context = (cmd.kind == StartMethodCommand::MethodKind::UnionFieldReader);

    // Save method context for proper return value formatting
    current_method_kind_ = cmd.kind;
    current_method_target_struct_ = cmd.target_struct;
    current_method_use_exceptions_ = cmd.use_exceptions;

    ctx_ << blank;

    // For choice readers, emit template declaration and initialize choice state
    if (cmd.kind == StartMethodCommand::MethodKind::ChoiceReader) {
        // Only emit template for external discriminator choices
        bool is_inline_discriminator = cmd.target_choice &&
                                       cmd.target_choice->inferred_discriminator_type.has_value();
        if (!is_inline_discriminator) {
            ctx_ << "template<typename SelectorType>" << endl;
        }
        in_choice_ = true;
        first_choice_case_ = true;  // Reset for first case
    }

    // For union readers and field readers, emit template declaration for parent context
    if (cmd.kind == StartMethodCommand::MethodKind::UnionReader ||
        cmd.kind == StartMethodCommand::MethodKind::UnionFieldReader) {
        ctx_ << "template<typename ParentT = void>" << endl;
    }

    std::ostringstream signature;
    if (cmd.is_static) {
        signature << "static ";
    }

    // Format return type based on method kind and error handling (C++-specific)
    std::string return_type;
    switch (cmd.kind) {
        case StartMethodCommand::MethodKind::StructReader:
            if (cmd.target_struct) {
                return_type = cmd.use_exceptions
                    ? cmd.target_struct->name
                    : "ReadResult<" + cmd.target_struct->name + ">";
            } else {
                return_type = "void";  // Fallback
            }
            break;
        case StartMethodCommand::MethodKind::UnionReader:
            // Union read() returns the union type itself
            return_type = current_struct_name_;  // Union name from context
            break;
        case StartMethodCommand::MethodKind::UnionFieldReader:
            if (cmd.return_type) {
                std::string field_type = ir_type_to_cpp(cmd.return_type);
                return_type = cmd.use_exceptions
                    ? field_type
                    : "ReadResult<" + field_type + ">";
            } else {
                return_type = "void";  // Fallback
            }
            break;
        case StartMethodCommand::MethodKind::ChoiceReader:
            // Choice readers return the choice type (wrapped in ReadResult if safe mode)
            return_type = cmd.use_exceptions
                ? current_struct_name_
                : "ReadResult<" + current_struct_name_ + ">";
            break;
        case StartMethodCommand::MethodKind::StandaloneReader:
            if (cmd.target_struct) {
                return_type = cmd.use_exceptions
                    ? cmd.target_struct->name
                    : "std::optional<" + cmd.target_struct->name + ">";
            } else {
                return_type = "void";  // Fallback
            }
            break;
        case StartMethodCommand::MethodKind::StructWriter:
            return_type = cmd.use_exceptions ? "void" : "bool";
            break;
        case StartMethodCommand::MethodKind::UserFunction:
            // User-defined functions: return type comes from IR
            if (cmd.return_type) {
                return_type = ir_type_to_cpp(cmd.return_type);
            } else {
                return_type = "void";  // Fallback
            }
            break;
        case StartMethodCommand::MethodKind::Custom:
            return_type = "void";  // Default for custom methods
            break;
    }

    signature << return_type << " " << cmd.method_name << "(";

    // Format parameters based on method kind (C++-specific)
    switch (cmd.kind) {
        case StartMethodCommand::MethodKind::StructReader:
        case StartMethodCommand::MethodKind::StandaloneReader:
            signature << "const uint8_t*& data, const uint8_t* end";
            break;
        case StartMethodCommand::MethodKind::UnionReader:
        case StartMethodCommand::MethodKind::UnionFieldReader:
            // Union readers have an optional parent parameter for constraint evaluation
            signature << "const uint8_t*& data, const uint8_t* end, const ParentT* parent = nullptr";
            break;
        case StartMethodCommand::MethodKind::ChoiceReader: {
            // Choice readers may have an additional selector_value parameter
            // (only for external discriminator choices)
            bool is_inline_discriminator = cmd.target_choice &&
                                           cmd.target_choice->inferred_discriminator_type.has_value();
            if (is_inline_discriminator) {
                signature << "const uint8_t*& data, const uint8_t* end";
            } else {
                signature << "const uint8_t*& data, const uint8_t* end, SelectorType selector_value";
            }
            break;
        }
        case StartMethodCommand::MethodKind::StructWriter:
            signature << "const uint8_t*& data, const uint8_t* end";
            break;
        case StartMethodCommand::MethodKind::UserFunction:
            // User-defined functions: render parameters from IR
            if (cmd.parameters) {
                for (size_t i = 0; i < cmd.parameters->size(); ++i) {
                    if (i > 0) signature << ", ";
                    const auto& param = (*cmd.parameters)[i];
                    signature << ir_type_to_cpp(&param.param_type) << " " << param.name;
                }
            }
            break;
        case StartMethodCommand::MethodKind::Custom:
            // No parameters for custom methods by default
            break;
    }

    // Add const qualifier for user-defined functions
    signature << ")";
    if (cmd.kind == StartMethodCommand::MethodKind::UserFunction) {
        signature << " const";
    }
    signature << " {";

    ctx_ << signature.str() << endl;
    ctx_.writer().indent();

    // For safe-mode struct readers, declare the result variable
    if (cmd.kind == StartMethodCommand::MethodKind::StructReader &&
        !cmd.use_exceptions && cmd.target_struct) {
        ctx_ << "ReadResult<" + cmd.target_struct->name + "> result;" << endl;
    }

    // For safe-mode choice readers, declare the result variable
    if (cmd.kind == StartMethodCommand::MethodKind::ChoiceReader && !cmd.use_exceptions) {
        ctx_ << "ReadResult<" + current_struct_name_ + "> result;" << endl;
    }

    // For struct readers, save the start pointer for absolute label offsets
    // Labels are relative to the beginning of the enclosing struct (per specification)
    if (cmd.kind == StartMethodCommand::MethodKind::StructReader) {
        ctx_ << "const uint8_t* start = data;  // Save start for absolute label offsets" << endl;
    }
}

void CppRenderer::render_end_method(const EndMethodCommand& cmd) {
    (void)cmd;
    ctx_.writer().unindent();
    ctx_ << "}" << endl;
    in_method_ = false;
    expr_context_.in_struct_method = false;

    // Clear method context
    current_method_kind_ = StartMethodCommand::MethodKind::Custom;
    current_method_target_struct_ = nullptr;
    current_method_use_exceptions_ = false;

    // Clear choice context
    in_choice_ = false;
    first_choice_case_ = false;
}

void CppRenderer::render_return_value(const ReturnValueCommand& cmd) {
    std::string return_expr = cmd.value;

    // For safe-mode struct readers, assign obj to result.value before returning
    if (current_method_kind_ == StartMethodCommand::MethodKind::StructReader &&
        !current_method_use_exceptions_ && return_expr == "result") {
        ctx_ << "result.value = obj;" << endl;
    }

    // For safe-mode choice readers, assign obj to result.value before returning
    if (current_method_kind_ == StartMethodCommand::MethodKind::ChoiceReader &&
        !current_method_use_exceptions_ && return_expr == "result") {
        ctx_ << "result.value = obj;" << endl;
    }

    // Wrap return value based on method kind and error handling strategy (C++-specific)
    if (current_method_kind_ == StartMethodCommand::MethodKind::StandaloneReader &&
        !current_method_use_exceptions_) {
        // Safe mode standalone reader returns std::optional<T>
        return_expr = "std::optional(" + return_expr + ")";
    }

    ctx_ << "return " + return_expr + ";" << endl;
}

void CppRenderer::render_return_expression(const ReturnExpressionCommand& cmd) {
    std::string expr = render_expression(cmd.expression);
    ctx_ << "return " + expr + ";" << endl;
}

// ============================================================================
// Reading Commands
// ============================================================================

void CppRenderer::render_read_field(const ReadFieldCommand& cmd) {
    std::string target = expr_context_.in_struct_method
        ? expr_context_.object_name + "." + cmd.field_name
        : cmd.field_name;

    std::string read_call = generate_read_call(cmd.field_type, cmd.use_exceptions);

    // For union field readers with exceptions: use auto with initialization
    // This produces: auto field_name = FieldType::read(data, end);
    if (current_method_kind_ == StartMethodCommand::MethodKind::UnionFieldReader && cmd.use_exceptions) {
        ctx_ << "auto " + cmd.field_name + " = " + read_call + ";" << endl;
    } else {
        // Default: simple assignment (variable already declared elsewhere)
        ctx_ << target + " = " + read_call + ";" << endl;
    }
}

void CppRenderer::render_read_array_element(const ReadArrayElementCommand& cmd) {
    std::string read_call = generate_read_call(cmd.element_type, cmd.use_exceptions);

    // For primitive types and enums, always just assign
    // For struct types in safe mode, the read_safe returns ReadResult which needs checking
    bool is_struct_or_union = (cmd.element_type->kind == ir::type_kind::struct_type ||
                               cmd.element_type->kind == ir::type_kind::union_type ||
                               cmd.element_type->kind == ir::type_kind::choice_type);

    if (cmd.use_exceptions || !is_struct_or_union) {
        ctx_ << cmd.element_name + " = " + read_call + ";" << endl;
    } else {
        // Struct types in safe mode return ReadResult, need to check it
        ctx_ << "auto result = " + read_call + ";" << endl;
        ctx_.start_if("!result");
        ctx_ << "return false;" << endl;
        ctx_.end_if();
        ctx_ << cmd.element_name + " = result.value;" << endl;
    }
}

void CppRenderer::render_seek_to_label(const SeekToLabelCommand& cmd) {
    std::string label_expr = render_expression(cmd.label_expr);

    // Generate unique variable name for this label
    std::string label_var = "label_pos" + std::to_string(label_counter_++);

    // Calculate label position (absolute offset from struct start, per specification)
    ctx_ << "const uint8_t* " + label_var + " = start + " + label_expr + ";" << endl;

    // Bounds check
    ctx_.start_if(label_var + " < start || " + label_var + " > end");
    if (cmd.use_exceptions) {
        ctx_ << "throw std::runtime_error(\"Label position out of bounds\");" << endl;
    } else {
        ctx_ << "result.error_message = \"Label position out of bounds\";" << endl;
        ctx_ << "return result;" << endl;
    }
    ctx_.end_if();

    // Seek to the labeled position
    ctx_ << "data = " + label_var + ";" << endl;
}

void CppRenderer::render_align_pointer(const AlignPointerCommand& cmd) {
    uint64_t align = cmd.alignment;
    uint64_t mask = align - 1;

    // Align data pointer to N-byte boundary relative to start of buffer,
    // NOT to absolute memory address. This ensures consistent parsing
    // regardless of where the buffer is allocated in memory.
    ctx_.start_scope();
    ctx_ << "size_t offset = data - start;" << endl;
    ctx_ << "size_t aligned_offset = (offset + " + std::to_string(mask) + ") & ~size_t(" + std::to_string(mask) + ");" << endl;
    ctx_ << "data = start + aligned_offset;" << endl;
    ctx_.end_scope();
}

void CppRenderer::render_resize_array(const ResizeArrayCommand& cmd) {
    std::string size_expr = render_expression(cmd.size_expr);
    std::string target = expr_context_.in_struct_method
        ? expr_context_.object_name + "." + cmd.array_name
        : cmd.array_name;

    ctx_ << target + ".resize(" + size_expr + ");" << endl;
}

void CppRenderer::render_append_to_array(const AppendToArrayCommand& cmd) {
    std::string target = expr_context_.in_struct_method
        ? expr_context_.object_name + "." + cmd.array_name
        : cmd.array_name;

    std::string read_call = generate_read_call(cmd.element_type, cmd.use_exceptions);

    // Check if this is a string or struct type that returns a result in safe mode
    bool is_string = (cmd.element_type->kind == ir::type_kind::string);
    bool is_struct_or_union = (cmd.element_type->kind == ir::type_kind::struct_type ||
                               cmd.element_type->kind == ir::type_kind::union_type ||
                               cmd.element_type->kind == ir::type_kind::choice_type);
    bool needs_result = !cmd.use_exceptions && (is_string || is_struct_or_union);

    if (needs_result) {
        // For safe mode with strings/structs, store result and dereference
        std::string result_var = is_string ? "str_result" : "nested_result";
        ctx_ << "auto " + result_var + " = " + read_call + ";" << endl;
        ctx_ << target + ".push_back(*" + result_var + ");" << endl;
    } else {
        // For primitives or exception mode, direct push
        ctx_ << target + ".push_back(" + read_call + ");" << endl;
    }
}

// ============================================================================
// Control Flow Commands
// ============================================================================

void CppRenderer::render_start_loop(const StartLoopCommand& cmd) {
    std::string count_expr = render_expression(cmd.count_expr);
    if (cmd.use_size_method) {
        count_expr += ".size()";
    }
    std::string init = "size_t " + cmd.loop_var + " = 0";
    std::string condition = cmd.loop_var + " < " + count_expr;
    std::string increment = cmd.loop_var + "++";
    ctx_.start_for(init, condition, increment);
}

void CppRenderer::render_end_loop(const EndLoopCommand& cmd) {
    (void)cmd;
    ctx_.end_for();
}

void CppRenderer::render_start_while_loop(const StartWhileLoopCommand& cmd) {
    ctx_.start_while(cmd.condition);
}

void CppRenderer::render_if(const IfCommand& cmd) {
    std::string condition = render_expression(cmd.condition);
    ctx_.start_if(condition);
}

void CppRenderer::render_else(const ElseCommand& cmd) {
    (void)cmd;
    ctx_.start_else();
}

void CppRenderer::render_end_if(const EndIfCommand& cmd) {
    (void)cmd;
    ctx_.end_if();
}

void CppRenderer::render_start_scope(const StartScopeCommand& cmd) {
    (void)cmd;
    ctx_.start_scope();
}

void CppRenderer::render_end_scope(const EndScopeCommand& cmd) {
    (void)cmd;
    ctx_.end_scope();
}

// ============================================================================
// Trial-and-Error Decoding Commands (for unions)
// ============================================================================

void CppRenderer::render_save_position(const SavePositionCommand& cmd) {
    ctx_ << "const uint8_t* " + cmd.var_name + " = data;  // Save position for backtracking" << endl;
}

void CppRenderer::render_restore_position(const RestorePositionCommand& cmd) {
    ctx_ << "data = " + cmd.var_name + ";  // Restore position for next branch attempt" << endl;
}

void CppRenderer::render_start_try_branch(const StartTryBranchCommand& cmd) {
    ctx_ << "// Try branch: " + cmd.branch_name << endl;
    ctx_.start_try();
    // Emit variant assignment for union branch attempt
    ctx_ << "result.value = read_as_" + cmd.branch_name + "(data, end, parent);" << endl;
}

void CppRenderer::render_end_try_branch(const EndTryBranchCommand& cmd) {
    ctx_.start_catch("const ConstraintViolation&", "");
    if (cmd.is_last_branch) {
        if (cmd.is_optional_union) {
            ctx_ << "// Last branch failed - union remains empty (std::monostate)" << endl;
        } else {
            ctx_ << "// Last branch failed - rethrow to propagate error" << endl;
            ctx_ << "throw;" << endl;
        }
    } else {
        ctx_ << "// Branch failed, try next" << endl;
    }
    ctx_.end_try();
}

// ============================================================================
// Validation Commands
// ============================================================================

void CppRenderer::render_bounds_check(const BoundsCheckCommand& cmd) {
    std::string value = render_expression(cmd.value_expr);
    std::string min_val = render_expression(cmd.min_expr);
    std::string max_val = render_expression(cmd.max_expr);

    std::string condition = value + " < " + min_val + " || " + value + " > " + max_val;
    generate_error_check(condition, cmd.error_message, cmd.use_exceptions);
}

void CppRenderer::render_constraint_check(const ConstraintCheckCommand& cmd) {
    // Set current field name for union self-references
    std::string saved_field_name = expr_context_.current_field_name;
    if (!cmd.field_name.empty()) {
        expr_context_.current_field_name = cmd.field_name;
    }

    std::string rendered_expr = render_expression(cmd.condition);
    std::string condition = "!(" + rendered_expr + ")";

    // Restore previous field name
    expr_context_.current_field_name = saved_field_name;

    // Check if constraint references parent - if so, wrap in null check
    bool references_parent = (rendered_expr.find("parent->") != std::string::npos);

    if (references_parent) {
        ctx_.start_if("parent != nullptr");
    }

    ctx_.start_if(condition);

    if (cmd.use_exceptions) {
        // Exception mode: throw ConstraintViolation
        ctx_ << "throw ConstraintViolation(\"" + cmd.error_message + "\");" << endl;
    } else {
        // Safe mode: set error message and return failure
        ctx_ << "result.error_message = \"" + cmd.error_message + "\";" << endl;
        ctx_ << "return result;" << endl;
    }

    ctx_.end_if();

    if (references_parent) {
        ctx_.end_if();
    }
}

// ============================================================================
// Variable Commands
// ============================================================================

void CppRenderer::render_declare_variable(const DeclareVariableCommand& cmd) {
    std::string type_str;

    // Convert semantic variable kind to C++ type string
    switch (cmd.kind) {
        case DeclareVariableCommand::VariableKind::StructInstance:
            if (!cmd.struct_type) {
                throw invalid_ir_error("Null struct_type in StructInstance variable declaration");
            }
            type_str = cmd.struct_type->name;
            break;
        case DeclareVariableCommand::VariableKind::LocalVariable:
            if (cmd.type) {
                type_str = ir_type_to_cpp(cmd.type);
            } else {
                // nullptr type means use "auto" for type deduction
                type_str = "auto";
            }
            break;
        case DeclareVariableCommand::VariableKind::Custom:
            type_str = cmd.custom_type_name;
            break;
    }

    if (cmd.init_expr) {
        std::string init = render_expression(cmd.init_expr);
        ctx_ << type_str + " " + cmd.var_name + " = " + init + ";" << endl;
    } else {
        // Use value initialization {} to zero-initialize all members
        // This prevents "used uninitialized" warnings for conditional fields
        ctx_ << type_str + " " + cmd.var_name + "{};" << endl;
    }

    // Add to expression context so subsequent expressions know this is a local variable
    expr_context_.variable_names[cmd.var_name] = cmd.var_name;
}

void CppRenderer::render_assign_variable(const AssignVariableCommand& cmd) {
    std::string value = render_expression(cmd.value_expr);
    ctx_ << cmd.target + " = " + value + ";" << endl;
}

void CppRenderer::render_assign_choice_wrapper(const AssignChoiceWrapperCommand& cmd) {
    ctx_ << cmd.target + " = " + cmd.wrapper_type + "{" + cmd.source_var + "};" << endl;
}

// ============================================================================
// Error Handling Commands
// ============================================================================

void CppRenderer::render_set_error_message(const SetErrorMessageCommand& cmd) {
    std::string message = render_expression(cmd.message_expr);
    ctx_ << "// TODO: Set error message: " + message << endl;
}

void CppRenderer::render_throw_exception(const ThrowExceptionCommand& cmd) {
    std::string message = render_expression(cmd.message_expr);
    ctx_ << "throw " + cmd.exception_type + "(" + message + ");" << endl;
}

void CppRenderer::render_declare_result_variable(const DeclareResultVariableCommand& cmd) {
    // Generate: ReadResult<StructName> result;
    std::string struct_name = cmd.struct_type->name;
    ctx_ << "ReadResult<" + struct_name + "> " + cmd.result_name + ";" << endl;
}

void CppRenderer::render_set_result_success(const SetResultSuccessCommand& cmd) {
    // Generate: result.success = true;
    ctx_ << cmd.result_name + ".success = true;" << endl;
}

void CppRenderer::render_comment(const CommentCommand& cmd) {
    ctx_ << "// " + cmd.text << endl;
}

void CppRenderer::render_read_primitive_to_variable(const ReadPrimitiveToVariableCommand& cmd) {
    // Generate: target_var = read_uint8(data, end); (with bounds checking)
    std::string read_func = get_read_function_name(cmd.primitive_type);
    ctx_ << cmd.target_var + " = " + read_func + "(data, end);" << endl;
}

void CppRenderer::render_extract_bitfield(const ExtractBitfieldCommand& cmd) {
    // Generate bitfield extraction with proper bit manipulation
    std::ostringstream extraction;

    if (cmd.source_var2.has_value()) {
        // Multi-byte bitfield: combine bits from two bytes
        // (((_bitfield_byte0 >> bit_offset) & first_mask) | ((_bitfield_byte1 & second_mask) << bits_in_first_byte))
        extraction << "(((" << cmd.source_var << " >> " << cmd.bit_offset << ") & 0x"
                   << std::hex << std::uppercase << cmd.first_mask.value() << ") | "
                   << "((" << cmd.source_var2.value() << " & 0x"
                   << std::hex << std::uppercase << cmd.second_mask.value() << ") << "
                   << std::dec << cmd.bits_in_first_byte.value() << "))";
    } else if (cmd.bit_offset == 0) {
        // Simple case: starts at byte boundary, no shift needed
        // (_bitfield_byte0 & 0xF)
        extraction << "(" << cmd.source_var << " & 0x"
                   << std::hex << std::uppercase << cmd.mask << ")";
    } else {
        // Single byte with offset: needs shift
        // ((_bitfield_byte0 >> 4) & 0xF)
        extraction << "((" << cmd.source_var << " >> " << std::dec << cmd.bit_offset
                   << ") & 0x" << std::hex << std::uppercase << cmd.mask << ")";
    }

    ctx_ << cmd.target_field + " = " + extraction.str() + ";" << endl;
}

// ============================================================================
// Helper: Map primitive type to read function name
// ============================================================================

std::string CppRenderer::get_read_function_name(ir::type_kind type) {
    // Map primitive types to their read function names
    // Note: For bitfield reads, we always use little-endian uint8
    switch (type) {
        case ir::type_kind::uint8:  return "read_uint8";
        case ir::type_kind::uint16: return "read_uint16_le";
        case ir::type_kind::uint32: return "read_uint32_le";
        case ir::type_kind::uint64: return "read_uint64_le";
        case ir::type_kind::int8:   return "read_int8";
        case ir::type_kind::int16:  return "read_int16_le";
        case ir::type_kind::int32:  return "read_int32_le";
        case ir::type_kind::int64:  return "read_int64_le";
        default:
            return "/* unknown type */";
    }
}

// ============================================================================
// Code Generation Helpers
// ============================================================================

void CppRenderer::emit_helper_functions() {
    // Delegate to CppHelperGenerator for cleaner separation of concerns
    CppHelperGenerator helper_gen(ctx_, error_handling_mode_);
    helper_gen.generate_all();
}

std::string CppRenderer::generate_read_call(const ir::type_ref* type, bool use_exceptions) {
    // Generate read function calls based on type
    (void)use_exceptions;  // Will use this for error handling variations in the future

    if (!type) {
        throw invalid_ir_error("Null type pointer in generate_read_call()");
    }

    // Handle enum types - cast from base type
    if (type->kind == ir::type_kind::enum_type) {
        if (module_ && type->type_index && *type->type_index < module_->enums.size()) {
            const auto& enum_def = module_->enums[*type->type_index];
            std::string base_read = generate_read_call(&enum_def.base_type, use_exceptions);
            return "static_cast<" + enum_def.name + ">(" + base_read + ")";
        }
        return "/* enum read error */";
    }

    // Handle bitfield types - read appropriate size and mask
    if (type->kind == ir::type_kind::bitfield && type->bit_width.has_value()) {
        size_t bits = *type->bit_width;
        uint64_t mask = (1ULL << bits) - 1;
        std::ostringstream oss;
        oss << std::hex << std::uppercase << mask;

        // Choose read function based on bit width (with bounds checking)
        std::string read_func = get_bitfield_read_function(bits) + "(data, end)";

        return "(" + read_func + " & 0x" + oss.str() + ")";
    }

    // Get endianness suffix for multi-byte types
    auto get_endian_suffix = [&]() -> std::string {
        if (!type->byte_order) return "_le";  // Default to little endian
        switch (*type->byte_order) {
            case ir::endianness::little: return "_le";
            case ir::endianness::big: return "_be";
            case ir::endianness::native: return "";
            default: return "_le";
        }
    };

    // Map primitive types to read functions with endianness and bounds checking
    switch (type->kind) {
        case ir::type_kind::uint8:  return "read_uint8(data, end)";
        case ir::type_kind::uint16: return "read_uint16" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::uint32: return "read_uint32" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::uint64: return "read_uint64" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::int8:   return "read_int8(data, end)";
        case ir::type_kind::int16:  return "read_int16" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::int32:  return "read_int32" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::int64:  return "read_int64" + get_endian_suffix() + "(data, end)";
        case ir::type_kind::boolean: return "read_uint8(data, end) != 0";
        default:
            break;
    }

    // For struct/union/choice types, call their static read method
    std::string cpp_type = ir_type_to_cpp(type);
    if (type->kind == ir::type_kind::struct_type ||
        type->kind == ir::type_kind::union_type) {
        // Use read_safe for safe mode, read for exception mode
        std::string method_name = use_exceptions ? "read" : "read_safe";

        // For unions, pass parent object for constraint evaluation
        if (type->kind == ir::type_kind::union_type && expr_context_.in_struct_method) {
            return cpp_type + "::" + method_name + "(data, end, &" + expr_context_.object_name + ")";
        }

        return cpp_type + "::" + method_name + "(data, end)";
    }

    // For choice types, call with or without selector_value parameter
    if (type->kind == ir::type_kind::choice_type) {
        std::string method_name = use_exceptions ? "read" : "read_safe";

        // Check if this is an inline discriminator choice
        bool is_inline_discriminator = false;
        if (module_ && type->type_index && *type->type_index < module_->choices.size()) {
            const auto& choice_def = module_->choices[*type->type_index];
            is_inline_discriminator = choice_def.inferred_discriminator_type.has_value();
        }

        // If explicit selector arguments are provided (parameterized choice instantiation),
        // render those expressions. Otherwise, for external discriminator choices,
        // use default selector_value from context. For inline discriminator choices,
        // don't pass any selector argument.
        if (is_inline_discriminator) {
            // Inline discriminator - no selector parameter
            return cpp_type + "::" + method_name + "(data, end)";
        }

        std::string selector_args;
        if (!type->choice_selector_args.empty()) {
            for (size_t i = 0; i < type->choice_selector_args.size(); ++i) {
                if (i > 0) selector_args += ", ";
                selector_args += render_expression(type->choice_selector_args[i].get());
            }
        } else {
            selector_args = "selector_value";
        }

        return cpp_type + "::" + method_name + "(data, end, " + selector_args + ")";
    }

    // For strings, use read_string or read_string_safe
    if (type->kind == ir::type_kind::string) {
        return use_exceptions ? "read_string(data, end)" : "read_string_safe(data, end)";
    }

    // For UTF-16 strings, use endianness-specific reader
    if (type->kind == ir::type_kind::u16_string) {
        std::string func_name;
        if (type->byte_order.has_value() && *type->byte_order == ir::endianness::big) {
            func_name = "read_u16string_be";
        } else {
            func_name = "read_u16string_le";  // Default to little-endian
        }
        return func_name + "(data, end)";
    }

    // For UTF-32 strings, use endianness-specific reader
    if (type->kind == ir::type_kind::u32_string) {
        std::string func_name;
        if (type->byte_order.has_value() && *type->byte_order == ir::endianness::big) {
            func_name = "read_u32string_be";
        } else {
            func_name = "read_u32string_le";  // Default to little-endian
        }
        return func_name + "(data, end)";
    }

    // For subtypes, use read_SubtypeName or read_SubtypeName_safe
    if (type->kind == ir::type_kind::subtype_ref) {
        std::string func_name = use_exceptions ? "read_" + cpp_type : "read_" + cpp_type + "_safe";
        return func_name + "(data, end)";
    }

    return cpp_type + "::read(data, end)";
}

void CppRenderer::generate_error_check(const std::string& error_condition,
                                       const std::string& error_message,
                                       bool use_exceptions) {
    ctx_.start_if(error_condition);
    if (use_exceptions) {
        ctx_ << "throw std::runtime_error(\"" + error_message + "\");" << endl;
    } else {
        ctx_ << "// Error: " + error_message << endl;
        ctx_ << "return false;" << endl;
    }
    ctx_.end_if();
}

void CppRenderer::generate_includes() {
    ctx_ << "#include <cstdint>" << endl;
    ctx_ << "#include <string>" << endl;
    ctx_ << "#include <vector>" << endl;
    ctx_ << "#include <stdexcept>" << endl;
    ctx_ << blank;
}

void CppRenderer::generate_namespace_open() {
    if (!namespace_.empty()) {
        ctx_.start_namespace(namespace_);
    }
}

void CppRenderer::generate_namespace_close() {
    if (!namespace_.empty()) {
        ctx_.end_namespace();
    }
}

// ============================================================================
// Type Conversion (IR → C++)
// ============================================================================

std::string CppRenderer::ir_type_to_cpp(const ir::type_ref* type) const {
    if (!type) {
        throw invalid_ir_error("Null type pointer in ir_type_to_cpp()");
    }

    // Check cache first - avoids redundant conversions for deeply nested types
    auto it = type_name_cache_.find(type);
    if (it != type_name_cache_.end()) {
        return it->second;
    }

    // Cache miss - compute the type name
    std::string result;

    // Check for array types first
    if (type->element_type) {
        std::string element_type = ir_type_to_cpp(type->element_type.get());

        // Fixed-size arrays use std::array
        if (type->kind == ir::type_kind::array_fixed && type->array_size.has_value()) {
            // Check if size comes from a constant reference - preserve constant name
            std::string size_str;
            if (type->array_size_expr &&
                (type->array_size_expr->type == ir::expr::constant_ref ||
                 type->array_size_expr->type == ir::expr::parameter_ref)) {
                size_str = type->array_size_expr->ref_name;
            } else {
                size_str = std::to_string(*type->array_size);
            }
            result = "std::array<" + element_type + ", " + size_str + ">";
        } else {
            // Variable/ranged arrays use std::vector
            result = "std::vector<" + element_type + ">";
        }
    } else {
        // Check for primitive types
        switch (type->kind) {
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
            result = get_primitive_cpp_type(type);
            break;

        case ir::type_kind::string:
            result = "std::string";
            break;

        case ir::type_kind::u16_string:
            result = "std::u16string";
            break;

        case ir::type_kind::u32_string:
            result = "std::u32string";
            break;

        case ir::type_kind::boolean:
            result = "bool";
            break;

        case ir::type_kind::bitfield:
            // Bit fields use smallest integer type that fits
            if (type->bit_width.has_value()) {
                result = get_bitfield_cpp_type(*type->bit_width);
            } else {
                result = "uint8_t";  // Default
            }
            break;

        case ir::type_kind::struct_type:
            if (module_ && type->type_index && *type->type_index < module_->structs.size()) {
                result = module_->structs[*type->type_index].name;
            } else {
                result = "/* struct type */";
            }
            break;

        case ir::type_kind::union_type:
            if (module_ && type->type_index && *type->type_index < module_->unions.size()) {
                result = module_->unions[*type->type_index].name;
            } else {
                result = "/* union type */";
            }
            break;

        case ir::type_kind::enum_type:
            if (module_ && type->type_index && *type->type_index < module_->enums.size()) {
                result = module_->enums[*type->type_index].name;
            } else {
                result = "/* enum type */";
            }
            break;

        case ir::type_kind::choice_type:
            if (module_ && type->type_index && *type->type_index < module_->choices.size()) {
                result = module_->choices[*type->type_index].name;
            } else {
                result = "/* choice type */";
            }
            break;

        case ir::type_kind::subtype_ref:
            if (module_ && type->type_index && *type->type_index < module_->subtypes.size()) {
                result = module_->subtypes[*type->type_index].name;
            } else {
                result = "/* subtype */";
            }
            break;

        default:
            result = "/* unknown type */";
            break;
        }
    }

    // Store in cache and return
    type_name_cache_[type] = result;
    return result;
}

std::string CppRenderer::get_primitive_cpp_type(const ir::type_ref* type) const {
    if (!type) {
        throw invalid_ir_error("Null type pointer in get_primitive_cpp_type()");
    }

    switch (type->kind) {
        case ir::type_kind::uint8:   return "uint8_t";
        case ir::type_kind::uint16:  return "uint16_t";
        case ir::type_kind::uint32:  return "uint32_t";
        case ir::type_kind::uint64:  return "uint64_t";
        case ir::type_kind::uint128: return "uint128_t";  // May not exist in C++
        case ir::type_kind::int8:    return "int8_t";
        case ir::type_kind::int16:   return "int16_t";
        case ir::type_kind::int32:   return "int32_t";
        case ir::type_kind::int64:   return "int64_t";
        case ir::type_kind::int128:  return "int128_t";   // May not exist in C++
        default:
            return "/* not a primitive */";
    }
}

// ============================================================================
// Bitfield Helper Functions
// ============================================================================

std::string CppRenderer::get_bitfield_cpp_type(size_t bits) const {
    if (bits <= bitfield_limits::UINT8_MAX_BITS) {
        return "uint8_t";
    } else if (bits <= bitfield_limits::UINT16_MAX_BITS) {
        return "uint16_t";
    } else if (bits <= bitfield_limits::UINT32_MAX_BITS) {
        return "uint32_t";
    } else {
        return "uint64_t";
    }
}

std::string CppRenderer::get_bitfield_read_function(size_t bits) const {
    if (bits <= bitfield_limits::UINT8_MAX_BITS) {
        return "read_uint8";
    } else if (bits <= bitfield_limits::UINT16_MAX_BITS) {
        return "read_uint16_le";
    } else if (bits <= bitfield_limits::UINT32_MAX_BITS) {
        return "read_uint32_le";
    } else {
        return "read_uint64_le";
    }
}

// ============================================================================
// Public API Functions
// ============================================================================

std::string generate_cpp_header(const ir::bundle& ir, const cpp_options& opts) {
    // Use command stream architecture for all code generation
    CommandBuilder builder;

    // Determine error handling mode
    bool use_exceptions = (opts.error_handling != cpp_options::results_only);

    // Determine namespace: use package name from IR if namespace is default "generated"
    std::string namespace_name = opts.namespace_name;
    if (namespace_name == "generated" && !ir.name.empty()) {
        // Convert package name (com.example.foo) to C++ namespace (com::example::foo)
        namespace_name = ir.name;
        std::replace(namespace_name.begin(), namespace_name.end(), '.', ':');
        // Convert single : to :: for C++ namespace separator
        std::string result;
        for (char c : namespace_name) {
            result += c;
            if (c == ':') result += ':';
        }
        namespace_name = result;
    }

    // Build commands for the entire module
    auto commands = builder.build_module(ir, namespace_name, opts, use_exceptions);

    // Render commands to C++ code
    CppRenderer renderer;
    renderer.set_module(&ir);  // Set module for type name resolution
    renderer.set_error_handling_mode(opts.error_handling);  // Set error handling mode
    renderer.render_commands(commands);

    return renderer.get_output();
}

std::string generate_cpp_struct(const ir::struct_def& struct_def, bool use_exceptions) {
    // Single struct generation using command stream architecture
    CommandBuilder builder;

    // Build commands for struct with read method
    auto commands = builder.build_struct_reader(struct_def, use_exceptions);

    // Render to C++ code
    CppRenderer renderer;
    renderer.render_commands(commands);

    return renderer.get_output();
}

} // namespace datascript::codegen

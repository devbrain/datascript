#pragma once

#include <memory>
#include <string>
#include <vector>
#include <optional>
#include <datascript/ir.hh>
#include <datascript/codegen.hh>

namespace datascript::codegen {

// ============================================================================
// Source Information
// ============================================================================

struct SourceInfo {
    std::string file;
    int line;
    int column;

    std::string format() const {
        return file + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

// ============================================================================
// Base Command
// ============================================================================

struct Command {
    enum Type {
        // Module-level
        ModuleStart,
        ModuleEnd,
        NamespaceStart,
        NamespaceEnd,
        IncludeDirective,

        // Constants
        DeclareConstant,

        // Enums
        StartEnum,
        DeclareEnumItem,
        EndEnum,

        // Subtypes
        Subtype,

        // Structure definition
        StartStruct,
        EndStruct,
        DeclareField,

        // Union definition
        StartUnion,
        EndUnion,

        // Choice definition
        StartChoice,
        EndChoice,
        StartChoiceCase,
        EndChoiceCase,

        // Methods
        StartMethod,
        EndMethod,
        ReturnValue,
        ReturnExpression,

        // Reading operations
        ReadField,
        ReadArrayElement,
        SeekToLabel,
        AlignPointer,

        // Array operations
        ResizeArray,
        AppendToArray,

        // Control flow
        StartLoop,
        StartWhileLoop,
        EndLoop,
        If,
        Else,
        EndIf,
        StartScope,
        EndScope,

        // Trial-and-error decoding (for unions)
        SavePosition,
        RestorePosition,
        StartTryBranch,
        EndTryBranch,

        // Validation
        BoundsCheck,
        ConstraintCheck,

        // Variable operations
        DeclareVariable,
        AssignVariable,
        AssignChoiceWrapper,  // For choice variant assignment with wrapper struct
        ReadPrimitiveToVariable,  // Read primitive type into a variable
        ExtractBitfield,  // Extract bitfield from byte variable(s)

        // Error handling
        SetErrorMessage,
        ThrowException,

        // Safe mode result handling
        DeclareResultVariable,
        SetResultSuccess,

        // Comments
        Comment
    };

    Type type;
    SourceInfo source;

    explicit Command(Type t) : type(t), source({"", 0, 0}) {}
    Command(Type t, SourceInfo src) : type(t), source(src) {}
    virtual ~Command() = default;
};

// ============================================================================
// Structure Commands
// ============================================================================

struct StartStructCommand : Command {
    std::string struct_name;
    std::string doc_comment;

    StartStructCommand(const std::string& n, const std::string& doc)
        : Command(StartStruct), struct_name(n), doc_comment(doc) {}
};

struct EndStructCommand : Command {
    EndStructCommand() : Command(EndStruct) {}
};

struct DeclareFieldCommand : Command {
    std::string field_name;
    const ir::type_ref* field_type;  // IR type, not language-specific string
    std::string doc_comment;

    DeclareFieldCommand(const std::string& name, const ir::type_ref* ftype, const std::string& doc)
        : Command(DeclareField), field_name(name), field_type(ftype), doc_comment(doc) {}
};

// ============================================================================
// Union Commands
// ============================================================================

struct StartUnionCommand : Command {
    std::string union_name;
    std::string doc_comment;
    std::vector<const ir::type_ref*> case_types;     // Types for std::variant
    std::vector<std::string> case_field_names;        // Field names for each case
    bool is_optional;                                  // If true, prepend std::monostate to variant

    StartUnionCommand(const std::string& n, const std::string& doc,
                     std::vector<const ir::type_ref*> types = {},
                     std::vector<std::string> field_names = {},
                     bool optional = false)
        : Command(StartUnion), union_name(n), doc_comment(doc),
          case_types(std::move(types)), case_field_names(std::move(field_names)),
          is_optional(optional) {}
};

struct EndUnionCommand : Command {
    EndUnionCommand() : Command(EndUnion) {}
};

// ============================================================================
// Choice Commands
// ============================================================================

struct StartChoiceCommand : Command {
    std::string choice_name;
    std::string doc_comment;
    std::vector<const ir::type_ref*> case_types;  // Types for std::variant
    std::vector<std::string> case_field_names;    // Field names for accessors

    StartChoiceCommand(const std::string& n, const std::string& doc,
                       std::vector<const ir::type_ref*> types,
                       std::vector<std::string> field_names)
        : Command(StartChoice), choice_name(n), doc_comment(doc),
          case_types(std::move(types)), case_field_names(std::move(field_names)) {}
};

struct EndChoiceCommand : Command {
    EndChoiceCommand() : Command(EndChoice) {}
};

struct StartChoiceCaseCommand : Command {
    std::vector<const ir::expr*> case_values;  // Selector values for exact match cases
    ir::case_selector_mode selector_mode = ir::case_selector_mode::exact;  // Comparison mode
    const ir::expr* range_bound = nullptr;     // Range bound expression (for >= 0x80, etc.)
    const ir::field* case_field;               // Field to read for this case

    // Constructor for exact match cases (backward compatible)
    StartChoiceCaseCommand(std::vector<const ir::expr*> vals, const ir::field* fld)
        : Command(StartChoiceCase), case_values(std::move(vals)),
          selector_mode(ir::case_selector_mode::exact), range_bound(nullptr), case_field(fld) {}

    // Constructor for range-based cases
    StartChoiceCaseCommand(ir::case_selector_mode mode, const ir::expr* bound, const ir::field* fld)
        : Command(StartChoiceCase), case_values{},
          selector_mode(mode), range_bound(bound), case_field(fld) {}
};

struct EndChoiceCaseCommand : Command {
    EndChoiceCaseCommand() : Command(EndChoiceCase) {}
};

// ============================================================================
// Method Commands
// ============================================================================

struct StartMethodCommand : Command {
    enum class MethodKind {
        StructReader,        // read() method for a struct
        StructWriter,        // write() method for a struct
        UnionReader,         // read() method for a union (trial-and-error)
        UnionFieldReader,    // read_as_<field>() method for a union field
        ChoiceReader,        // read() method for a choice (tagged union)
        StandaloneReader,    // Static read function
        UserFunction,        // User-defined function (const member function)
        Custom               // Custom method
    };

    std::string method_name;
    MethodKind kind;
    const ir::struct_def* target_struct;  // For struct-related methods (can be null for custom/union)
    const ir::choice_def* target_choice;  // For choice-related methods (can be null otherwise)
    const ir::type_ref* return_type;      // For union field readers and user functions
    const std::vector<ir::function_param>* parameters;  // For user functions (pointer to IR data, not copied)
    bool use_exceptions;  // Error handling strategy
    bool is_static;

    StartMethodCommand(const std::string& n, MethodKind k,
                      const ir::struct_def* target, bool exceptions, bool stat)
        : Command(StartMethod), method_name(n), kind(k),
          target_struct(target), target_choice(nullptr), return_type(nullptr), parameters(nullptr), use_exceptions(exceptions), is_static(stat) {}

    // Constructor for choice readers
    StartMethodCommand(const std::string& n, MethodKind k,
                      const ir::choice_def* choice, bool exceptions, bool stat)
        : Command(StartMethod), method_name(n), kind(k),
          target_struct(nullptr), target_choice(choice), return_type(nullptr), parameters(nullptr), use_exceptions(exceptions), is_static(stat) {}

    // Constructor for union field readers
    StartMethodCommand(const std::string& n, const ir::type_ref* ret_type, bool exceptions, bool stat)
        : Command(StartMethod), method_name(n), kind(MethodKind::UnionFieldReader),
          target_struct(nullptr), target_choice(nullptr), return_type(ret_type), parameters(nullptr), use_exceptions(exceptions), is_static(stat) {}

    // Constructor for user-defined functions
    StartMethodCommand(const std::string& n, const ir::type_ref* ret_type,
                      const std::vector<ir::function_param>* params, bool stat)
        : Command(StartMethod), method_name(n), kind(MethodKind::UserFunction),
          target_struct(nullptr), target_choice(nullptr), return_type(ret_type), parameters(params),
          use_exceptions(true), is_static(stat) {}
};

struct EndMethodCommand : Command {
    EndMethodCommand() : Command(EndMethod) {}
};

struct ReturnValueCommand : Command {
    std::string value;

    explicit ReturnValueCommand(const std::string& val)
        : Command(ReturnValue), value(val) {}
};

struct ReturnExpressionCommand : Command {
    const ir::expr* expression;

    explicit ReturnExpressionCommand(const ir::expr* expr)
        : Command(ReturnExpression), expression(expr) {}
};

// ============================================================================
// Reading Commands
// ============================================================================

struct ReadFieldCommand : Command {
    std::string field_name;
    const ir::type_ref* field_type;  // IR type, not language-specific string
    bool use_exceptions;

    ReadFieldCommand(const std::string& name, const ir::type_ref* ftype, bool exc)
        : Command(ReadField), field_name(name), field_type(ftype), use_exceptions(exc) {}
};

struct ReadArrayElementCommand : Command {
    std::string element_name;
    const ir::type_ref* element_type;  // IR type, not language-specific string
    bool use_exceptions;

    ReadArrayElementCommand(const std::string& elem, const ir::type_ref* etype, bool exc)
        : Command(ReadArrayElement), element_name(elem), element_type(etype), use_exceptions(exc) {}
};

struct SeekToLabelCommand : Command {
    const ir::expr* label_expr;  // Expression that evaluates to the seek position
    bool use_exceptions;

    SeekToLabelCommand(const ir::expr* expr, bool exc)
        : Command(SeekToLabel), label_expr(expr), use_exceptions(exc) {}
};

struct AlignPointerCommand : Command {
    uint64_t alignment;  // Alignment boundary (must be power of 2)
    bool use_exceptions;

    AlignPointerCommand(uint64_t align, bool exc)
        : Command(AlignPointer), alignment(align), use_exceptions(exc) {}
};

// ============================================================================
// Array Commands
// ============================================================================

struct ResizeArrayCommand : Command {
    std::string array_name;
    const ir::expr* size_expr;

    ResizeArrayCommand(const std::string& arr, const ir::expr* size)
        : Command(ResizeArray), array_name(arr), size_expr(size) {}
};

struct AppendToArrayCommand : Command {
    std::string array_name;
    const ir::type_ref* element_type;
    bool use_exceptions;

    AppendToArrayCommand(const std::string& arr, const ir::type_ref* etype, bool exc)
        : Command(AppendToArray), array_name(arr), element_type(etype), use_exceptions(exc) {}
};

// ============================================================================
// Control Flow Commands
// ============================================================================

struct StartLoopCommand : Command {
    std::string loop_var;
    const ir::expr* count_expr;
    bool use_size_method;  // If true, append .size() to count expression

    StartLoopCommand(const std::string& var, const ir::expr* count, bool use_size = false)
        : Command(StartLoop), loop_var(var), count_expr(count), use_size_method(use_size) {}
};

struct EndLoopCommand : Command {
    EndLoopCommand() : Command(EndLoop) {}
};

struct StartWhileLoopCommand : Command {
    std::string condition;  // Raw C++ condition string (e.g., "data < end")

    StartWhileLoopCommand(const std::string& cond)
        : Command(StartWhileLoop), condition(cond) {}
};

struct IfCommand : Command {
    const ir::expr* condition;

    explicit IfCommand(const ir::expr* cond)
        : Command(If), condition(cond) {}
};

struct ElseCommand : Command {
    ElseCommand() : Command(Else) {}
};

struct EndIfCommand : Command {
    EndIfCommand() : Command(EndIf) {}
};

struct StartScopeCommand : Command {
    StartScopeCommand() : Command(StartScope) {}
};

struct EndScopeCommand : Command {
    EndScopeCommand() : Command(EndScope) {}
};

// ============================================================================
// Trial-and-Error Decoding Commands (for unions)
// ============================================================================

struct SavePositionCommand : Command {
    std::string var_name;  // Variable name to save position (e.g., "union_pos_0")

    explicit SavePositionCommand(const std::string& name)
        : Command(SavePosition), var_name(name) {}
};

struct RestorePositionCommand : Command {
    std::string var_name;  // Variable name to restore from

    explicit RestorePositionCommand(const std::string& name)
        : Command(RestorePosition), var_name(name) {}
};

struct StartTryBranchCommand : Command {
    std::string branch_name;  // For documentation/debugging

    explicit StartTryBranchCommand(const std::string& name)
        : Command(StartTryBranch), branch_name(name) {}
};

struct EndTryBranchCommand : Command {
    bool is_last_branch;  // If true and not optional, rethrow/propagate error instead of catching
    bool is_optional_union;  // If true, don't rethrow on last branch (leave as monostate)

    explicit EndTryBranchCommand(bool is_last, bool is_optional = false)
        : Command(EndTryBranch), is_last_branch(is_last), is_optional_union(is_optional) {}
};

// ============================================================================
// Validation Commands
// ============================================================================

struct BoundsCheckCommand : Command {
    std::string value_name;  // For error messages
    const ir::expr* value_expr;
    const ir::expr* min_expr;
    const ir::expr* max_expr;
    std::string error_message;
    bool use_exceptions;  // throw vs return error

    BoundsCheckCommand(const std::string& name, const ir::expr* val,
                      const ir::expr* min, const ir::expr* max,
                      const std::string& msg, bool exc)
        : Command(BoundsCheck), value_name(name), value_expr(val),
          min_expr(min), max_expr(max), error_message(msg),
          use_exceptions(exc) {}
};

struct ConstraintCheckCommand : Command {
    const ir::expr* condition;
    std::string error_message;
    bool use_exceptions;
    std::string field_name;  // Field being validated (for union self-references)

    ConstraintCheckCommand(const ir::expr* cond, const std::string& msg, bool exc, const std::string& field = "")
        : Command(ConstraintCheck), condition(cond), error_message(msg), use_exceptions(exc), field_name(field) {}
};

// ============================================================================
// Variable Commands
// ============================================================================

struct DeclareVariableCommand : Command {
    enum class VariableKind {
        LocalVariable,        // Regular local variable
        StructInstance,       // Instance of a struct type (for standalone readers)
        Custom                // Custom variable
    };

    std::string var_name;
    VariableKind kind;
    const ir::struct_def* struct_type;  // For StructInstance kind
    const ir::type_ref* type;            // For LocalVariable kind
    const ir::expr* init_expr;           // nullptr if no initialization
    std::string custom_type_name;        // For Custom kind

    // Constructor for struct instance
    DeclareVariableCommand(const std::string& var, const ir::struct_def* sdef)
        : Command(DeclareVariable), var_name(var), kind(VariableKind::StructInstance),
          struct_type(sdef), type(nullptr), init_expr(nullptr), custom_type_name("") {}

    // Constructor for regular variable with IR type
    DeclareVariableCommand(const std::string& var, const ir::type_ref* t, const ir::expr* init = nullptr)
        : Command(DeclareVariable), var_name(var), kind(VariableKind::LocalVariable),
          struct_type(nullptr), type(t), init_expr(init), custom_type_name("") {}

    // Constructor for custom type name
    DeclareVariableCommand(const std::string& var, const std::string& type_name)
        : Command(DeclareVariable), var_name(var), kind(VariableKind::Custom),
          struct_type(nullptr), type(nullptr), init_expr(nullptr), custom_type_name(type_name) {}
};

struct AssignVariableCommand : Command {
    std::string target;  // e.g., "result.value" or "obj.field"
    const ir::expr* value_expr;

    AssignVariableCommand(const std::string& tgt, const ir::expr* val)
        : Command(AssignVariable), target(tgt), value_expr(val) {}
};

struct AssignChoiceWrapperCommand : Command {
    std::string target;       // e.g., "obj.data"
    std::string wrapper_type; // e.g., "Case0_text_length"
    std::string source_var;   // e.g., "text_length"

    AssignChoiceWrapperCommand(const std::string& tgt, const std::string& wrapper, const std::string& src)
        : Command(AssignChoiceWrapper), target(tgt), wrapper_type(wrapper), source_var(src) {}
};

// ============================================================================
// Bitfield Operations (Language-Agnostic)
// ============================================================================

/// Read a primitive type from the data buffer into a variable
/// Replaces: RawCodeCommand("var = read_uint8(data);")
struct ReadPrimitiveToVariableCommand : Command {
    std::string target_var;       // Variable name: "_bitfield_byte0"
    ir::type_kind primitive_type; // uint8, uint16, uint32, etc.
    bool use_exceptions;          // Exception vs safe mode

    ReadPrimitiveToVariableCommand(const std::string& var, ir::type_kind ptype, bool exceptions)
        : Command(ReadPrimitiveToVariable), target_var(var), primitive_type(ptype), use_exceptions(exceptions) {}
};

/// Extract a bitfield from byte variable(s) and assign to target field
/// Handles single-byte and multi-byte bitfields with proper bit shifting and masking
/// Replaces: RawCodeCommand("obj.field = (_bitfield_byte0 >> 4) & 0xF;")
struct ExtractBitfieldCommand : Command {
    std::string target_field;     // Target field: "obj.nibble1"
    std::string source_var;       // Source byte variable: "_bitfield_byte0"
    size_t bit_offset;            // Bit offset within source byte (0-7)
    size_t bit_width;             // Width of bitfield (1-64)
    uint64_t mask;                // Bitmask for extraction

    // For multi-byte bitfields (when bit_offset + bit_width > 8)
    std::optional<std::string> source_var2;       // Second byte variable: "_bitfield_byte1"
    std::optional<size_t> bits_in_first_byte;     // How many bits from first byte
    std::optional<uint64_t> first_mask;           // Mask for first byte
    std::optional<uint64_t> second_mask;          // Mask for second byte

    // Single-byte bitfield constructor
    ExtractBitfieldCommand(const std::string& target, const std::string& src,
                          size_t offset, size_t width, uint64_t m)
        : Command(ExtractBitfield), target_field(target), source_var(src),
          bit_offset(offset), bit_width(width), mask(m) {}

    // Multi-byte bitfield constructor
    ExtractBitfieldCommand(const std::string& target, const std::string& src1, const std::string& src2,
                          size_t offset, size_t width, uint64_t m,
                          size_t bits_first, uint64_t mask1, uint64_t mask2)
        : Command(ExtractBitfield), target_field(target), source_var(src1),
          bit_offset(offset), bit_width(width), mask(m),
          source_var2(src2), bits_in_first_byte(bits_first),
          first_mask(mask1), second_mask(mask2) {}
};

// ============================================================================
// Error Handling Commands
// ============================================================================

struct SetErrorMessageCommand : Command {
    const ir::expr* message_expr;  // Can be a complex expression

    explicit SetErrorMessageCommand(const ir::expr* msg)
        : Command(SetErrorMessage), message_expr(msg) {}
};

struct ThrowExceptionCommand : Command {
    std::string exception_type;
    const ir::expr* message_expr;

    ThrowExceptionCommand(const std::string& exc_type, const ir::expr* msg)
        : Command(ThrowException), exception_type(exc_type), message_expr(msg) {}
};

// ============================================================================
// Safe Mode Result Handling Commands
// ============================================================================

/// Declare a ReadResult<T> variable for safe mode error handling.
/// In C++: ReadResult<StructName> result;
struct DeclareResultVariableCommand : Command {
    std::string result_name;      // Variable name (typically "result")
    const ir::struct_def* struct_type;  // The struct type being read

    DeclareResultVariableCommand(const std::string& name, const ir::struct_def* stype)
        : Command(DeclareResultVariable), result_name(name), struct_type(stype) {}
};

/// Set result.success = true in safe mode (indicates successful read).
struct SetResultSuccessCommand : Command {
    std::string result_name;  // Variable name (typically "result")

    explicit SetResultSuccessCommand(const std::string& name)
        : Command(SetResultSuccess), result_name(name) {}
};

// ============================================================================
// Comment Command
// ============================================================================

struct CommentCommand : Command {
    std::string text;

    explicit CommentCommand(const std::string& txt)
        : Command(Comment), text(txt) {}
};

// Removed: RawCodeCommand - replaced with language-agnostic commands

// ============================================================================
// Module-Level Commands
// ============================================================================

struct ModuleStartCommand : Command {
    ModuleStartCommand() : Command(ModuleStart) {}
};

struct ModuleEndCommand : Command {
    ModuleEndCommand() : Command(ModuleEnd) {}
};

struct NamespaceStartCommand : Command {
    std::string namespace_name;

    explicit NamespaceStartCommand(const std::string& ns)
        : Command(NamespaceStart), namespace_name(ns) {}
};

struct NamespaceEndCommand : Command {
    std::string namespace_name;  // For closing comment

    explicit NamespaceEndCommand(const std::string& ns)
        : Command(NamespaceEnd), namespace_name(ns) {}
};

struct IncludeDirectiveCommand : Command {
    std::string header;
    bool system_header;  // true for <>, false for ""

    IncludeDirectiveCommand(const std::string& h, bool sys = true)
        : Command(IncludeDirective), header(h), system_header(sys) {}
};

// ============================================================================
// Constant Commands
// ============================================================================

struct DeclareConstantCommand : Command {
    std::string name;
    const ir::type_ref* type;
    const ir::expr* value;

    DeclareConstantCommand(const std::string& n, const ir::type_ref* t, const ir::expr* v)
        : Command(DeclareConstant), name(n), type(t), value(v) {}
};

// ============================================================================
// Enum Commands
// ============================================================================

struct StartEnumCommand : Command {
    std::string enum_name;
    const ir::type_ref* base_type;
    bool is_bitmask;

    StartEnumCommand(const std::string& name, const ir::type_ref* base, bool bitmask = false)
        : Command(StartEnum), enum_name(name), base_type(base), is_bitmask(bitmask) {}
};

struct DeclareEnumItemCommand : Command {
    std::string item_name;
    int64_t value;
    bool is_last;  // For comma formatting

    DeclareEnumItemCommand(const std::string& name, int64_t val, bool last = false)
        : Command(DeclareEnumItem), item_name(name), value(val), is_last(last) {}
};

struct EndEnumCommand : Command {
    EndEnumCommand() : Command(EndEnum) {}
};

struct SubtypeCommand : Command {
    const ir::subtype_def* subtype;
    cpp_options opts;

    SubtypeCommand(const ir::subtype_def* st, const cpp_options& o)
        : Command(Subtype), subtype(st), opts(o) {}
};

// ============================================================================
// Command Collection
// ============================================================================

using CommandPtr = std::unique_ptr<Command>;
using CommandList = std::vector<CommandPtr>;

// Helper to downcast commands (use with caution, verify type first)
template<typename T>
const T* as(const Command* cmd) {
    return static_cast<const T*>(cmd);
}

template<typename T>
const T& as(const Command& cmd) {
    return static_cast<const T&>(cmd);
}

} // namespace datascript::codegen

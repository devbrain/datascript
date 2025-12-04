//
// Intermediate Representation (IR) for DataScript
//
// Simplified, language-agnostic representation of analyzed schemas.
// Used for code generation, serialization, and cross-language support.
//

#pragma once

#include "ast.hh"
#include <string>
#include <vector>
#include <map>
#include <optional>
#include <memory>
#include <variant>

namespace datascript::ir {

// ============================================================================
// Source Location Tracking
// ============================================================================

/// Source location preserved from DataScript code
struct source_location {
    std::string file_path;
    size_t line;
    size_t column;

    /// Create from AST source position
    static source_location from_ast(const ast::source_pos& pos) {
        return {pos.file, pos.line, pos.column};
    }

    /// Format as "file:line:column"
    [[nodiscard]] std::string format() const {
        return file_path + ":" + std::to_string(line) + ":" + std::to_string(column);
    }
};

// ============================================================================
// Type System
// ============================================================================

enum class type_kind {
    // Primitive integer types
    uint8, uint16, uint32, uint64, uint128,
    int8, int16, int32, int64, int128,

    // Other primitives
    boolean,
    string,

    // Bitfields
    bitfield,

    // User-defined types (reference by index)
    struct_type,
    union_type,
    enum_type,
    choice_type,
    subtype_ref,

    // Array types
    array_fixed,
    array_variable,
    array_ranged
};

enum class endianness {
    little,
    big,
    native
};

// Forward declaration
struct expr;

struct type_ref {
    type_kind kind;
    source_location source;

    // For primitives: optional endianness
    std::optional<endianness> byte_order;

    // For bitfields: bit width
    std::optional<size_t> bit_width;

    // For user types: index into module's type arrays
    std::optional<size_t> type_index;

    // For arrays
    std::unique_ptr<type_ref> element_type;
    std::optional<uint64_t> array_size;      // For fixed arrays (compile-time constant)
    std::unique_ptr<expr> array_size_expr;   // For variable arrays (runtime expression)

    // For ranged arrays: T[min..max] or T[..max]
    std::unique_ptr<expr> min_size_expr;     // Minimum size (can be literal, constant, or field ref)
    std::unique_ptr<expr> max_size_expr;     // Maximum size (can be literal, constant, or field ref)

    // For parameterized choices: selector argument expressions
    std::vector<std::unique_ptr<expr>> choice_selector_args;  // Arguments for choice instantiation

    // Computed by semantic analysis
    size_t size_bytes;
    size_t alignment;
    bool is_variable_size;

    // Default constructor
    type_ref() : kind(type_kind::uint8), source({"", 0, 0}),
                 size_bytes(0), alignment(0), is_variable_size(false) {}
};

// ============================================================================
// Expression System
// ============================================================================

struct expr {
    enum kind {
        literal_int,
        literal_bool,
        literal_string,
        parameter_ref,
        field_ref,
        constant_ref,
        array_index,
        binary_op,
        unary_op,
        ternary_op,
        function_call
    };

    enum op_type {
        // Arithmetic
        add, sub, mul, div, mod,
        // Comparison
        eq, ne, lt, gt, le, ge,
        // Logical
        logical_and, logical_or,
        // Bitwise
        bit_and, bit_or, bit_xor, bit_shift_left, bit_shift_right,
        // Unary
        negate, logical_not, bit_not
    };

    kind type;
    source_location source;

    // For literals
    uint64_t int_value = 0;
    bool bool_value = false;
    std::string string_value;

    // For references
    std::string ref_name;
    std::optional<size_t> field_index;

    // For operators
    op_type op = add;
    std::unique_ptr<expr> left;
    std::unique_ptr<expr> right;
    std::unique_ptr<expr> condition;
    std::unique_ptr<expr> true_expr;
    std::unique_ptr<expr> false_expr;

    // For function calls
    std::vector<std::unique_ptr<expr>> arguments;

    // Default constructor
    expr() : type(literal_int), source({"", 0, 0}) {}

    // Helper: create literal int
    static expr make_literal_int(uint64_t value, const source_location& loc) {
        expr e;
        e.type = literal_int;
        e.int_value = value;
        e.source = loc;
        return e;
    }

    // Helper: create literal bool
    static expr make_literal_bool(bool value, const source_location& loc) {
        expr e;
        e.type = literal_bool;
        e.bool_value = value;
        e.source = loc;
        return e;
    }

    // Helper: create parameter reference
    static expr make_param_ref(const std::string& name, const source_location& loc) {
        expr e;
        e.type = parameter_ref;
        e.ref_name = name;
        e.source = loc;
        return e;
    }

    // Helper: create binary operation
    static expr make_binary_op(op_type operation,
                               std::unique_ptr<expr> l,
                               std::unique_ptr<expr> r,
                               const source_location& loc) {
        expr e;
        e.type = binary_op;
        e.op = operation;
        e.left = std::move(l);
        e.right = std::move(r);
        e.source = loc;
        return e;
    }

    // Helper: deep copy an expression (including all unique_ptr children)
    static expr copy(const expr& other) {
        expr e;
        e.type = other.type;
        e.source = other.source;
        e.int_value = other.int_value;
        e.bool_value = other.bool_value;
        e.string_value = other.string_value;
        e.ref_name = other.ref_name;
        e.field_index = other.field_index;
        e.op = other.op;

        // Deep copy unique_ptr members
        if (other.left) {
            e.left = std::make_unique<expr>(copy(*other.left));
        }
        if (other.right) {
            e.right = std::make_unique<expr>(copy(*other.right));
        }
        if (other.condition) {
            e.condition = std::make_unique<expr>(copy(*other.condition));
        }
        if (other.true_expr) {
            e.true_expr = std::make_unique<expr>(copy(*other.true_expr));
        }
        if (other.false_expr) {
            e.false_expr = std::make_unique<expr>(copy(*other.false_expr));
        }

        return e;
    }
};

// ============================================================================
// Constraints
// ============================================================================

struct constraint_def {
    std::string name;
    source_location source;

    struct parameter {
        std::string name;
        type_ref type;
        source_location source;
    };
    std::vector<parameter> params;

    expr condition;
    std::string error_message_template;
};

// ============================================================================
// Fields and Types
// ============================================================================

struct field {
    std::string name;
    type_ref type;
    source_location source;

    size_t offset;

    // Applied constraints
    struct constraint_application {
        size_t constraint_index;
        std::vector<expr> arguments;
        source_location source;
    };
    std::vector<constraint_application> constraints;

    // Inline constraint (anonymous constraint expression: "field: expr")
    std::optional<expr> inline_constraint;

    // Default value (field initialization: "field = expr")
    std::optional<expr> default_value;

    // Field condition
    enum condition_kind {
        always,
        never,
        runtime
    };
    condition_kind condition = always;
    std::optional<expr> runtime_condition;

    // Label: seek to this position before reading the field
    std::optional<expr> label;

    // Alignment: pad to N-byte boundary before reading the field
    std::optional<uint64_t> alignment;

    std::string documentation;
};

// Function parameter
struct function_param {
    std::string name;
    type_ref param_type;
    source_location source;
};

// Statement types for function bodies
struct return_statement {
    expr value;
    source_location source;
};

struct expression_statement {
    expr expression;
    source_location source;
};

using statement = std::variant<
    return_statement,
    expression_statement
>;

// Function definition
struct function_def {
    std::string name;
    source_location source;

    type_ref return_type;
    std::vector<function_param> parameters;
    std::vector<statement> body;

    std::string documentation;
};

struct struct_def {
    std::string name;
    source_location source;

    std::vector<field> fields;
    std::vector<function_def> functions;

    size_t total_size;
    size_t alignment;

    std::string documentation;
};

struct enum_def {
    std::string name;
    source_location source;

    type_ref base_type;

    struct item {
        std::string name;
        uint64_t value;
        source_location source;
        std::string documentation;
    };
    std::vector<item> items;

    bool is_bitmask = false;  // true for bitmask, false for enum

    std::string documentation;
};

struct subtype_def {
    std::string name;
    source_location source;

    type_ref base_type;  // The underlying type (e.g., uint16)
    expr constraint;     // Validation expression (uses 'this')

    std::string documentation;
};

struct union_case {
    std::string case_name;
    source_location source;
    std::vector<field> fields;  // Fields in this case (1 for simple, N for anonymous block)
    std::optional<expr> condition;  // Optional runtime condition
    bool is_anonymous_block;  // true if { fields... } name syntax
    std::string documentation;
};

struct union_def {
    std::string name;
    source_location source;

    std::vector<union_case> cases;  // Changed from fields

    size_t size;
    size_t alignment;

    std::string documentation;
};

struct choice_def {
    std::string name;
    source_location source;

    struct param {
        std::string name;
        type_ref type;
        source_location source;
    };
    std::vector<param> parameters;  // Parameters for the choice (e.g., selector value)

    expr selector;

    struct case_def {
        std::vector<expr> case_values;
        field case_field;
        source_location source;
    };
    std::vector<case_def> cases;

    size_t size;
    size_t alignment;

    std::string documentation;
};

// ============================================================================
// Module
// ============================================================================

struct bundle {
    std::string name;
    source_location source;

    struct metadata {
        std::string datascript_version;
        std::string generator_version;
        std::string generated_at;
        std::vector<std::string> source_files;
    };
    metadata meta;

    std::vector<struct_def> structs;
    std::vector<enum_def> enums;
    std::vector<subtype_def> subtypes;
    std::vector<union_def> unions;
    std::vector<choice_def> choices;
    std::vector<constraint_def> constraints;

    std::map<std::string, uint64_t> constants;

    /**
     * Topologically sorted emission order for structs, unions, and choices.
     * Each entry specifies which type to emit and the index into the respective vector.
     * Type codes: 0 = struct, 1 = union, 2 = choice
     */
    std::vector<std::pair<size_t, size_t>> type_emission_order;
};

} // namespace datascript::ir

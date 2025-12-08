//
// Created by igor on 22/11/2025.
//

#pragma once
#include <string>
#include <cstdint>
#include <utility>
#include <variant>
#include <vector>
#include <memory>
#include <optional>

namespace datascript::ast {
    struct source_pos {
        source_pos(std::string file_, std::size_t line_, std::size_t column_)
            : file(std::move(file_)),
              line(line_),
              column(column_) {
        }

        std::string file;
        std::size_t line;
        std::size_t column;
    };

    // -----------------------------
    // Expression node definitions
    // -----------------------------
    struct literal_int {
        source_pos pos;
        uint64_t value;
    };

    struct literal_string {
        source_pos pos;
        std::string value;
    };

    struct literal_bool {
        source_pos pos;
        bool value;
    };

    struct identifier {
        source_pos pos;
        std::string name;
    };

    // Forward declaration for recursive types
    struct expr;

    // Unary operator types
    enum class unary_op {
        neg,      // -
        pos,      // +
        bit_not,  // ~
        log_not   // !
    };

    struct unary_expr {
        source_pos pos;
        unary_op op;
        std::unique_ptr<expr> operand;
    };

    // Binary operator types
    enum class binary_op {
        // Arithmetic
        add, sub, mul, div, mod,
        // Comparison
        eq, ne, lt, gt, le, ge,
        // Bitwise
        bit_and, bit_or, bit_xor, lshift, rshift,
        // Logical
        log_and, log_or
    };

    struct binary_expr {
        source_pos pos;
        binary_op op;
        std::unique_ptr<expr> left;
        std::unique_ptr<expr> right;
    };

    // Ternary conditional operator
    struct ternary_expr {
        source_pos pos;
        std::unique_ptr<expr> condition;
        std::unique_ptr<expr> true_expr;
        std::unique_ptr<expr> false_expr;
    };

    // Field access expression (obj.field)
    struct field_access_expr {
        source_pos pos;
        std::unique_ptr<expr> object;
        std::string field_name;
    };

    // Array indexing expression (arr[index])
    struct array_index_expr {
        source_pos pos;
        std::unique_ptr<expr> array;
        std::unique_ptr<expr> index;
    };

    // Function call expression (func(args...))
    struct function_call_expr {
        source_pos pos;
        std::unique_ptr<expr> function;
        std::vector<expr> arguments;
    };

    using expr_node = std::variant <
        literal_int,
        literal_bool,
        literal_string,
        identifier,
        unary_expr,
        binary_expr,
        ternary_expr,
        field_access_expr,
        array_index_expr,
        function_call_expr
    >;

    struct expr {
        expr_node node;
    };

    enum class endian {
        unspec,
        little,
        big,
    };

    struct primitive_type {
        source_pos pos;
        bool is_signed; // true: int, false: uint
        std::uint32_t bits; // 8/16/32/64/128
        endian byte_order{endian::unspec}; // unspec/little/big
    };

    struct bit_field_type_fixed {
        source_pos pos;
        uint64_t width; // bit: 32 -> 32
    };

    struct bit_field_type_expr {
        source_pos pos;
        expr width_expr; // bit<expr>
    };

    struct string_type {
        source_pos pos;
    };

    struct u16_string_type {
        source_pos pos;
        endian byte_order{endian::unspec}; // unspec/little/big
    };

    struct u32_string_type {
        source_pos pos;
        endian byte_order{endian::unspec}; // unspec/little/big
    };

    struct bool_type {
        source_pos pos;
    };

    struct qualified_name {
        source_pos pos;
        std::vector<std::string> parts;
    };

    // Forward declaration for type struct (needed for recursive array types)
    struct type;

    // Array types - wrap an element type
    struct array_type_fixed {
        source_pos pos;
        std::unique_ptr<type> element_type;
        expr size;  // Fixed size: T[size]
    };

    struct array_type_range {
        source_pos pos;
        std::unique_ptr<type> element_type;
        std::optional<expr> min_size;  // Optional min: T[min..max] or T[..max]
        expr max_size;                  // Required max
    };

    struct array_type_unsized {
        source_pos pos;
        std::unique_ptr<type> element_type;  // Unsized: T[]
    };

    // Type instantiation (parameterized type with arguments)
    // Example: Record(16) or Matrix(4, 8)
    struct type_instantiation {
        source_pos pos;
        qualified_name base_type;           // The base type name (e.g., "Record")
        std::vector<expr> arguments;        // The argument expressions (e.g., [16])
    };

    using type_node = std::variant <
        primitive_type,
        bit_field_type_fixed,
        bit_field_type_expr,
        string_type,
        u16_string_type,
        u32_string_type,
        bool_type,
        qualified_name,
        array_type_fixed,
        array_type_range,
        array_type_unsized,
        type_instantiation
    >;

    struct type {
        type_node node;
    };

    struct constant_def {
        source_pos pos;
        std::string name;
        type ctype;
        expr value;
        std::optional<std::string> docstring;
    };

    // Subtype definition with constraint
    struct subtype_def {
        source_pos pos;
        std::string name;
        type base_type;
        expr constraint;  // Expression using 'this' to refer to the value
        std::optional<std::string> docstring;
    };

    // Parameter for constraints and functions
    struct param {
        source_pos pos;
        type param_type;
        std::string name;
    };

    // Constraint definition
    struct constraint_def {
        source_pos pos;
        std::string name;
        std::vector<param> params;  // Optional parameters
        expr condition;  // The constraint expression
        std::optional<std::string> docstring;
    };

    // Package declaration
    struct package_decl {
        source_pos pos;
        std::vector<std::string> name_parts;  // e.g., ["foo", "bar", "baz"] for "package foo.bar.baz;"
    };

    // Import declaration
    struct import_decl {
        source_pos pos;
        std::vector<std::string> name_parts;  // e.g., ["foo", "bar", "baz"] for "import foo.bar.baz;"
        bool is_wildcard;  // true if "import foo.bar.*;"
    };

    // Enum item (member)
    struct enum_item {
        source_pos pos;
        std::string name;
        std::optional<expr> value;  // Optional value expression
        std::optional<std::string> docstring;
    };

    // Enum or bitmask definition
    struct enum_def {
        source_pos pos;
        std::string name;
        type base_type;  // Underlying type (e.g., uint8, uint32)
        std::vector<enum_item> items;
        bool is_bitmask;  // true for bitmask, false for enum
        std::optional<std::string> docstring;
    };

    // Struct field definition
    struct field_def {
        source_pos pos;
        type field_type;
        std::string name;
        std::optional<expr> condition;       // Optional if clause: "if expression"
        std::optional<expr> constraint;      // Optional inline constraint: "field: expression"
        std::optional<expr> default_value;   // Optional default value: "field = expression"
        std::optional<std::string> docstring;
    };

    // Label directive (standalone): expression:
    struct label_directive {
        source_pos pos;
        expr label_expr;  // The expression that evaluates to the position
    };

    // Alignment directive (standalone): align(N):
    struct alignment_directive {
        source_pos pos;
        expr alignment_expr;  // The alignment value (e.g., 8 for 8-byte alignment)
    };

    // Statement types for function bodies
    struct return_statement {
        source_pos pos;
        expr value;  // Expression to return
    };

    struct expression_statement {
        source_pos pos;
        expr expression;  // Expression to evaluate (for side effects, though we don't have many)
    };

    using statement = std::variant<
        return_statement,
        expression_statement
    >;

    // Function definition (member function in a struct/union/choice)
    struct function_def {
        source_pos pos;
        std::string name;
        type return_type;
        std::vector<param> parameters;
        std::vector<statement> body;
        std::optional<std::string> docstring;
    };

    // Forward declarations for recursive inline types
    struct union_case;
    struct struct_body_item_storage;

    // Inline union field - will be desugared to named type during semantic analysis
    // Example: union { ... } field_name;
    struct inline_union_field {
        source_pos pos;
        std::vector<union_case> cases;
        std::string field_name;
        std::optional<expr> condition;       // Optional if clause
        std::optional<expr> constraint;      // Optional inline constraint
        std::optional<std::string> docstring;
    };

    // Inline struct field - will be desugared to named type during semantic analysis
    // Example: { ... } field_name;
    // Note: Uses unique_ptr to break circular dependency with struct_body_item
    struct inline_struct_field {
        source_pos pos;
        std::unique_ptr<struct_body_item_storage> body;
        std::string field_name;
        std::optional<expr> condition;       // Optional if clause
        std::optional<expr> constraint;      // Optional inline constraint
        std::optional<std::string> docstring;
    };

    // Struct body item - can be a field, label, alignment directive, function, or inline type
    using struct_body_item = std::variant<
        field_def,
        label_directive,
        alignment_directive,
        function_def,
        inline_union_field,
        inline_struct_field
    >;

    // Storage wrapper to allow incomplete type in inline_struct_field
    struct struct_body_item_storage {
        std::vector<struct_body_item> items;
    };

    // Struct definition
    struct struct_def {
        source_pos pos;
        std::string name;
        std::vector<param> parameters;          // Optional type parameters
        std::vector<struct_body_item> body;     // Body items: fields, labels, alignment directives
        std::optional<std::string> docstring;
    };

    // Union case (one alternative in a union)
    struct union_case {
        source_pos pos;
        std::string case_name;                    // Name of this case/variant
        std::vector<struct_body_item> items;      // Items in this case (1 for simple field, N for anonymous block)
        std::optional<expr> condition;            // Optional condition: "} name: condition;"
        bool is_anonymous_block;                  // true if defined as { items... } name;
        std::optional<std::string> docstring;
    };

    // Union definition
    struct union_def {
        source_pos pos;
        std::string name;
        std::vector<param> parameters;        // Optional type parameters
        std::vector<union_case> cases;        // Changed from fields to cases
        std::optional<std::string> docstring;
    };

    // Choice case (one branch in a choice)
    struct choice_case {
        source_pos pos;
        std::vector<expr> case_exprs;  // Empty for default case
        bool is_default;
        std::vector<struct_body_item> items;  // Items in this case (1 for simple field, N for anonymous block)
        std::string field_name;               // Name of the field
        bool is_anonymous_block;              // true if defined as { items... } name;
    };

    // Choice definition (tagged union with selector)
    struct choice_def {
        source_pos pos;
        std::string name;
        std::vector<param> parameters;        // Optional type parameters
        std::optional<expr> selector;         // Optional: external (on field) or inline discriminator
        std::optional<type> inline_discriminator_type;  // Required for inline choices (when selector is nullopt)
        std::vector<choice_case> cases;
        std::optional<std::string> docstring;
    };

    // Type alias definition (typedef)
    struct type_alias_def {
        source_pos pos;
        std::string name;
        type target_type;
        std::optional<std::string> docstring;
    };

    struct module {
        std::optional<package_decl> package;  // At most one package declaration
        std::vector<import_decl> imports;
        endian default_endianness{endian::big};  // Default is big endian per spec
        std::vector<constant_def> constants;
        std::vector<subtype_def> subtypes;
        std::vector<constraint_def> constraints;
        std::vector<type_alias_def> type_aliases;
        std::vector<enum_def> enums;
        std::vector<struct_def> structs;
        std::vector<union_def> unions;
        std::vector<choice_def> choices;
    };
}

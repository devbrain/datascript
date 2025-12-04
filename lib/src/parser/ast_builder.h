/*
 * Parser context - C interface for communication between C parser/lexer and C++ code
 */

#ifndef PARSER_CONTEXT_H
#define PARSER_CONTEXT_H

#include <stddef.h>
#include "parser/parser_context.h"
#include "parser/scanner_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Operator codes - match ast::unary_op enum values */
enum parser_unary_op {
    PARSER_UNARY_NEG = 0,      /* - */
    PARSER_UNARY_POS = 1,      /* + */
    PARSER_UNARY_BIT_NOT = 2,  /* ~ */
    PARSER_UNARY_LOG_NOT = 3   /* ! */
};

/* Binary operator codes - match ast::binary_op enum values */
enum parser_binary_op {
    /* Arithmetic */
    PARSER_BINARY_ADD = 0,
    PARSER_BINARY_SUB = 1,
    PARSER_BINARY_MUL = 2,
    PARSER_BINARY_DIV = 3,
    PARSER_BINARY_MOD = 4,
    /* Comparison */
    PARSER_BINARY_EQ = 5,
    PARSER_BINARY_NE = 6,
    PARSER_BINARY_LT = 7,
    PARSER_BINARY_GT = 8,
    PARSER_BINARY_LE = 9,
    PARSER_BINARY_GE = 10,
    /* Bitwise */
    PARSER_BINARY_BIT_AND = 11,
    PARSER_BINARY_BIT_OR = 12,
    PARSER_BINARY_BIT_XOR = 13,
    PARSER_BINARY_LSHIFT = 14,
    PARSER_BINARY_RSHIFT = 15,
    /* Logical */
    PARSER_BINARY_LOG_AND = 16,
    PARSER_BINARY_LOG_OR = 17
};

/* Forward declarations for AST node types (opaque pointers) */
typedef struct ast_type ast_type_t;
typedef struct ast_expr ast_expr_t;
typedef struct ast_const ast_const_t;

/* Type-safe opaque pointers for specific AST variants */
/* These provide compile-time type safety in grammar without runtime overhead */
typedef struct ast_primitive_type ast_primitive_type_t;
typedef struct ast_string_type ast_string_type_t;
typedef struct ast_bool_type ast_bool_type_t;
typedef struct ast_bitfield_type ast_bitfield_type_t;
typedef struct ast_qualified_name ast_qualified_name_t;
typedef struct ast_array_type_fixed ast_array_type_fixed_t;
typedef struct ast_array_type_range ast_array_type_range_t;
typedef struct ast_array_type_unsized ast_array_type_unsized_t;

typedef struct ast_expr_literal_int ast_expr_literal_int_t;
typedef struct ast_expr_literal_string ast_expr_literal_string_t;
typedef struct ast_expr_literal_bool ast_expr_literal_bool_t;
typedef struct ast_expr_identifier ast_expr_identifier_t;
typedef struct ast_expr_unary ast_expr_unary_t;
typedef struct ast_expr_binary ast_expr_binary_t;
typedef struct ast_expr_ternary ast_expr_ternary_t;
typedef struct ast_expr_field_access ast_expr_field_access_t;
typedef struct ast_expr_array_index ast_expr_array_index_t;
typedef struct ast_expr_function_call ast_expr_function_call_t;

/* Argument list for function calls */
typedef struct ast_arg_list ast_arg_list_t;

/* Constraint and parameter types */
typedef struct ast_param ast_param_t;
typedef struct ast_param_list ast_param_list_t;
typedef struct ast_constraint_def ast_constraint_def_t;

/* Enum types */
typedef struct ast_enum_item ast_enum_item_t;
typedef struct ast_enum_item_list ast_enum_item_list_t;
typedef struct ast_enum_def ast_enum_def_t;

/* Struct types */
typedef struct ast_field_def ast_field_def_t;
typedef struct ast_field_list ast_field_list_t;
typedef struct ast_label_directive ast_label_directive_t;
typedef struct ast_alignment_directive ast_alignment_directive_t;
typedef struct ast_struct_body_item ast_struct_body_item_t;
typedef struct ast_struct_body_list ast_struct_body_list_t;
typedef struct ast_struct_def ast_struct_def_t;
typedef struct ast_inline_union_field ast_inline_union_field_t;
typedef struct ast_inline_struct_field ast_inline_struct_field_t;

/* Union types */
typedef struct ast_union_case ast_union_case_t;
typedef struct ast_union_case_list ast_union_case_list_t;
typedef struct ast_union_def ast_union_def_t;

/* Choice types */
typedef struct ast_choice_case ast_choice_case_t;
typedef struct ast_choice_case_list ast_choice_case_list_t;
typedef struct ast_expr_list ast_expr_list_t;
typedef struct ast_choice_def ast_choice_def_t;

/* Function and statement types */
typedef struct ast_function_def ast_function_def_t;
typedef struct ast_statement ast_statement_t;
typedef struct ast_statement_list ast_statement_list_t;







/* C interface functions - implemented in parser_bridge.cpp */



/* Parser callbacks - called from parser.y actions */
void* parser_alloc_node(parser_context_t* ctx, int type);
void parser_free_node(parser_context_t* ctx, void* node);

/* AST building callbacks - type-safe with specific variant types */
void parser_build_constant(parser_context_t* ctx, ast_type_t* type, token_value_t* name_tok, ast_expr_t* expr, token_value_t* docstring);
void parser_build_subtype(parser_context_t* ctx, ast_type_t* base_type, token_value_t* name_tok, ast_expr_t* constraint_expr, token_value_t* docstring);

/* Type builders - return specific type-safe pointers */
ast_primitive_type_t* parser_build_integer_type(parser_context_t* ctx, int is_signed, int bits, int endian);
ast_primitive_type_t* parser_build_integer_type_with_endian(parser_context_t* ctx, ast_primitive_type_t* type, int endian);
ast_string_type_t* parser_build_string_type(parser_context_t* ctx);
ast_bool_type_t* parser_build_bool_type(parser_context_t* ctx);
ast_bitfield_type_t* parser_build_bit_field(parser_context_t* ctx, token_value_t* width_tok);
ast_bitfield_type_t* parser_build_bit_field_expr(parser_context_t* ctx, ast_expr_t* width_expr);

/* Qualified name builders */
ast_qualified_name_t* parser_build_qualified_name_single(parser_context_t* ctx, token_value_t* ident_tok);
ast_qualified_name_t* parser_build_qualified_name_append(parser_context_t* ctx, ast_qualified_name_t* qname, token_value_t* ident_tok);

/* Expression builders - return specific type-safe pointers */
ast_expr_literal_int_t* parser_build_integer_literal(parser_context_t* ctx, token_value_t* literal_tok);
ast_expr_literal_string_t* parser_build_string_literal(parser_context_t* ctx, token_value_t* literal_tok);
ast_expr_literal_bool_t* parser_build_bool_literal(parser_context_t* ctx, token_value_t* literal_tok);
ast_expr_identifier_t* parser_build_identifier(parser_context_t* ctx, token_value_t* ident_tok);

/* Operator expression builders */
ast_expr_unary_t* parser_build_unary_expr(parser_context_t* ctx, int op, ast_expr_t* operand);
ast_expr_binary_t* parser_build_binary_expr(parser_context_t* ctx, int op, ast_expr_t* left, ast_expr_t* right);
ast_expr_ternary_t* parser_build_ternary_expr(parser_context_t* ctx, ast_expr_t* condition, ast_expr_t* true_expr, ast_expr_t* false_expr);
ast_expr_field_access_t* parser_build_field_access_expr(parser_context_t* ctx, ast_expr_t* object, token_value_t* field_tok);
ast_expr_array_index_t* parser_build_array_index_expr(parser_context_t* ctx, ast_expr_t* array, ast_expr_t* index);

/* Function call expression builders */
ast_arg_list_t* parser_build_arg_list_single(parser_context_t* ctx, ast_expr_t* arg);
ast_arg_list_t* parser_build_arg_list_append(parser_context_t* ctx, ast_arg_list_t* list, ast_expr_t* arg);
ast_expr_function_call_t* parser_build_function_call_expr(parser_context_t* ctx, ast_expr_t* function, ast_arg_list_t* args);
ast_expr_function_call_t* parser_build_function_call_expr_noargs(parser_context_t* ctx, ast_expr_t* function);

/* Constraint and parameter builders */
ast_param_t* parser_build_param(parser_context_t* ctx, ast_type_t* type, token_value_t* name_tok);
ast_param_list_t* parser_build_param_list_single(parser_context_t* ctx, ast_param_t* param);
ast_param_list_t* parser_build_param_list_append(parser_context_t* ctx, ast_param_list_t* list, ast_param_t* param);
void parser_build_constraint(parser_context_t* ctx, token_value_t* name_tok, ast_param_t** params, int param_count, ast_expr_t* condition, token_value_t* docstring);
void parser_build_constraint_with_list(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* param_list, ast_expr_t* condition, token_value_t* docstring);

/* Package and import builders */
void parser_build_package(parser_context_t* ctx, ast_qualified_name_t* qname);
void parser_build_import(parser_context_t* ctx, ast_qualified_name_t* qname, int is_wildcard);

/* Global endianness directive */
void parser_set_default_endianness(parser_context_t* ctx, int endian);

/* Enum builders */
ast_enum_item_t* parser_build_enum_item(parser_context_t* ctx, token_value_t* name_tok, ast_expr_t* value_expr, token_value_t* docstring);
ast_enum_item_list_t* parser_build_enum_item_list_single(parser_context_t* ctx, ast_enum_item_t* item);
ast_enum_item_list_t* parser_build_enum_item_list_append(parser_context_t* ctx, ast_enum_item_list_t* list, ast_enum_item_t* item);
void parser_build_enum(parser_context_t* ctx, int is_bitmask, ast_type_t* base_type, token_value_t* name_tok, ast_enum_item_list_t* items, token_value_t* docstring);

/* Struct builders */
ast_field_def_t* parser_build_field(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok, token_value_t* docstring);
ast_field_def_t* parser_build_field_with_condition(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok, ast_expr_t* condition, token_value_t* docstring);
ast_field_def_t* parser_build_field_with_constraint(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok, ast_expr_t* constraint, token_value_t* docstring);
ast_field_def_t* parser_build_field_with_default(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok, ast_expr_t* default_value, token_value_t* docstring);
ast_field_def_t* parser_build_field_with_constraint_and_default(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok, ast_expr_t* constraint, ast_expr_t* default_value, token_value_t* docstring);
ast_field_list_t* parser_build_field_list_single(parser_context_t* ctx, ast_field_def_t* field);
ast_field_list_t* parser_build_field_list_append(parser_context_t* ctx, ast_field_list_t* list, ast_field_def_t* field);

/* Struct body item builders */
ast_label_directive_t* parser_build_label_directive(parser_context_t* ctx, ast_expr_t* label_expr);
ast_alignment_directive_t* parser_build_alignment_directive(parser_context_t* ctx, ast_expr_t* alignment_expr);
ast_struct_body_item_t* parser_label_to_body_item(parser_context_t* ctx, ast_label_directive_t* label);
ast_struct_body_item_t* parser_alignment_to_body_item(parser_context_t* ctx, ast_alignment_directive_t* alignment);
ast_struct_body_item_t* parser_field_to_body_item(parser_context_t* ctx, ast_field_def_t* field);
ast_struct_body_item_t* parser_field_to_body_item_with_docstring(parser_context_t* ctx, ast_field_def_t* field, token_value_t* docstring);
ast_struct_body_list_t* parser_build_struct_body_list_single(parser_context_t* ctx, ast_struct_body_item_t* item);
ast_struct_body_list_t* parser_build_struct_body_list_append(parser_context_t* ctx, ast_struct_body_list_t* list, ast_struct_body_item_t* item);

/* Inline type builders */
ast_inline_union_field_t* parser_build_inline_union_field(parser_context_t* ctx, ast_union_case_list_t* cases, token_value_t* name_tok, token_value_t* docstring);
ast_inline_struct_field_t* parser_build_inline_struct_field(parser_context_t* ctx, ast_struct_body_list_t* body, token_value_t* name_tok, token_value_t* docstring);
ast_struct_body_item_t* parser_inline_union_to_body_item(parser_context_t* ctx, ast_inline_union_field_t* inline_union);
ast_struct_body_item_t* parser_inline_struct_to_body_item(parser_context_t* ctx, ast_inline_struct_field_t* inline_struct);

/* Struct builders - note: parameter builders are declared later in this file */
void parser_build_struct(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params, ast_struct_body_list_t* body, token_value_t* docstring);

/* Union builders */
typedef struct ast_union_case ast_union_case_t;
typedef struct ast_union_case_list ast_union_case_list_t;

ast_union_case_t* parser_build_union_case_simple(parser_context_t* ctx, ast_field_def_t* field);
ast_union_case_t* parser_build_union_case_anonymous(parser_context_t* ctx, ast_struct_body_list_t* body, token_value_t* name_tok, ast_expr_t* condition, token_value_t* docstring);
ast_union_case_list_t* parser_build_union_case_list_single(parser_context_t* ctx, ast_union_case_t* union_case);
ast_union_case_list_t* parser_build_union_case_list_append(parser_context_t* ctx, ast_union_case_list_t* list, ast_union_case_t* union_case);
void parser_build_union(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params, ast_union_case_list_t* cases, token_value_t* docstring);

/* Choice builders */
ast_expr_list_t* parser_build_expr_list_single(parser_context_t* ctx, ast_expr_t* expr);
ast_expr_list_t* parser_build_expr_list_append(parser_context_t* ctx, ast_expr_list_t* list, ast_expr_t* expr);
ast_choice_case_t* parser_build_choice_case(parser_context_t* ctx, ast_expr_list_t* case_exprs, ast_field_def_t* field);
ast_choice_case_t* parser_build_choice_default(parser_context_t* ctx, ast_field_def_t* field);
ast_choice_case_list_t* parser_build_choice_case_list_single(parser_context_t* ctx, ast_choice_case_t* choice_case);
ast_choice_case_list_t* parser_build_choice_case_list_append(parser_context_t* ctx, ast_choice_case_list_t* list, ast_choice_case_t* choice_case);
void parser_build_choice(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params, ast_expr_t* selector, ast_choice_case_list_t* cases, token_value_t* docstring);

/* Array type builders */
ast_array_type_fixed_t* parser_build_array_type_fixed(parser_context_t* ctx, ast_type_t* element_type, ast_expr_t* size);
ast_array_type_range_t* parser_build_array_type_range(parser_context_t* ctx, ast_type_t* element_type, ast_expr_t* min_size, ast_expr_t* max_size);
ast_array_type_unsized_t* parser_build_array_type_unsized(parser_context_t* ctx, ast_type_t* element_type);

/* Type conversion helpers - upcast from specific to general */
ast_type_t* parser_primitive_to_type(ast_primitive_type_t* prim);
ast_type_t* parser_string_to_type(ast_string_type_t* str);
ast_type_t* parser_bool_to_type(ast_bool_type_t* b);
ast_type_t* parser_bitfield_to_type(ast_bitfield_type_t* bf);
ast_type_t* parser_qualified_name_to_type(ast_qualified_name_t* qname);
ast_type_t* parser_array_fixed_to_type(ast_array_type_fixed_t* arr);
ast_type_t* parser_array_range_to_type(ast_array_type_range_t* arr);
ast_type_t* parser_array_unsized_to_type(ast_array_type_unsized_t* arr);

/* Type instantiation builder */
typedef struct ast_type_instantiation ast_type_instantiation_t;

ast_type_instantiation_t* parser_build_type_instantiation(parser_context_t* ctx, ast_qualified_name_t* base_type, ast_arg_list_t* arguments);
ast_type_t* parser_type_instantiation_to_type(ast_type_instantiation_t* inst);

ast_expr_t* parser_literal_int_to_expr(ast_expr_literal_int_t* lit);
ast_expr_t* parser_literal_string_to_expr(ast_expr_literal_string_t* lit);
ast_expr_t* parser_literal_bool_to_expr(ast_expr_literal_bool_t* lit);
ast_expr_t* parser_identifier_to_expr(ast_expr_identifier_t* id);
ast_expr_t* parser_unary_to_expr(ast_expr_unary_t* unary);
ast_expr_t* parser_binary_to_expr(ast_expr_binary_t* binary);
ast_expr_t* parser_ternary_to_expr(ast_expr_ternary_t* ternary);
ast_expr_t* parser_field_access_to_expr(ast_expr_field_access_t* field_access);
ast_expr_t* parser_array_index_to_expr(ast_expr_array_index_t* array_index);
ast_expr_t* parser_function_call_to_expr(ast_expr_function_call_t* function_call);

/* Function builders */
ast_function_def_t* parser_build_function(
    parser_context_t* ctx,
    ast_type_t* return_type,
    token_value_t* name_tok,
    ast_param_list_t* params,
    ast_statement_list_t* body,
    token_value_t* docstring
);

/* Statement builders */
ast_statement_t* parser_build_return_statement(parser_context_t* ctx, ast_expr_t* value);
ast_statement_t* parser_build_expression_statement(parser_context_t* ctx, ast_expr_t* expression);

/* Statement list builders */
ast_statement_list_t* parser_build_statement_list_single(parser_context_t* ctx, ast_statement_t* stmt);
ast_statement_list_t* parser_build_statement_list_append(
    parser_context_t* ctx,
    ast_statement_list_t* list,
    ast_statement_t* stmt
);

/* Function to struct body item conversions */
ast_struct_body_item_t* parser_function_to_body_item(parser_context_t* ctx, ast_function_def_t* func);
ast_struct_body_item_t* parser_function_to_body_item_with_docstring(
    parser_context_t* ctx,
    ast_function_def_t* func,
    token_value_t* docstring
);

/* List destructors - used by Lemon %destructor directives to clean up on parse errors */
void parser_destroy_arg_list(ast_arg_list_t* list);
void parser_destroy_param_list(ast_param_list_t* list);
void parser_destroy_enum_item_list(ast_enum_item_list_t* list);
void parser_destroy_field_list(ast_field_list_t* list);
void parser_destroy_expr_list(ast_expr_list_t* list);
void parser_destroy_union_case_list(ast_union_case_list_t* list);
void parser_destroy_choice_case_list(ast_choice_case_list_t* list);
void parser_destroy_statement_list(ast_statement_list_t* list);
void parser_destroy_inline_union_field(ast_inline_union_field_t* field);
void parser_destroy_inline_struct_field(ast_inline_struct_field_t* field);

#ifdef __cplusplus
}
#endif

#endif /* PARSER_CONTEXT_H */
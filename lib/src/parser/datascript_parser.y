/* DataScript Parser Grammar for Lemon */

%include {
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

#include "parser/ast_builder.h"

/* Forward declarations */
void* ParseAlloc(void* (*allocProc)(size_t));
void Parse(void*, int, token_value_t*, parser_context_t*);
void ParseFree(void*, void(*freeProc)(void*));
void ParseTrace(FILE *TraceFILE, char *zTracePrompt);

} /* end %include */

/* Token type - passed as third parameter to Parse() */
%token_type {token_value_t*}

/* Token destructor - nothing to do, tokens are in pool */
%token_destructor {
    /* Tokens allocated from pool, not individually freed */
}

/* List destructors - clean up list holders on parse errors */
%destructor param_list {
    parser_destroy_param_list($$);
}

%destructor argument_list_content {
    parser_destroy_arg_list($$);
}

%destructor enum_item_list {
    parser_destroy_enum_item_list($$);
}

%destructor field_list {
    parser_destroy_field_list($$);
}

%destructor case_clause_list {
    parser_destroy_expr_list($$);
}

%destructor choice_case_list {
    parser_destroy_choice_case_list($$);
}

%destructor union_case_list {
    parser_destroy_union_case_list($$);
}

%destructor statement_list {
    parser_destroy_statement_list($$);
}

%destructor function_param_list {
    parser_destroy_param_list($$);
}

/* Note: inline_union_field and inline_struct_field are NOT grammar non-terminals.
 * They are created and immediately converted within struct_body_item rules,
 * so they never appear on the parser stack and don't need destructors. */

/* Extra argument - parser context */
%extra_argument {parser_context_t* ctx}

/* Define types for non-terminals that carry AST nodes */
%type type_specifier {ast_type_t*}
%type base_type_specifier {ast_type_t*}
%type primitive_type {ast_type_t*}
%type integer_type {ast_primitive_type_t*}
%type string_type {ast_string_type_t*}
%type u16_string_type {ast_u16_string_type_t*}
%type u32_string_type {ast_u32_string_type_t*}
%type bool_type {ast_bool_type_t*}
%type bit_field_type {ast_bitfield_type_t*}
%type qualified_name {ast_qualified_name_t*}
%type array_type {ast_type_t*}
%type expression {ast_expr_t*}
%type shift_expression {ast_expr_t*}
%type postfix_expression {ast_expr_t*}
%type primary_expression {ast_expr_t*}
%type label_expression {ast_expr_t*}
%type param {ast_param_t*}
%type param_list {ast_param_list_t*}
%type param_list_content {ast_param_list_t*}
%type argument_list_content {ast_arg_list_t*}
%type enum_item {ast_enum_item_t*}
%type enum_item_list {ast_enum_item_list_t*}
%type field_def {ast_field_def_t*}
%type field_def_nodoc {ast_field_def_t*}
%type field_list {ast_field_list_t*}
%type case_clause_list {ast_expr_list_t*}
%type choice_case {ast_choice_case_t*}
%type choice_case_list {ast_choice_case_list_t*}
%type union_case {ast_union_case_t*}
%type union_case_list {ast_union_case_list_t*}
%type type_instantiation {ast_type_instantiation_t*}
%type optional_docstring {token_value_t*}
%type struct_body_item {ast_struct_body_item_t*}
%type struct_body_list {ast_struct_body_list_t*}
%type function_def_nodoc {ast_function_def_t*}
%type function_param_list {ast_param_list_t*}
%type function_param {ast_param_t*}
%type statement {ast_statement_t*}
%type statement_list {ast_statement_list_t*}

/* Token definitions */
%token_prefix TOKEN_

/* Define tokens - EOF is handled specially by Lemon as token 0 */
%token IDENTIFIER INTEGER_LITERAL STRING_LITERAL BOOL_LITERAL DOC_COMMENT.
%token CONST PACKAGE IMPORT STRUCT UNION CHOICE ENUM BITMASK SUBTYPE CONSTRAINT.
%token FUNCTION RETURN IF OPTIONAL ON CASE DEFAULT ALIGN LITTLE BIG.
%token BOOL STRING U16STRING U32STRING BIT.
%token UINT8 UINT16 UINT32 UINT64 UINT128.
%token INT8 INT16 INT32 INT64 INT128.
%token SEMICOLON COMMA DOT DOTDOT COLON EQUALS.
%token LPAREN RPAREN LBRACE RBRACE LBRACKET RBRACKET.
%token LT GT PLUS MINUS STAR SLASH PERCENT QUESTION AT.
%token BIT_AND BIT_OR BIT_XOR BIT_NOT LOG_NOT.
%token EQ NE LE GE LSHIFT RSHIFT AND OR.

/* Operator precedence (lowest to highest) */
%left QUESTION COLON.       /* ?: ternary */
%left OR.                   /* || */
%left AND.                  /* && */
%left BIT_OR.              /* | */
%left BIT_XOR.             /* ^ */
%left BIT_AND.             /* & */
%left EQ NE.               /* == != */
%left LT GT LE GE.         /* < > <= >= */
%left LSHIFT RSHIFT.       /* << >> */
%left PLUS MINUS.          /* + - */
%left STAR SLASH PERCENT.  /* * / % */
%right UNARY_MINUS UNARY_PLUS BIT_NOT LOG_NOT. /* unary -, +, ~, ! */

/* Start symbol */
%start_symbol datascript_spec

/* Accept - mark successful parse */
%parse_accept {
    /* Parse completed successfully */
}

/* Grammar Rules */

datascript_spec ::= module_content.

module_content ::= .  /* Empty */
module_content ::= module_content module_item.

module_item ::= constant_def.
module_item ::= subtype_def.
module_item ::= constraint_def.
module_item ::= package_decl.
module_item ::= import_decl.
module_item ::= endianness_directive.
module_item ::= type_definition.

/* Optional Docstring - accepts multiple consecutive docstrings, uses only the last one */
optional_docstring(R) ::= . {
    R = NULL;  /* No docstring */
}

optional_docstring(R) ::= DOC_COMMENT(D). {
    R = D;  /* Single docstring */
}

optional_docstring(R) ::= optional_docstring DOC_COMMENT(D). {
    R = D;  /* Multiple docstrings - use the last one (attached to declaration) */
}

/* Package and Import */
package_decl ::= optional_docstring PACKAGE qualified_name(Q) SEMICOLON. {
    parser_build_package(ctx, Q);
}

import_decl ::= IMPORT qualified_name(Q) SEMICOLON. {
    parser_build_import(ctx, Q, 0);
}

import_decl ::= IMPORT qualified_name(Q) DOT STAR SEMICOLON. {
    parser_build_import(ctx, Q, 1);
}

/* Global Endianness Directive */
endianness_directive ::= LITTLE SEMICOLON. {
    parser_set_default_endianness(ctx, 1); /* 1 = little endian */
}

endianness_directive ::= BIG SEMICOLON. {
    parser_set_default_endianness(ctx, 2); /* 2 = big endian */
}

/* Constant Definition */
constant_def ::= optional_docstring(D) CONST type_specifier(T) IDENTIFIER(N) EQUALS expression(E) SEMICOLON. {
    parser_build_constant(ctx, T, N, E, D);
}

/* Subtype Definition */
subtype_def ::= optional_docstring(D) SUBTYPE type_specifier(T) IDENTIFIER(N) COLON expression(E) SEMICOLON. {
    parser_build_subtype(ctx, T, N, E, D);
}

/* Constraint Definition */
constraint_def ::= optional_docstring(D) CONSTRAINT IDENTIFIER(N) LBRACE expression(E) RBRACE SEMICOLON. {
    parser_build_constraint(ctx, N, NULL, 0, E, D);
}

constraint_def ::= optional_docstring(D) CONSTRAINT IDENTIFIER(N) param_list(P) LBRACE expression(E) RBRACE SEMICOLON. {
    parser_build_constraint_with_list(ctx, N, P, E, D);
}

/* Parameter List */
param_list(R) ::= LPAREN param_list_content(L) RPAREN. {
    R = L;
}

param_list_content(R) ::= param(P). {
    R = parser_build_param_list_single(ctx, P);
}

param_list_content(R) ::= param_list_content(L) COMMA param(P). {
    R = parser_build_param_list_append(ctx, L, P);
}

/* Parameter */
param(R) ::= type_specifier(T) IDENTIFIER(N). {
    R = parser_build_param(ctx, T, N);
}

/* Type Instantiation - parameterized type with arguments */
type_instantiation(R) ::= qualified_name(Q) LPAREN argument_list_content(A) RPAREN. {
    R = parser_build_type_instantiation(ctx, Q, A);
}

/* Type Specifiers - convert specific types to general ast_type_t* */
type_specifier(R) ::= base_type_specifier(T). { R = T; }
type_specifier(R) ::= array_type(A). { R = A; }

/* Base type specifiers (non-array types) */
base_type_specifier(R) ::= primitive_type(T). { R = T; }
base_type_specifier(R) ::= string_type(T). { R = parser_string_to_type(T); }
base_type_specifier(R) ::= u16_string_type(T). { R = parser_u16_string_to_type(T); }
base_type_specifier(R) ::= u32_string_type(T). { R = parser_u32_string_to_type(T); }
base_type_specifier(R) ::= bool_type(T). { R = parser_bool_to_type(T); }
base_type_specifier(R) ::= bit_field_type(T). { R = parser_bitfield_to_type(T); }
base_type_specifier(R) ::= qualified_name(Q). { R = parser_qualified_name_to_type(Q); }
base_type_specifier(R) ::= type_instantiation(I). { R = parser_type_instantiation_to_type(I); }

/* Primitive Types - convert ast_primitive_type_t* to ast_type_t* */
primitive_type(R) ::= integer_type(T). { R = parser_primitive_to_type(T); }
primitive_type(R) ::= LITTLE integer_type(T). {
    R = parser_primitive_to_type(parser_build_integer_type_with_endian(ctx, T, 1)); /* 1 = little */
}
primitive_type(R) ::= BIG integer_type(T). {
    R = parser_primitive_to_type(parser_build_integer_type_with_endian(ctx, T, 2)); /* 2 = big */
}

integer_type(R) ::= UINT8. {
    R = parser_build_integer_type(ctx, 0, 8, 0);
}
integer_type(R) ::= UINT16. {
    R = parser_build_integer_type(ctx, 0, 16, 0);
}
integer_type(R) ::= UINT32. {
    R = parser_build_integer_type(ctx, 0, 32, 0);
}
integer_type(R) ::= UINT64. {
    R = parser_build_integer_type(ctx, 0, 64, 0);
}
integer_type(R) ::= UINT128. {
    R = parser_build_integer_type(ctx, 0, 128, 0);
}
integer_type(R) ::= INT8. {
    R = parser_build_integer_type(ctx, 1, 8, 0);
}
integer_type(R) ::= INT16. {
    R = parser_build_integer_type(ctx, 1, 16, 0);
}
integer_type(R) ::= INT32. {
    R = parser_build_integer_type(ctx, 1, 32, 0);
}
integer_type(R) ::= INT64. {
    R = parser_build_integer_type(ctx, 1, 64, 0);
}
integer_type(R) ::= INT128. {
    R = parser_build_integer_type(ctx, 1, 128, 0);
}

/* String and Boolean Types */
string_type(R) ::= STRING. {
    R = parser_build_string_type(ctx);
}

/* UTF-16 String Type with optional endianness */
u16_string_type(R) ::= U16STRING. {
    R = parser_build_u16_string_type(ctx, 0);  /* 0 = unspec */
}
u16_string_type(R) ::= LITTLE U16STRING. {
    R = parser_build_u16_string_type(ctx, 1);  /* 1 = little */
}
u16_string_type(R) ::= BIG U16STRING. {
    R = parser_build_u16_string_type(ctx, 2);  /* 2 = big */
}

/* UTF-32 String Type with optional endianness */
u32_string_type(R) ::= U32STRING. {
    R = parser_build_u32_string_type(ctx, 0);  /* 0 = unspec */
}
u32_string_type(R) ::= LITTLE U32STRING. {
    R = parser_build_u32_string_type(ctx, 1);  /* 1 = little */
}
u32_string_type(R) ::= BIG U32STRING. {
    R = parser_build_u32_string_type(ctx, 2);  /* 2 = big */
}

bool_type(R) ::= BOOL. {
    R = parser_build_bool_type(ctx);
}

/* Bit Field Type */
bit_field_type(R) ::= BIT COLON INTEGER_LITERAL(W). {
    R = parser_build_bit_field(ctx, W);
}
bit_field_type(R) ::= BIT LT shift_expression(E) GT. {
    R = parser_build_bit_field_expr(ctx, E);
}

/* Array Types - use base_type_specifier to avoid left-recursion */
array_type(R) ::= base_type_specifier(T) LBRACKET RBRACKET. {
    R = parser_array_unsized_to_type(parser_build_array_type_unsized(ctx, T));
}

array_type(R) ::= base_type_specifier(T) LBRACKET expression(E) RBRACKET. {
    R = parser_array_fixed_to_type(parser_build_array_type_fixed(ctx, T, E));
}

array_type(R) ::= base_type_specifier(T) LBRACKET expression(MIN) DOTDOT expression(MAX) RBRACKET. {
    R = parser_array_range_to_type(parser_build_array_type_range(ctx, T, MIN, MAX));
}

array_type(R) ::= base_type_specifier(T) LBRACKET DOTDOT expression(MAX) RBRACKET. {
    R = parser_array_range_to_type(parser_build_array_type_range(ctx, T, NULL, MAX));
}

/* Shift expressions - everything except comparisons, logical ops, and ternary */
/* Used inside bit<> to avoid conflict with < > */
shift_expression(R) ::= postfix_expression(E). { R = E; }

/* Unary operators in shift_expression */
shift_expression(R) ::= MINUS shift_expression(E). [UNARY_MINUS] {
    R = parser_unary_to_expr(parser_build_unary_expr(ctx, PARSER_UNARY_NEG, E));
}
shift_expression(R) ::= PLUS shift_expression(E). [UNARY_PLUS] {
    R = parser_unary_to_expr(parser_build_unary_expr(ctx, PARSER_UNARY_POS, E));
}
shift_expression(R) ::= BIT_NOT shift_expression(E). [BIT_NOT] {
    R = parser_unary_to_expr(parser_build_unary_expr(ctx, PARSER_UNARY_BIT_NOT, E));
}
shift_expression(R) ::= LOG_NOT shift_expression(E). [LOG_NOT] {
    R = parser_unary_to_expr(parser_build_unary_expr(ctx, PARSER_UNARY_LOG_NOT, E));
}

/* Arithmetic operators */
shift_expression(R) ::= shift_expression(L) PLUS shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_ADD, L, E));
}
shift_expression(R) ::= shift_expression(L) MINUS shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_SUB, L, E));
}
shift_expression(R) ::= shift_expression(L) STAR shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_MUL, L, E));
}
shift_expression(R) ::= shift_expression(L) SLASH shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_DIV, L, E));
}
shift_expression(R) ::= shift_expression(L) PERCENT shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_MOD, L, E));
}

/* Shift operators */
shift_expression(R) ::= shift_expression(L) LSHIFT shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_LSHIFT, L, E));
}
shift_expression(R) ::= shift_expression(L) RSHIFT shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_RSHIFT, L, E));
}

/* Bitwise operators */
shift_expression(R) ::= shift_expression(L) BIT_AND shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_BIT_AND, L, E));
}
shift_expression(R) ::= shift_expression(L) BIT_OR shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_BIT_OR, L, E));
}
shift_expression(R) ::= shift_expression(L) BIT_XOR shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_BIT_XOR, L, E));
}

/* Parentheses */
shift_expression(R) ::= LPAREN expression(E) RPAREN. {
    R = E;
}

/* Full expressions - build on top of shift_expression, adding comparisons/logical/ternary */
expression(R) ::= shift_expression(E). { R = E; }

/* Binary operators - Comparison (use shift_expression as operands) */
expression(R) ::= shift_expression(L) EQ shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_EQ, L, E));
}
expression(R) ::= shift_expression(L) NE shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_NE, L, E));
}
expression(R) ::= shift_expression(L) LT shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_LT, L, E));
}
expression(R) ::= shift_expression(L) GT shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_GT, L, E));
}
expression(R) ::= shift_expression(L) LE shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_LE, L, E));
}
expression(R) ::= shift_expression(L) GE shift_expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_GE, L, E));
}

/* Binary operators - Logical (use expression as operands for left-to-right evaluation) */
expression(R) ::= expression(L) AND expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_LOG_AND, L, E));
}
expression(R) ::= expression(L) OR expression(E). {
    R = parser_binary_to_expr(parser_build_binary_expr(ctx, PARSER_BINARY_LOG_OR, L, E));
}

/* Ternary conditional operator */
expression(R) ::= expression(C) QUESTION expression(T) COLON expression(F). {
    R = parser_ternary_to_expr(parser_build_ternary_expr(ctx, C, T, F));
}

/* Postfix Expressions - field access, array indexing, and function calls */
postfix_expression(R) ::= primary_expression(E). {
    R = E;
}

postfix_expression(R) ::= postfix_expression(O) DOT IDENTIFIER(F). {
    R = parser_field_access_to_expr(parser_build_field_access_expr(ctx, O, F));
}

postfix_expression(R) ::= postfix_expression(A) LBRACKET expression(I) RBRACKET. {
    R = parser_array_index_to_expr(parser_build_array_index_expr(ctx, A, I));
}

postfix_expression(R) ::= postfix_expression(F) LPAREN RPAREN. {
    R = parser_function_call_to_expr(parser_build_function_call_expr_noargs(ctx, F));
}

postfix_expression(R) ::= postfix_expression(F) LPAREN argument_list_content(A) RPAREN. {
    R = parser_function_call_to_expr(parser_build_function_call_expr(ctx, F, A));
}

/* Argument list for function calls */
argument_list_content(R) ::= expression(E). {
    R = parser_build_arg_list_single(ctx, E);
}

argument_list_content(R) ::= argument_list_content(L) COMMA expression(E). {
    R = parser_build_arg_list_append(ctx, L, E);
}

/* Primary Expressions - convert specific expr types to general ast_expr_t* */
primary_expression(R) ::= INTEGER_LITERAL(L). {
    R = parser_literal_int_to_expr(parser_build_integer_literal(ctx, L));
}

primary_expression(R) ::= STRING_LITERAL(L). {
    R = parser_literal_string_to_expr(parser_build_string_literal(ctx, L));
}

primary_expression(R) ::= BOOL_LITERAL(L). {
    R = parser_literal_bool_to_expr(parser_build_bool_literal(ctx, L));
}

primary_expression(R) ::= IDENTIFIER(I). {
    R = parser_identifier_to_expr(parser_build_identifier(ctx, I));
}

/* Qualified Name - for user-defined types (e.g., Foo, Foo.Bar.Baz) */
qualified_name(R) ::= IDENTIFIER(I). {
    R = parser_build_qualified_name_single(ctx, I);
}
qualified_name(R) ::= qualified_name(Q) DOT IDENTIFIER(I). {
    R = parser_build_qualified_name_append(ctx, Q, I);
}


type_definition ::= struct_def.
type_definition ::= union_def.
type_definition ::= choice_def.
type_definition ::= enum_def.

/* Struct definitions */
struct_def ::= optional_docstring(D) STRUCT IDENTIFIER(N) LBRACE RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, NULL, NULL, D);
}

struct_def ::= optional_docstring(D) STRUCT IDENTIFIER(N) LBRACE struct_body_list(B) RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, NULL, B, D);
}

struct_def ::= optional_docstring(D) STRUCT IDENTIFIER(N) param_list(P) LBRACE RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, P, NULL, D);
}

struct_def ::= optional_docstring(D) STRUCT IDENTIFIER(N) param_list(P) LBRACE struct_body_list(B) RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, P, B, D);
}

/* Struct definitions (shorthand syntax without 'struct' keyword) */
struct_def ::= optional_docstring(D) IDENTIFIER(N) LBRACE RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, NULL, NULL, D);
}

struct_def ::= optional_docstring(D) IDENTIFIER(N) LBRACE struct_body_list(B) RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, NULL, B, D);
}

struct_def ::= optional_docstring(D) IDENTIFIER(N) param_list(P) LBRACE RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, P, NULL, D);
}

struct_def ::= optional_docstring(D) IDENTIFIER(N) param_list(P) LBRACE struct_body_list(B) RBRACE SEMICOLON. {
    parser_build_struct(ctx, N, P, B, D);
}

/* Struct body list - one or more body items (fields, labels, alignments) */
struct_body_list(R) ::= struct_body_item(I). {
    R = parser_build_struct_body_list_single(ctx, I);
}

struct_body_list(R) ::= struct_body_list(L) struct_body_item(I). {
    R = parser_build_struct_body_list_append(ctx, L, I);
}

/* Struct body item - can be field, label, or alignment */
/* Note: Labels and alignment don't have docstrings, only fields do */
struct_body_item(R) ::= ALIGN LPAREN expression(E) RPAREN COLON. {
    ast_alignment_directive_t* align = parser_build_alignment_directive(ctx, E);
    R = parser_alignment_to_body_item(ctx, align);
}

/* Label expression - identifier or field access, but not array indexing or function calls */
/* This avoids ambiguity with array type syntax like Type[10] field; */
label_expression(R) ::= primary_expression(E). {
    R = E;
}

label_expression(R) ::= label_expression(O) DOT IDENTIFIER(F). {
    R = parser_field_access_to_expr(parser_build_field_access_expr(ctx, O, F));
}

/* Label directive - supports field access like header.offset: */
struct_body_item(R) ::= label_expression(E) COLON. {
    ast_label_directive_t* label = parser_build_label_directive(ctx, E);
    R = parser_label_to_body_item(ctx, label);
}

/* Field without docstring */
struct_body_item(R) ::= field_def_nodoc(F). {
    R = parser_field_to_body_item(ctx, F);
}

/* Field with docstring - docstring is at struct_body_item level to avoid ambiguity */
struct_body_item(R) ::= DOC_COMMENT(D) field_def_nodoc(F). {
    /* Apply docstring to field */
    R = parser_field_to_body_item_with_docstring(ctx, F, D);
}

/* Function without docstring */
struct_body_item(R) ::= function_def_nodoc(F). {
    R = parser_function_to_body_item(ctx, F);
}

/* Function with docstring */
struct_body_item(R) ::= DOC_COMMENT(D) function_def_nodoc(F). {
    R = parser_function_to_body_item_with_docstring(ctx, F, D);
}

/* Inline union field without docstring */
struct_body_item(R) ::= UNION LBRACE union_case_list(C) RBRACE IDENTIFIER(N) SEMICOLON. {
    ast_inline_union_field_t* inline_union = parser_build_inline_union_field(ctx, C, N, NULL);
    R = parser_inline_union_to_body_item(ctx, inline_union);
}

/* Inline union field with docstring */
struct_body_item(R) ::= DOC_COMMENT(D) UNION LBRACE union_case_list(C) RBRACE IDENTIFIER(N) SEMICOLON. {
    ast_inline_union_field_t* inline_union = parser_build_inline_union_field(ctx, C, N, D);
    R = parser_inline_union_to_body_item(ctx, inline_union);
}

/* Inline struct field without docstring */
struct_body_item(R) ::= LBRACE struct_body_list(B) RBRACE IDENTIFIER(N) SEMICOLON. {
    ast_inline_struct_field_t* inline_struct = parser_build_inline_struct_field(ctx, B, N, NULL);
    R = parser_inline_struct_to_body_item(ctx, inline_struct);
}

/* Inline struct field with docstring */
struct_body_item(R) ::= DOC_COMMENT(D) LBRACE struct_body_list(B) RBRACE IDENTIFIER(N) SEMICOLON. {
    ast_inline_struct_field_t* inline_struct = parser_build_inline_struct_field(ctx, B, N, D);
    R = parser_inline_struct_to_body_item(ctx, inline_struct);
}

/* Field definition - type identifier [array-spec] [: constraint] [= default] [if condition] ; */

/* Basic field */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) SEMICOLON. {
    R = parser_build_field(ctx, T, N, D);
}

/* Field with inline constraint: field: expr; */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) COLON expression(C) SEMICOLON. {
    R = parser_build_field_with_constraint(ctx, T, N, C, D);
}

/* Field with default value: field = expr; */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) EQUALS expression(V) SEMICOLON. {
    R = parser_build_field_with_default(ctx, T, N, V, D);
}

/* Field with both constraint and default: field: expr = expr; */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) COLON expression(C) EQUALS expression(V) SEMICOLON. {
    R = parser_build_field_with_constraint_and_default(ctx, T, N, C, V, D);
}

/* Field with if condition: field if expr; */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) IF expression(E) SEMICOLON. {
    R = parser_build_field_with_condition(ctx, T, N, E, D);
}

/* Optional field syntax sugar: field optional expr; (desugars to if) */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) OPTIONAL expression(E) SEMICOLON. {
    R = parser_build_field_with_condition(ctx, T, N, E, D);
}

/* Unsized array fields */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) LBRACKET RBRACKET SEMICOLON. {
    ast_type_t* array_type = parser_array_unsized_to_type(parser_build_array_type_unsized(ctx, T));
    R = parser_build_field(ctx, array_type, N, D);
}

/* Fixed-size array fields */
field_def(R) ::= optional_docstring(D) type_specifier(T) IDENTIFIER(N) LBRACKET expression(E) RBRACKET SEMICOLON. {
    ast_type_t* array_type = parser_array_fixed_to_type(parser_build_array_type_fixed(ctx, T, E));
    R = parser_build_field(ctx, array_type, N, D);
}

/* Field definitions without docstring (for use in struct_body_item to avoid ambiguity) */
/* These pass NULL for docstring parameter */

/* Basic field - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) SEMICOLON. {
    R = parser_build_field(ctx, T, N, NULL);
}

/* Field with inline constraint - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) COLON expression(C) SEMICOLON. {
    R = parser_build_field_with_constraint(ctx, T, N, C, NULL);
}

/* Field with default value - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) EQUALS expression(V) SEMICOLON. {
    R = parser_build_field_with_default(ctx, T, N, V, NULL);
}

/* Field with both constraint and default - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) COLON expression(C) EQUALS expression(V) SEMICOLON. {
    R = parser_build_field_with_constraint_and_default(ctx, T, N, C, V, NULL);
}

/* Field with if condition - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) IF expression(E) SEMICOLON. {
    R = parser_build_field_with_condition(ctx, T, N, E, NULL);
}

/* Optional field syntax sugar - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) OPTIONAL expression(E) SEMICOLON. {
    R = parser_build_field_with_condition(ctx, T, N, E, NULL);
}

/* Unsized array field - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) LBRACKET RBRACKET SEMICOLON. {
    ast_type_t* array_type = parser_array_unsized_to_type(parser_build_array_type_unsized(ctx, T));
    R = parser_build_field(ctx, array_type, N, NULL);
}

/* Fixed-size array field - no docstring */
field_def_nodoc(R) ::= type_specifier(T) IDENTIFIER(N) LBRACKET expression(E) RBRACKET SEMICOLON. {
    ast_type_t* array_type = parser_array_fixed_to_type(parser_build_array_type_fixed(ctx, T, E));
    R = parser_build_field(ctx, array_type, N, NULL);
}

/* Function definitions */

/* Function without docstring - no parameters */
function_def_nodoc(R) ::= FUNCTION type_specifier(T) IDENTIFIER(N) LPAREN RPAREN LBRACE statement_list(S) RBRACE. {
    R = parser_build_function(ctx, T, N, NULL, S, NULL);
}

/* Function without docstring - with parameters */
function_def_nodoc(R) ::= FUNCTION type_specifier(T) IDENTIFIER(N) LPAREN function_param_list(P) RPAREN LBRACE statement_list(S) RBRACE. {
    R = parser_build_function(ctx, T, N, P, S, NULL);
}

/* Function parameter list */
function_param_list(R) ::= function_param(P). {
    R = parser_build_param_list_single(ctx, P);
}

function_param_list(R) ::= function_param_list(L) COMMA function_param(P). {
    R = parser_build_param_list_append(ctx, L, P);
}

/* Function parameter - type and name */
function_param(R) ::= type_specifier(T) IDENTIFIER(N). {
    R = parser_build_param(ctx, T, N);
}

/* Statement list - one or more statements */
statement_list(R) ::= statement(S). {
    R = parser_build_statement_list_single(ctx, S);
}

statement_list(R) ::= statement_list(L) statement(S). {
    R = parser_build_statement_list_append(ctx, L, S);
}

/* Statement - return or expression statement */
statement(R) ::= RETURN expression(E) SEMICOLON. {
    R = parser_build_return_statement(ctx, E);
}

statement(R) ::= expression(E) SEMICOLON. {
    R = parser_build_expression_statement(ctx, E);
}

/* Union definitions */
union_def ::= optional_docstring(D) UNION IDENTIFIER(N) LBRACE RBRACE SEMICOLON. {
    parser_build_union(ctx, N, NULL, NULL, D);
}

union_def ::= optional_docstring(D) UNION IDENTIFIER(N) LBRACE union_case_list(C) RBRACE SEMICOLON. {
    parser_build_union(ctx, N, NULL, C, D);
}

union_def ::= optional_docstring(D) UNION IDENTIFIER(N) param_list(P) LBRACE RBRACE SEMICOLON. {
    parser_build_union(ctx, N, P, NULL, D);
}

union_def ::= optional_docstring(D) UNION IDENTIFIER(N) param_list(P) LBRACE union_case_list(C) RBRACE SEMICOLON. {
    parser_build_union(ctx, N, P, C, D);
}

/* Union case list - one or more cases */
union_case_list(R) ::= union_case(C). {
    R = parser_build_union_case_list_single(ctx, C);
}

union_case_list(R) ::= union_case_list(L) union_case(C). {
    R = parser_build_union_case_list_append(ctx, L, C);
}

/* Union case - either simple field or anonymous block */
union_case(R) ::= field_def(F). {
    R = parser_build_union_case_simple(ctx, F);
}

union_case(R) ::= LBRACE struct_body_list(B) RBRACE IDENTIFIER(N) COLON expression(E) SEMICOLON. {
    R = parser_build_union_case_anonymous(ctx, B, N, E, NULL);
}

union_case(R) ::= LBRACE struct_body_list(B) RBRACE IDENTIFIER(N) SEMICOLON. {
    R = parser_build_union_case_anonymous(ctx, B, N, NULL, NULL);
}

union_case(R) ::= LBRACE RBRACE IDENTIFIER(N) SEMICOLON. {
    R = parser_build_union_case_anonymous(ctx, NULL, N, NULL, NULL);
}

/* Choice definitions (tagged unions with selector) */
choice_def ::= optional_docstring(D) CHOICE IDENTIFIER(N) ON expression(S) LBRACE choice_case_list(C) RBRACE SEMICOLON. {
    parser_build_choice(ctx, N, NULL, S, C, D);
}

choice_def ::= optional_docstring(D) CHOICE IDENTIFIER(N) param_list(P) ON expression(S) LBRACE choice_case_list(C) RBRACE SEMICOLON. {
    parser_build_choice(ctx, N, P, S, C, D);
}

/* Choice with inline discriminator (requires explicit type) */
choice_def ::= optional_docstring(D) CHOICE IDENTIFIER(N) COLON type_specifier(T) LBRACE choice_case_list(C) RBRACE SEMICOLON. {
    parser_build_choice_inline(ctx, N, NULL, T, C, D);
}

choice_def ::= optional_docstring(D) CHOICE IDENTIFIER(N) param_list(P) COLON type_specifier(T) LBRACE choice_case_list(C) RBRACE SEMICOLON. {
    parser_build_choice_inline(ctx, N, P, T, C, D);
}

/* Choice case list - one or more cases */
choice_case_list(R) ::= choice_case(C). {
    R = parser_build_choice_case_list_single(ctx, C);
}

choice_case_list(R) ::= choice_case_list(L) choice_case(C). {
    R = parser_build_choice_case_list_append(ctx, L, C);
}

/* Choice case - either case clauses or default */
choice_case(R) ::= case_clause_list(E) field_def(F). {
    R = parser_build_choice_case(ctx, E, F);
}

choice_case(R) ::= DEFAULT COLON field_def(F). {
    R = parser_build_choice_default(ctx, F);
}

/* Case clause list - one or more "case expr:" clauses */
case_clause_list(R) ::= CASE expression(E) COLON. {
    R = parser_build_expr_list_single(ctx, E);
}

case_clause_list(R) ::= case_clause_list(L) CASE expression(E) COLON. {
    R = parser_build_expr_list_append(ctx, L, E);
}

/* Enum and Bitmask definitions */
enum_def ::= optional_docstring(D) ENUM type_specifier(T) IDENTIFIER(N) LBRACE enum_item_list(L) RBRACE SEMICOLON. {
    parser_build_enum(ctx, 0, T, N, L, D);
}

enum_def ::= optional_docstring(D) BITMASK type_specifier(T) IDENTIFIER(N) LBRACE enum_item_list(L) RBRACE SEMICOLON. {
    parser_build_enum(ctx, 1, T, N, L, D);
}

/* Enum item list */
enum_item_list(R) ::= enum_item(I). {
    R = parser_build_enum_item_list_single(ctx, I);
}

enum_item_list(R) ::= enum_item_list(L) COMMA enum_item(I). {
    R = parser_build_enum_item_list_append(ctx, L, I);
}

/* Enum item - with optional value */
enum_item(R) ::= optional_docstring(D) IDENTIFIER(N). {
    R = parser_build_enum_item(ctx, N, NULL, D);
}

enum_item(R) ::= optional_docstring(D) IDENTIFIER(N) EQUALS expression(E). {
    R = parser_build_enum_item(ctx, N, E, D);
}

/* Error handling */
%syntax_error {
    const char* token_name = parser_get_token_name(yymajor);
    parser_set_error(ctx, PARSER_ERROR_SYNTAX,
                    "Syntax error at line %d: unexpected %s",
                    parser_get_line(ctx), token_name);
}

%parse_failure {
    parser_set_error(ctx, PARSER_ERROR_SYNTAX, "Parse failure");
}

/* Stack overflow */
%stack_overflow {
    parser_set_error(ctx, PARSER_ERROR_MEMORY, "Parser stack overflow");
}
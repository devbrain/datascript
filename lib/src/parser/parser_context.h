//
// Created by igor on 26/11/2025.
//

#pragma once
#include "parser/scanner_context.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Parser context */
typedef struct parser_context parser_context_t;

/* Error codes */
enum parser_error_code {
    PARSER_OK = 0,
    PARSER_ERROR_SYNTAX = 1,
    PARSER_ERROR_MEMORY = 2,
    PARSER_ERROR_UNEXPECTED_TOKEN = 3,
    PARSER_ERROR_INVALID_LITERAL = 4,
    PARSER_ERROR_IO = 5,
    PARSER_ERROR_SEMANTIC = 6,
    PARSER_ERROR_INTERNAL = 99
};

/* Error reporting */
#if defined(__GNUC__) || defined(__clang__)
__attribute__((format(printf, 3, 4)))
#endif
void parser_set_error(parser_context_t* ctx, enum parser_error_code code, const char* format, ...);

/* Get token pool usage statistics */
int parser_get_token_pool_max_used(const parser_context_t* ctx);
int parser_get_line(const parser_context_t* ctx);

/* Get human-readable token name */
const char* parser_get_token_name(int token_code);
#ifdef __cplusplus
}
#endif



#if defined(DATASCRIPT_PARSER_CONTEXT_VISIBLE)
#include "parser/ast_holder.hh"
#include "parser/scanner_context.h"

/* Error context */
typedef struct parser_error {
    enum parser_error_code code;
    char message[256];
    int line;
    int column;
    const char* filename;
} parser_error_t;

typedef struct parser_context {
    /* Error handling */
    parser_error_t error;

    /* AST builder (C++ object) */
    ast_module_holder* ast_builder;
    scanner_context_t* m_scanner;
    int token_pool_max_used;

} parser_context_t;
#endif



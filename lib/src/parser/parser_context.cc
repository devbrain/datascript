//
// Created by igor on 27/11/2025.
//
#include <cstdarg>
#include <cstdio>

#define DATASCRIPT_PARSER_CONTEXT_VISIBLE
#include "parser/parser_context.h"
#include "gen/datascript_parser.h"

void parser_set_error(parser_context_t* ctx, enum parser_error_code code, const char* format, ...) {
    if (!ctx) return;

    ctx->error.code = code;
    ctx->error.line = ctx->m_scanner->line;
    ctx->error.column = ctx->m_scanner->column;
    ctx->error.filename = ctx->m_scanner->filename;

    va_list args;
    va_start(args, format);
    vsnprintf(ctx->error.message, sizeof(ctx->error.message), format, args);
    va_end(args);
}

/* Get token pool usage statistics */
int parser_get_token_pool_max_used(const parser_context_t* ctx) {
    if (!ctx) return 0;
    return ctx->token_pool_max_used;
}

int parser_get_line(const parser_context_t* ctx) {
    return ctx->m_scanner->line;
}

const char* parser_get_token_name(int token_code) {
    switch (token_code) {
        case 0: return "end of file";
        case TOKEN_IDENTIFIER: return "identifier";
        case TOKEN_INTEGER_LITERAL: return "integer literal";
        case TOKEN_STRING_LITERAL: return "string literal";
        case TOKEN_BOOL_LITERAL: return "boolean literal";
        case TOKEN_DOC_COMMENT: return "doc comment";
        case TOKEN_CONST: return "'const'";
        case TOKEN_PACKAGE: return "'package'";
        case TOKEN_IMPORT: return "'import'";
        case TOKEN_STRUCT: return "'struct'";
        case TOKEN_UNION: return "'union'";
        case TOKEN_CHOICE: return "'choice'";
        case TOKEN_ENUM: return "'enum'";
        case TOKEN_BITMASK: return "'bitmask'";
        case TOKEN_SUBTYPE: return "'subtype'";
        case TOKEN_CONSTRAINT: return "'constraint'";
        case TOKEN_FUNCTION: return "'function'";
        case TOKEN_RETURN: return "'return'";
        case TOKEN_IF: return "'if'";
        case TOKEN_ON: return "'on'";
        case TOKEN_CASE: return "'case'";
        case TOKEN_DEFAULT: return "'default'";
        case TOKEN_ALIGN: return "'align'";
        case TOKEN_LITTLE: return "'little'";
        case TOKEN_BIG: return "'big'";
        case TOKEN_BOOL: return "'bool'";
        case TOKEN_STRING: return "'string'";
        case TOKEN_BIT: return "'bit'";
        case TOKEN_UINT8: return "'uint8'";
        case TOKEN_UINT16: return "'uint16'";
        case TOKEN_UINT32: return "'uint32'";
        case TOKEN_UINT64: return "'uint64'";
        case TOKEN_UINT128: return "'uint128'";
        case TOKEN_INT8: return "'int8'";
        case TOKEN_INT16: return "'int16'";
        case TOKEN_INT32: return "'int32'";
        case TOKEN_INT64: return "'int64'";
        case TOKEN_INT128: return "'int128'";
        case TOKEN_SEMICOLON: return "';'";
        case TOKEN_COMMA: return "','";
        case TOKEN_DOT: return "'.'";
        case TOKEN_DOTDOT: return "'..'";
        case TOKEN_COLON: return "':'";
        case TOKEN_EQUALS: return "'='";
        case TOKEN_LPAREN: return "'('";
        case TOKEN_RPAREN: return "')'";
        case TOKEN_LBRACE: return "'{'";
        case TOKEN_RBRACE: return "'}'";
        case TOKEN_LBRACKET: return "'['";
        case TOKEN_RBRACKET: return "']'";
        case TOKEN_LT: return "'<'";
        case TOKEN_GT: return "'>'";
        case TOKEN_PLUS: return "'+'";
        case TOKEN_MINUS: return "'-'";
        case TOKEN_STAR: return "'*'";
        case TOKEN_SLASH: return "'/'";
        case TOKEN_PERCENT: return "'%'";
        case TOKEN_QUESTION: return "'?'";
        case TOKEN_AT: return "'@'";
        case TOKEN_BIT_AND: return "'&'";
        case TOKEN_BIT_OR: return "'|'";
        case TOKEN_BIT_XOR: return "'^'";
        case TOKEN_BIT_NOT: return "'~'";
        case TOKEN_LOG_NOT: return "'!'";
        case TOKEN_EQ: return "'=='";
        case TOKEN_NE: return "'!='";
        case TOKEN_LE: return "'<='";
        case TOKEN_GE: return "'>='";
        case TOKEN_LSHIFT: return "'<<'";
        case TOKEN_RSHIFT: return "'>>'";
        case TOKEN_AND: return "'&&'";
        case TOKEN_OR: return "'||'";
        case TOKEN_UNARY_MINUS: return "unary '-'";
        case TOKEN_UNARY_PLUS: return "unary '+'";
        default: return "unknown token";
    }
}
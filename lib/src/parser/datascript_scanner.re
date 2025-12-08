/*!re2c
    re2c:define:YYCTYPE = char;
    re2c:define:YYCURSOR = ctx->cursor;
    re2c:define:YYLIMIT = ctx->limit;
    re2c:define:YYMARKER = ctx->marker;
    re2c:yyfill:enable = 0;
*/

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "parser/scanner_context.h"
#include "parser/parser_context.h"
#include "gen/datascript_parser.h"  /* Include lemon-generated token definitions */

/* Helper to count newlines and update position */
static void update_position(scanner_context_t* ctx, const char* start, const char* end) {
    while (start < end) {
        if (*start == '\n') {
            ctx->line++;
            ctx->column = 1;
        } else {
            ctx->column++;
        }
        start++;
    }
}

/* Scanner function - returns token type, fills token value */
int parser_scan_token(scanner_context_t* ctx, parser_context_t* pctx, token_value_t* token) {
    const char* start;

scan:
    /* Check if we've reached the end of actual data (not including padding) */
    if (ctx->cursor >= ctx->eof) {
        return 0; /* EOF */
    }

    start = ctx->cursor;
    token->line = ctx->line;
    token->column = ctx->column;

    /*!re2c
        re2c:define:YYCTYPE = char;
        re2c:define:YYCURSOR = ctx->cursor;
        re2c:define:YYLIMIT = ctx->limit;
        re2c:yyfill:enable = 1;
        re2c:define:YYFILL = "goto eof_error;";

        /* End of input */
        "\x00" { return 0; /* Lemon uses 0 for EOF */ }

        /* Whitespace - skip and rescan */
        [ \t\r\n]+ {
            update_position(ctx, start, ctx->cursor);
            goto scan;
        }

        /* Comments */
        "//" [^\n]* "\n"? {
            update_position(ctx, start, ctx->cursor);
            goto scan;
        }

        /* Doc comments - must come before regular block comments */
        "/**" ([^*] | ("*" [^/]))* "*/" {
            token->start = start;
            token->end = ctx->cursor;
            update_position(ctx, start, ctx->cursor);
            return TOKEN_DOC_COMMENT;
        }

        "/*" ([^*] | ("*" [^/]))* "*/" {
            update_position(ctx, start, ctx->cursor);
            goto scan;
        }

        /* Keywords - must come before identifier */
        "const"     { return TOKEN_CONST; }
        "constraint" { return TOKEN_CONSTRAINT; }
        "package"   { return TOKEN_PACKAGE; }
        "import"    { return TOKEN_IMPORT; }
        "struct"    { return TOKEN_STRUCT; }
        "union"     { return TOKEN_UNION; }
        "choice"    { return TOKEN_CHOICE; }
        "enum"      { return TOKEN_ENUM; }
        "bitmask"   { return TOKEN_BITMASK; }
        "subtype"   { return TOKEN_SUBTYPE; }
        "typedef"   { return TOKEN_TYPEDEF; }
        "function"  { return TOKEN_FUNCTION; }
        "return"    { return TOKEN_RETURN; }
        "if"        { return TOKEN_IF; }
        "optional"  { return TOKEN_OPTIONAL; }
        "on"        { return TOKEN_ON; }
        "case"      { return TOKEN_CASE; }
        "default"   { return TOKEN_DEFAULT; }
        "align"     { return TOKEN_ALIGN; }
        "little"    { return TOKEN_LITTLE; }
        "big"       { return TOKEN_BIG; }
        "bool"      { return TOKEN_BOOL; }
        "string"    { return TOKEN_STRING; }
        "u16string" { return TOKEN_U16STRING; }
        "u32string" { return TOKEN_U32STRING; }
        "bit"       { return TOKEN_BIT; }

        /* Integer types - must come before identifier */
        "uint8"     { return TOKEN_UINT8; }
        "uint16"    { return TOKEN_UINT16; }
        "uint32"    { return TOKEN_UINT32; }
        "uint64"    { return TOKEN_UINT64; }
        "uint128"   { return TOKEN_UINT128; }
        "int8"      { return TOKEN_INT8; }
        "int16"     { return TOKEN_INT16; }
        "int32"     { return TOKEN_INT32; }
        "int64"     { return TOKEN_INT64; }
        "int128"    { return TOKEN_INT128; }

        /* Boolean literals */
        "true"      { token->start = start; token->end = ctx->cursor; return TOKEN_BOOL_LITERAL; }
        "false"     { token->start = start; token->end = ctx->cursor; return TOKEN_BOOL_LITERAL; }

        /* Identifiers */
        [a-zA-Z_][a-zA-Z0-9_]* {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_IDENTIFIER;
        }

        /* Integer literals - just store pointers, parse in C++ code */
        "0x" [0-9a-fA-F]+ {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_INTEGER_LITERAL;
        }

        "0b" [01]+ {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_INTEGER_LITERAL;
        }

        "0" [0-7]+ {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_INTEGER_LITERAL;
        }

        [01]+ "b" {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_INTEGER_LITERAL;
        }

        [0-9]+ {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_INTEGER_LITERAL;
        }

        /* String literals - store pointers including quotes, process in C++ code */
        "\"" ([^"\\] | "\\" ["\\nrt])* "\"" {
            token->start = start;
            token->end = ctx->cursor;
            return TOKEN_STRING_LITERAL;
        }

        /* Two-character operators - must come before single-character versions */
        "=="        { return TOKEN_EQ; }
        "!="        { return TOKEN_NE; }
        "<="        { return TOKEN_LE; }
        ">="        { return TOKEN_GE; }
        "<<"        { return TOKEN_LSHIFT; }
        ">>"        { return TOKEN_RSHIFT; }
        "&&"        { return TOKEN_AND; }
        "||"        { return TOKEN_OR; }
        ".."        { return TOKEN_DOTDOT; }

        /* Single-character operators and punctuation */
        ";"         { return TOKEN_SEMICOLON; }
        ","         { return TOKEN_COMMA; }
        "."         { return TOKEN_DOT; }
        ":"         { return TOKEN_COLON; }
        "="         { return TOKEN_EQUALS; }
        "("         { return TOKEN_LPAREN; }
        ")"         { return TOKEN_RPAREN; }
        "{"         { return TOKEN_LBRACE; }
        "}"         { return TOKEN_RBRACE; }
        "["         { return TOKEN_LBRACKET; }
        "]"         { return TOKEN_RBRACKET; }
        "<"         { return TOKEN_LT; }
        ">"         { return TOKEN_GT; }
        "+"         { return TOKEN_PLUS; }
        "-"         { return TOKEN_MINUS; }
        "*"         { return TOKEN_STAR; }
        "/"         { return TOKEN_SLASH; }
        "%"         { return TOKEN_PERCENT; }
        "?"         { return TOKEN_QUESTION; }
        "@"         { return TOKEN_AT; }
        "&"         { return TOKEN_BIT_AND; }
        "|"         { return TOKEN_BIT_OR; }
        "^"         { return TOKEN_BIT_XOR; }
        "~"         { return TOKEN_BIT_NOT; }
        "!"         { return TOKEN_LOG_NOT; }

        /* Catch-all for errors */
        * {
            parser_set_error(pctx, PARSER_ERROR_UNEXPECTED_TOKEN,
                           "Unexpected character '%c' at line %d, column %d",
                           *start, ctx->line, ctx->column);
            return -1;
        }
    */

eof_error:
    /* Hit EOF while trying to match a token (e.g., unclosed string) */
    parser_set_error(pctx, PARSER_ERROR_UNEXPECTED_TOKEN,
                   "Unexpected end of input at line %d, column %d",
                   ctx->line, ctx->column);
    return -1;
}
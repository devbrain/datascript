//
// Created by igor on 26/11/2025.
//

#pragma once

typedef struct scanner_context {
    const char* input;
    const char* cursor;
    const char* limit; /* Points past padding for re2c bounds checking */
    const char* eof; /* Points to end of actual data (where null terminator starts) */
    const char* marker;
    int line;
    int column;
    const char* filename;
} scanner_context_t;


/* Token value - just pointers to buffer and source location */
typedef struct token_value {
    /* Points to token text in input buffer */
    const char* start;  /* First character */
    const char* end;    /* One past last character */

    /* Source location */
    int line;
    int column;
} token_value_t;
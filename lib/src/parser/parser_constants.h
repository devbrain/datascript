//
// Created by igor on 27/11/2025.
//
// Parser constants - named constants to replace magic numbers
//

#ifndef DATASCRIPT_PARSER_CONSTANTS_H
#define DATASCRIPT_PARSER_CONSTANTS_H

#include <cstddef>

namespace datascript::parser {

/* Endianness encoding for C interface */
enum EndianEncoding {
    ENDIAN_UNSPEC = 0,
    ENDIAN_LITTLE = 1,
    ENDIAN_BIG = 2
};

/* Buffer configuration */
constexpr size_t INPUT_BUFFER_PADDING = 32;  // Bytes of padding for re2c lookahead

/* Error message buffer size */
constexpr size_t ERROR_MESSAGE_BUFFER_SIZE = 256;

/* Parser limits (prevent DoS attacks) */
constexpr size_t MAX_IDENTIFIER_LENGTH = 256;
constexpr size_t MAX_STRING_LITERAL_LENGTH = 65536;  // 64KB
constexpr size_t MAX_EXPRESSION_DEPTH = 1000;

} // namespace datascript::parser

#endif // DATASCRIPT_PARSER_CONSTANTS_H

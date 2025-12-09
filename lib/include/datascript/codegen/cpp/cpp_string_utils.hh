//
// C++ String Utilities for Code Generation
//
// Provides string formatting utilities for generating valid C++ code:
// - String literal escaping (for docstrings, comments, etc.)
// - Identifier sanitization
//

#pragma once

#include <string>

namespace datascript::codegen {

/**
 * Escape special characters in a string for use in C++ string literals.
 *
 * Handles: newlines (\n), carriage returns (\r), tabs (\t),
 * backslashes (\\), and double quotes (\").
 *
 * @param str The raw string to escape
 * @return The escaped string safe for use in C++ string literals
 */
inline std::string escape_cpp_string_literal(const std::string& str) {
    std::string result;
    result.reserve(str.size() + str.size() / 4);  // Reserve extra for escapes

    for (char c : str) {
        switch (c) {
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            case '\\': result += "\\\\"; break;
            case '"':  result += "\\\""; break;
            default:   result += c; break;
        }
    }

    return result;
}

}  // namespace datascript::codegen

/*
 * C++ Bridge for DataScript Parser
 * Provides extern "C" interface for C parser/lexer to call C++ AST building code
 */

#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <memory>
#include <vector>
#include <deque>
#include <charconv>
#include <optional>

#define DATASCRIPT_PARSER_CONTEXT_VISIBLE

#include "parser/ast_builder.h"
#include "parser/ast_holder.hh"
#include "parser/parser_context.h"
#include "parser/parser_constants.h"

namespace {
    datascript::ast::source_pos make_pos(parser_context_t* ctx) {
        return datascript::ast::source_pos{
            ctx->m_scanner->filename,
            static_cast <size_t>(ctx->m_scanner->line),
            static_cast <size_t>(ctx->m_scanner->column)
        };
    }

    template<typename InternalType, typename ExternalType>
    struct types {
        template<typename TypePtr, typename... Args>
        static InternalType* make(parser_context_t* ctx, TypePtr* elem, Args&&... args) {
            using namespace datascript::ast;
            ctx->ast_builder->temp_types.push_back(type{
                    type_node{
                        ExternalType{
                            make_pos(ctx),
                            std::make_unique <type>(
                                std::move(*reinterpret_cast <type*>(elem))
                            ),
                            std::forward <Args>(args)...
                        }
                    }
                }
            );
            return reinterpret_cast <InternalType*>(&ctx->ast_builder->temp_types.back());
        }
    };

    template<typename InternalType, typename ExternalType>
    struct simple_types {
        template<typename... Args>
        static InternalType* make(parser_context_t* ctx, Args&&... args) {
            using namespace datascript::ast;
            ctx->ast_builder->temp_types.push_back(type{
                    type_node{
                        ExternalType{
                            make_pos(ctx),
                            std::forward <Args>(args)...
                        }
                    }
                }
            );
            return reinterpret_cast <InternalType*>(&ctx->ast_builder->temp_types.back());
        }
    };

    template<typename InternalType, typename ExternalType>
    struct exprs {
        template<typename... Args>
        static InternalType* make(parser_context_t* ctx, Args&&... args) {
            using namespace datascript::ast;
            ctx->ast_builder->temp_exprs.push_back(expr{
                expr_node{
                    ExternalType{
                        make_pos(ctx),
                        std::forward <Args>(args)...
                    }
                }
            });
            return reinterpret_cast <InternalType*>(&ctx->ast_builder->temp_exprs.back());
        }
    };
}

/* Helper to extract string from token (creates a std::string) */
static std::string extract_string(const token_value_t* tok) {
    if (!tok || !tok->start || !tok->end) {
        return {};
    }
    return std::string(tok->start, static_cast<size_t>(tok->end - tok->start));
}

/* Helper to process docstring: extract content from Javadoc-style comment, strip leading asterisks and whitespace */
static std::optional <std::string> process_docstring(const token_value_t* tok) {
    if (!tok || !tok->start || !tok->end) {
        return std::nullopt;
    }

    const char* start = tok->start;
    const char* end = tok->end;

    // Skip /** at beginning
    if (end - start < 5) return std::nullopt; // Minimum: /***/
    start += 3; // Skip /**

    // Skip */ at end
    end -= 2; // Skip */

    std::string result;
    bool at_line_start = true;

    for (const char* p = start; p < end; ++p) {
        char ch = *p;

        if (ch == '\n') {
            result += ch;
            at_line_start = true;
        } else if (at_line_start) {
            // Skip leading whitespace and * at start of line
            if (ch == ' ' || ch == '\t') {
                continue; // Skip whitespace
            } else if (ch == '*') {
                // Skip the * and one following space if present
                if (p + 1 < end && (*(p + 1) == ' ' || *(p + 1) == '\t')) {
                    ++p; // Skip the space after *
                }
                at_line_start = false;
            } else {
                result += ch;
                at_line_start = false;
            }
        } else {
            result += ch;
        }
    }

    // Trim trailing whitespace
    while (!result.empty() && (result.back() == ' ' || result.back() == '\t' || result.back() == '\n' || result.back()
                               == '\r')) {
        result.pop_back();
    }

    // Trim leading whitespace
    size_t first_non_space = result.find_first_not_of(" \t\n\r");
    if (first_non_space != std::string::npos) {
        result = result.substr(first_non_space);
    } else if (!result.empty()) {
        result.clear(); // All whitespace
    }

    return result.empty() ? std::nullopt : std::optional <std::string>(result);
}

/* Helper to parse integer literal from token text using C++20 std::from_chars
 * Returns std::nullopt on overflow or parse error */
static std::optional <std::uint64_t> parse_integer_safe(
    const token_value_t* tok,
    parser_context_t* ctx) {
    if (!tok || !tok->start || !tok->end) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Invalid token for integer parsing");
        return std::nullopt;
    }

    const char* start = tok->start;
    const char* end = tok->end;
    size_t len = static_cast<size_t>(end - start);

    if (len == 0) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Empty integer literal at line %d", tok->line);
        return std::nullopt;
    }

    // Determine base and adjust parse range
    int base = 10;
    const char* parse_start = start;
    const char* parse_end = end;

    // Hexadecimal: 0xABCD or 0XABCD
    if (len > 2 && start[0] == '0' && (start[1] == 'x' || start[1] == 'X')) {
        base = 16;
        parse_start = start + 2;
    }
    // Binary: 0b1010 or 0B1010
    else if (len > 2 && start[0] == '0' && (start[1] == 'b' || start[1] == 'B')) {
        base = 2;
        parse_start = start + 2;
    }
    // Binary suffix: 1010b or 1010B
    else if (len > 1 && (end[-1] == 'b' || end[-1] == 'B')) {
        base = 2;
        parse_end = end - 1; // Exclude 'b' suffix
    }
    // Octal: 0777
    else if (len > 1 && start[0] == '0' && start[1] >= '0' && start[1] <= '7') {
        base = 8;
    }

    // Validate that we have digits to parse
    if (parse_start >= parse_end) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Integer literal has no digits at line %d column %d",
                         tok->line, tok->column);
        return std::nullopt;
    }

    // Parse using std::from_chars (C++20)
    std::uint64_t value = 0;
    auto [ptr, ec] = std::from_chars(parse_start, parse_end, value, base);

    // Check for errors
    if (ec == std::errc::invalid_argument) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Invalid integer literal at line %d column %d: no valid digits",
                         tok->line, tok->column);
        return std::nullopt;
    }

    if (ec == std::errc::result_out_of_range) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Integer literal overflow at line %d column %d: value exceeds 64 bits",
                         tok->line, tok->column);
        return std::nullopt;
    }

    // Verify all characters were consumed
    if (ptr != parse_end) {
        parser_set_error(ctx, PARSER_ERROR_INVALID_LITERAL,
                         "Invalid characters in integer literal at line %d column %d",
                         tok->line, tok->column);
        return std::nullopt;
    }

    return value;
}

/* Helper to parse boolean literal from token text */
static bool parse_bool(const token_value_t* tok) {
    if (!tok || !tok->start || !tok->end) {
        return false;
    }
    /* "true" vs "false" - just check first character */
    return tok->start[0] == 't';
}

/* Helper to extract and process string literal with escape sequences */
static std::string parse_string_literal(const token_value_t* tok) {
    if (!tok || !tok->start || !tok->end) {
        return {};
    }

    /* Skip opening and closing quotes */
    const char* src = tok->start + 1;
    const char* end = tok->end - 1;

    std::string result;
    result.reserve(static_cast<size_t>(end - src));

    while (src < end) {
        if (*src == '\\' && src + 1 < end) {
            src++;
            switch (*src) {
                case 'n': result += '\n';
                    break;
                case 'r': result += '\r';
                    break;
                case 't': result += '\t';
                    break;
                case '\\': result += '\\';
                    break;
                case '"': result += '"';
                    break;
                default: result += *src;
                    break;
            }
            src++;
        } else {
            result += *src++;
        }
    }
    return result;
}

using namespace datascript::ast;

/* Array type builders */
ast_array_type_fixed_t*
parser_build_array_type_fixed(parser_context_t* ctx, ast_type_t* element_type, ast_expr_t* size) {
    if (!ctx || !ctx->ast_builder || !element_type || !size) {
        return nullptr;
    }

    try {
        /* Cast pointers back to C++ types */
        auto* size_ptr = reinterpret_cast <expr*>(size);
        return types <ast_array_type_fixed_t, array_type_fixed>::make(ctx,
                                                                      element_type,
                                                                      std::move(*size_ptr));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build array type: %s", e.what());
        return nullptr;
    }
}

ast_array_type_range_t* parser_build_array_type_range(parser_context_t* ctx, ast_type_t* element_type,
                                                      ast_expr_t* min_size, ast_expr_t* max_size) {
    if (!ctx || !ctx->ast_builder || !element_type || !max_size) {
        return nullptr;
    }

    try {
        /* Cast pointers back to C++ types */
        auto* max_ptr = reinterpret_cast <expr*>(max_size);

        /* Optional min size */
        std::optional <expr> min_opt;
        if (min_size) {
            auto* min_ptr = reinterpret_cast <expr*>(min_size);
            min_opt = std::move(*min_ptr);
        }

        return types <ast_array_type_range_t, array_type_range>::make(
            ctx, element_type,
            std::move(min_opt),
            std::move(*max_ptr));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build range array type: %s", e.what());
        return nullptr;
    }
}

ast_array_type_unsized_t* parser_build_array_type_unsized(parser_context_t* ctx, ast_type_t* element_type) {
    if (!ctx || !ctx->ast_builder || !element_type) {
        return nullptr;
    }

    try {
        /* Cast pointer back to C++ type */
        return types <ast_array_type_unsized_t, array_type_unsized>::make(ctx, element_type);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build unsized array type: %s", e.what());
        return nullptr;
    }
}

/* Type conversion helpers - zero-cost upcasts */

ast_type_t* parser_primitive_to_type(ast_primitive_type_t* prim) {
    return reinterpret_cast <ast_type_t*>(prim);
}

ast_type_t* parser_string_to_type(ast_string_type_t* str) {
    return reinterpret_cast <ast_type_t*>(str);
}

ast_type_t* parser_u16_string_to_type(ast_u16_string_type_t* str) {
    return reinterpret_cast <ast_type_t*>(str);
}

ast_type_t* parser_u32_string_to_type(ast_u32_string_type_t* str) {
    return reinterpret_cast <ast_type_t*>(str);
}

ast_type_t* parser_bool_to_type(ast_bool_type_t* b) {
    return reinterpret_cast <ast_type_t*>(b);
}

ast_type_t* parser_bitfield_to_type(ast_bitfield_type_t* bf) {
    return reinterpret_cast <ast_type_t*>(bf);
}

ast_type_t* parser_qualified_name_to_type(ast_qualified_name_t* qname) {
    return reinterpret_cast <ast_type_t*>(qname);
}

ast_type_t* parser_array_fixed_to_type(ast_array_type_fixed_t* arr) {
    return reinterpret_cast <ast_type_t*>(arr);
}

ast_type_t* parser_array_range_to_type(ast_array_type_range_t* arr) {
    return reinterpret_cast <ast_type_t*>(arr);
}

ast_type_t* parser_array_unsized_to_type(ast_array_type_unsized_t* arr) {
    return reinterpret_cast <ast_type_t*>(arr);
}

ast_expr_t* parser_literal_int_to_expr(ast_expr_literal_int_t* lit) {
    return reinterpret_cast <ast_expr_t*>(lit);
}

ast_expr_t* parser_literal_string_to_expr(ast_expr_literal_string_t* lit) {
    return reinterpret_cast <ast_expr_t*>(lit);
}

ast_expr_t* parser_literal_bool_to_expr(ast_expr_literal_bool_t* lit) {
    return reinterpret_cast <ast_expr_t*>(lit);
}

ast_expr_t* parser_identifier_to_expr(ast_expr_identifier_t* id) {
    return reinterpret_cast <ast_expr_t*>(id);
}

ast_expr_t* parser_unary_to_expr(ast_expr_unary_t* unary) {
    return reinterpret_cast <ast_expr_t*>(unary);
}

ast_expr_t* parser_binary_to_expr(ast_expr_binary_t* binary) {
    return reinterpret_cast <ast_expr_t*>(binary);
}

ast_expr_t* parser_ternary_to_expr(ast_expr_ternary_t* ternary) {
    return reinterpret_cast <ast_expr_t*>(ternary);
}

ast_expr_t* parser_field_access_to_expr(ast_expr_field_access_t* field_access) {
    return reinterpret_cast <ast_expr_t*>(field_access);
}

ast_expr_t* parser_array_index_to_expr(ast_expr_array_index_t* array_index) {
    return reinterpret_cast <ast_expr_t*>(array_index);
}

ast_expr_t* parser_function_call_to_expr(ast_expr_function_call_t* function_call) {
    return reinterpret_cast <ast_expr_t*>(function_call);
}

/* AST building callbacks - type-safe builders */

void parser_build_constant(parser_context_t* ctx, ast_type_t* type, token_value_t* name_tok, ast_expr_t* expr,
                           token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !type || !name_tok || !expr) {
        return;
    }

    try {
        /* Cast opaque pointers back to C++ types */
        auto* type_ptr = reinterpret_cast <datascript::ast::type*>(type);
        auto* expr_ptr = reinterpret_cast <datascript::ast::expr*>(expr);

        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Create constant definition */
        constant_def constant{
            make_pos(ctx),
            name,
            std::move(*type_ptr),
            std::move(*expr_ptr),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->constants.push_back(std::move(constant));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build constant: %s", e.what());
    }
}

void parser_build_subtype(parser_context_t* ctx, ast_type_t* base_type, token_value_t* name_tok,
                          ast_expr_t* constraint_expr, token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !base_type || !name_tok || !constraint_expr) {
        return;
    }

    try {
        /* Cast opaque pointers back to C++ types */
        auto* type_ptr = reinterpret_cast <datascript::ast::type*>(base_type);
        auto* expr_ptr = reinterpret_cast <datascript::ast::expr*>(constraint_expr);

        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Create subtype definition */
        subtype_def subtype{
            make_pos(ctx),
            name,
            std::move(*type_ptr),
            std::move(*expr_ptr),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->subtypes.push_back(std::move(subtype));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build subtype: %s", e.what());
    }
}

void parser_build_type_alias(parser_context_t* ctx, token_value_t* name_tok, ast_type_t* target_type,
                             token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !name_tok || !target_type) {
        return;
    }

    try {
        /* Cast opaque pointers back to C++ types */
        auto* type_ptr = reinterpret_cast <datascript::ast::type*>(target_type);

        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Create type alias definition */
        type_alias_def alias{
            make_pos(ctx),
            name,
            std::move(*type_ptr),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->type_aliases.push_back(std::move(alias));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build type alias: %s", e.what());
    }
}

ast_primitive_type_t* parser_build_integer_type(parser_context_t* ctx, int is_signed, int bits, int endian) {
    if (!ctx || !ctx->ast_builder) {
        return nullptr;
    }

    try {
        auto e = endian::unspec;
        if (endian == datascript::parser::ENDIAN_LITTLE) e = endian::little;
        else if (endian == datascript::parser::ENDIAN_BIG) e = endian::big;

        return simple_types <ast_primitive_type_t, primitive_type>::make(ctx, is_signed != 0,
                                                                         static_cast <std::uint32_t>(bits),
                                                                         e);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build integer type: %s", e.what());
        return nullptr;
    }
}

ast_primitive_type_t* parser_build_integer_type_with_endian(parser_context_t* ctx, ast_primitive_type_t* type,
                                                            int endian) {
    if (!ctx || !type) {
        return nullptr;
    }

    try {
        /* Cast back to C++ type and modify endianness */
        auto* type_ptr = reinterpret_cast <datascript::ast::type*>(type);

        /* Check if it's a primitive type */
        if (auto* prim = std::get_if <primitive_type>(&type_ptr->node)) {
            /* Modify the endianness */
            if (endian == datascript::parser::ENDIAN_LITTLE) {
                prim->byte_order = endian::little;
            } else if (endian == datascript::parser::ENDIAN_BIG) {
                prim->byte_order = endian::big;
            }
        }
        return type;
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to set endianness: %s", e.what());
        return nullptr;
    }
}

ast_string_type_t* parser_build_string_type(parser_context_t* ctx) {
    if (!ctx || !ctx->ast_builder) {
        return nullptr;
    }

    try {
        return simple_types <ast_string_type_t, string_type>::make(ctx);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build string type: %s", e.what());
        return nullptr;
    }
}

ast_u16_string_type_t* parser_build_u16_string_type(parser_context_t* ctx, int endian_param) {
    if (!ctx || !ctx->ast_builder) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;
        endian e = endian::unspec;
        if (endian_param == datascript::parser::ENDIAN_LITTLE) e = endian::little;
        else if (endian_param == datascript::parser::ENDIAN_BIG) e = endian::big;

        return simple_types <ast_u16_string_type_t, u16_string_type>::make(ctx, e);
    } catch (const std::exception& ex) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build u16string type: %s", ex.what());
        return nullptr;
    }
}

ast_u32_string_type_t* parser_build_u32_string_type(parser_context_t* ctx, int endian_param) {
    if (!ctx || !ctx->ast_builder) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;
        endian e = endian::unspec;
        if (endian_param == datascript::parser::ENDIAN_LITTLE) e = endian::little;
        else if (endian_param == datascript::parser::ENDIAN_BIG) e = endian::big;

        return simple_types <ast_u32_string_type_t, u32_string_type>::make(ctx, e);
    } catch (const std::exception& ex) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build u32string type: %s", ex.what());
        return nullptr;
    }
}

ast_bool_type_t* parser_build_bool_type(parser_context_t* ctx) {
    if (!ctx || !ctx->ast_builder) {
        return nullptr;
    }

    try {
        return simple_types <ast_bool_type_t, bool_type>::make(ctx);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build bool type: %s", e.what());
        return nullptr;
    }
}

ast_bitfield_type_t* parser_build_bit_field(parser_context_t* ctx, token_value_t* width_tok) {
    if (!ctx || !ctx->ast_builder || !width_tok) {
        return nullptr;
    }

    try {
        auto width_opt = parse_integer_safe(width_tok, ctx);
        if (!width_opt) {
            return nullptr; // Error already set by parse_integer_safe
        }
        std::uint64_t width = *width_opt;
        return simple_types <ast_bitfield_type_t, bit_field_type_fixed>::make(ctx, width);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build bit field: %s", e.what());
        return nullptr;
    }
}

ast_bitfield_type_t* parser_build_bit_field_expr(parser_context_t* ctx, ast_expr_t* width_expr) {
    if (!ctx || !ctx->ast_builder || !width_expr) {
        return nullptr;
    }

    try {
        auto* expr_ptr = reinterpret_cast <expr*>(width_expr);
        return simple_types <ast_bitfield_type_t, bit_field_type_expr>::make(ctx, std::move(*expr_ptr));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build dynamic bit field: %s", e.what());
        return nullptr;
    }
}

ast_qualified_name_t* parser_build_qualified_name_single(parser_context_t* ctx, token_value_t* ident_tok) {
    if (!ctx || !ctx->ast_builder || !ident_tok) {
        return nullptr;
    }

    try {
        /* Extract identifier name from token */
        std::string name = extract_string(ident_tok);
        return simple_types <ast_qualified_name_t, qualified_name>::make(ctx, std::vector <std::string>{name});
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build qualified name: %s", e.what());
        return nullptr;
    }
}

ast_qualified_name_t* parser_build_qualified_name_append(parser_context_t* ctx, ast_qualified_name_t* qname,
                                                         token_value_t* ident_tok) {
    if (!ctx || !qname || !ident_tok) {
        return nullptr;
    }

    try {
        /* Cast back to C++ type */
        auto* qname_ptr = reinterpret_cast <type*>(qname);

        /* Check if it's a qualified_name */
        if (auto* qualified = std::get_if <qualified_name>(&qname_ptr->node)) {
            /* Extract identifier name from token */
            std::string name = extract_string(ident_tok);

            /* Append to parts vector */
            qualified->parts.push_back(name);
        }

        return qname;
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append to qualified name: %s", e.what());
        return nullptr;
    }
}

ast_expr_literal_int_t* parser_build_integer_literal(parser_context_t* ctx, token_value_t* literal_tok) {
    if (!ctx || !ctx->ast_builder || !literal_tok) {
        return nullptr;
    }

    try {
        auto value_opt = parse_integer_safe(literal_tok, ctx);
        if (!value_opt) {
            return nullptr; // Error already set by parse_integer_safe
        }
        std::uint64_t value = *value_opt;
        return exprs <ast_expr_literal_int_t, literal_int>::make(ctx, value);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build integer literal: %s", e.what());
        return nullptr;
    }
}

ast_expr_literal_string_t* parser_build_string_literal(parser_context_t* ctx, token_value_t* literal_tok) {
    if (!ctx || !ctx->ast_builder || !literal_tok) {
        return nullptr;
    }

    try {
        /* Parse string with escape sequences from token */
        std::string value = parse_string_literal(literal_tok);
        return exprs <ast_expr_literal_string_t, literal_string>::make(ctx, value);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build string literal: %s", e.what());
        return nullptr;
    }
}

ast_expr_literal_bool_t* parser_build_bool_literal(parser_context_t* ctx, token_value_t* literal_tok) {
    if (!ctx || !ctx->ast_builder || !literal_tok) {
        return nullptr;
    }

    try {
        bool value = parse_bool(literal_tok);
        return exprs <ast_expr_literal_bool_t, literal_bool>::make(ctx, value);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build bool literal: %s", e.what());
        return nullptr;
    }
}

ast_expr_identifier_t* parser_build_identifier(parser_context_t* ctx, token_value_t* ident_tok) {
    if (!ctx || !ctx->ast_builder || !ident_tok) {
        return nullptr;
    }

    try {
        std::string name = extract_string(ident_tok);
        return exprs <ast_expr_identifier_t, identifier>::make(ctx, name);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build identifier: %s", e.what());
        return nullptr;
    }
}

ast_expr_unary_t* parser_build_unary_expr(parser_context_t* ctx, int op, ast_expr_t* operand) {
    if (!ctx || !ctx->ast_builder || !operand) {
        return nullptr;
    }

    try {
        /* Cast opaque pointer back to C++ type */
        auto* operand_ptr = reinterpret_cast <expr*>(operand);

        /* Convert int op to unary_op enum */
        unary_op unary_op;
        switch (op) {
            case PARSER_UNARY_NEG: unary_op = unary_op::neg;
                break;
            case PARSER_UNARY_POS: unary_op = unary_op::pos;
                break;
            case PARSER_UNARY_BIT_NOT: unary_op = unary_op::bit_not;
                break;
            case PARSER_UNARY_LOG_NOT: unary_op = unary_op::log_not;
                break;
            default:
                parser_set_error(ctx, PARSER_ERROR_INTERNAL, "Invalid unary operator: %d", op);
                return nullptr;
        }
        return exprs <ast_expr_unary_t, unary_expr>::make(ctx, unary_op,
                                                          std::make_unique <expr>(
                                                              std::move(*operand_ptr)));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build unary expression: %s", e.what());
        return nullptr;
    }
}

ast_expr_binary_t* parser_build_binary_expr(parser_context_t* ctx, int op, ast_expr_t* left, ast_expr_t* right) {
    if (!ctx || !ctx->ast_builder || !left || !right) {
        return nullptr;
    }

    try {
        /* Cast opaque pointers back to C++ types */
        auto* left_ptr = reinterpret_cast <expr*>(left);
        auto* right_ptr = reinterpret_cast <expr*>(right);

        /* Convert int op to binary_op enum */
        binary_op binary_op;
        switch (op) {
            case PARSER_BINARY_ADD: binary_op = binary_op::add;
                break;
            case PARSER_BINARY_SUB: binary_op = binary_op::sub;
                break;
            case PARSER_BINARY_MUL: binary_op = binary_op::mul;
                break;
            case PARSER_BINARY_DIV: binary_op = binary_op::div;
                break;
            case PARSER_BINARY_MOD: binary_op = binary_op::mod;
                break;
            case PARSER_BINARY_EQ: binary_op = binary_op::eq;
                break;
            case PARSER_BINARY_NE: binary_op = binary_op::ne;
                break;
            case PARSER_BINARY_LT: binary_op = binary_op::lt;
                break;
            case PARSER_BINARY_GT: binary_op = binary_op::gt;
                break;
            case PARSER_BINARY_LE: binary_op = binary_op::le;
                break;
            case PARSER_BINARY_GE: binary_op = binary_op::ge;
                break;
            case PARSER_BINARY_BIT_AND: binary_op = binary_op::bit_and;
                break;
            case PARSER_BINARY_BIT_OR: binary_op = binary_op::bit_or;
                break;
            case PARSER_BINARY_BIT_XOR: binary_op = binary_op::bit_xor;
                break;
            case PARSER_BINARY_LSHIFT: binary_op = binary_op::lshift;
                break;
            case PARSER_BINARY_RSHIFT: binary_op = binary_op::rshift;
                break;
            case PARSER_BINARY_LOG_AND: binary_op = binary_op::log_and;
                break;
            case PARSER_BINARY_LOG_OR: binary_op = binary_op::log_or;
                break;
            default:
                parser_set_error(ctx, PARSER_ERROR_INTERNAL, "Invalid binary operator: %d", op);
                return nullptr;
        }
        return exprs <ast_expr_binary_t, binary_expr>::make(ctx, binary_op,
                                                            std::make_unique <expr>(
                                                                std::move(*left_ptr)),
                                                            std::make_unique <expr>(
                                                                std::move(*right_ptr)));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build binary expression: %s", e.what());
        return nullptr;
    }
}

ast_expr_ternary_t* parser_build_ternary_expr(parser_context_t* ctx, ast_expr_t* condition, ast_expr_t* true_expr,
                                              ast_expr_t* false_expr) {
    if (!ctx || !ctx->ast_builder || !condition || !true_expr || !false_expr) {
        return nullptr;
    }

    try {
        /* Cast opaque pointers back to C++ types */
        auto* cond_ptr = reinterpret_cast <expr*>(condition);
        auto* true_ptr = reinterpret_cast <expr*>(true_expr);
        auto* false_ptr = reinterpret_cast <expr*>(false_expr);

        return exprs <ast_expr_ternary_t, ternary_expr>::make(ctx, std::make_unique <expr>(std::move(*cond_ptr)),
                                                              std::make_unique <expr>(std::move(*true_ptr)),
                                                              std::make_unique <expr>(std::move(*false_ptr)));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build ternary expression: %s", e.what());
        return nullptr;
    }
}

ast_expr_field_access_t* parser_build_field_access_expr(parser_context_t* ctx, ast_expr_t* object,
                                                        token_value_t* field_tok) {
    if (!ctx || !ctx->ast_builder || !object || !field_tok) {
        return nullptr;
    }

    try {
        /* Cast object pointer back to C++ type */
        auto* obj_ptr = reinterpret_cast <expr*>(object);

        /* Extract field name from token */
        std::string field_name = extract_string(field_tok);

        return exprs <ast_expr_field_access_t, field_access_expr>::make(
            ctx, std::make_unique <expr>(std::move(*obj_ptr)),
            field_name);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field access expression: %s", e.what());
        return nullptr;
    }
}

ast_expr_array_index_t* parser_build_array_index_expr(parser_context_t* ctx, ast_expr_t* array, ast_expr_t* index) {
    if (!ctx || !ctx->ast_builder || !array || !index) {
        return nullptr;
    }

    try {
        /* Cast pointers back to C++ types */
        auto* array_ptr = reinterpret_cast <expr*>(array);
        auto* index_ptr = reinterpret_cast <expr*>(index);

        return exprs <ast_expr_array_index_t, array_index_expr>::make(ctx,
                                                                      std::make_unique <expr>(
                                                                          std::move(*array_ptr)),
                                                                      std::make_unique <expr>(
                                                                          std::move(*index_ptr))
        );
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build array index expression: %s", e.what());
        return nullptr;
    }
}

/* Helper struct to hold argument list during parsing */
struct arg_list_holder {
    std::vector <ast_expr_t*> args;
};

namespace datascript::parser {
    /* Custom deleters for each holder type */
    struct arg_list_deleter {
        void operator()(arg_list_holder* p) const { delete p; }
    };

    using arg_list_guard = std::unique_ptr <arg_list_holder, arg_list_deleter>;
}

ast_arg_list_t* parser_build_arg_list_single(parser_context_t* ctx, ast_expr_t* arg) {
    if (!ctx || !arg) {
        return nullptr;
    }

    try {
        /* Create new argument list holder */
        auto* arg_list = new arg_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::arg_list_guard guard(arg_list);
        arg_list->args.push_back(arg);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_arg_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build argument list: %s", e.what());
        return nullptr;
    }
}

ast_arg_list_t* parser_build_arg_list_append(parser_context_t* ctx, ast_arg_list_t* list, ast_expr_t* arg) {
    if (!ctx || !list || !arg) {
        /* Clean up list on error - use guard for exception safety */
        datascript::parser::arg_list_guard list_guard(reinterpret_cast <arg_list_holder*>(list));
        return nullptr;
    }

    try {
        /* Cast back to ArgListHolder */
        auto* arg_list = reinterpret_cast <arg_list_holder*>(list);
        arg_list->args.push_back(arg);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::arg_list_guard list_guard(reinterpret_cast <arg_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append to argument list: %s", e.what());
        return nullptr;
    }
}

ast_expr_function_call_t* parser_build_function_call_expr(parser_context_t* ctx, ast_expr_t* function,
                                                          ast_arg_list_t* args) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::arg_list_guard arg_guard(reinterpret_cast <arg_list_holder*>(args));

    if (!ctx || !ctx->ast_builder || !function) {
        return nullptr;
    }

    try {
        /* Cast function pointer back to C++ type */
        auto* func_ptr = reinterpret_cast <expr*>(function);

        /* Build argument vector */
        std::vector <expr> arguments;
        if (arg_guard) {
            for (auto* arg_ptr : arg_guard->args) {
                auto* expr_ptr = reinterpret_cast <expr*>(arg_ptr);
                arguments.push_back(std::move(*expr_ptr));
            }
        }
        return exprs <ast_expr_function_call_t, function_call_expr>::make(
            ctx, std::make_unique <expr>(std::move(*func_ptr)),
            std::move(arguments));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build function call expression: %s", e.what());
        return nullptr;
    }
    // arg_guard automatically deletes on scope exit
}

ast_expr_function_call_t* parser_build_function_call_expr_noargs(parser_context_t* ctx, ast_expr_t* function) {
    return parser_build_function_call_expr(ctx, function, nullptr);
}

/* Helper struct to hold parameter list during parsing */
struct param_list_holder {
    std::vector <ast_param_t*> params;
};

namespace datascript::parser {
    struct param_list_deleter {
        void operator()(param_list_holder* p) const { delete p; }
    };

    using param_list_guard = std::unique_ptr <param_list_holder, param_list_deleter>;
}

ast_param_t* parser_build_param(parser_context_t* ctx, ast_type_t* type, token_value_t* name_tok) {
    if (!ctx || !ctx->ast_builder || !type || !name_tok) {
        return nullptr;
    }

    try {
        /* Cast opaque pointer back to C++ type */
        auto* type_ptr = reinterpret_cast <datascript::ast::type*>(type);

        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Create parameter */
        ctx->ast_builder->temp_params.emplace_back(
            param{
                make_pos(ctx),
                std::move(*type_ptr),
                name
            }
        );

        /* Return pointer to element in deque */
        return reinterpret_cast <ast_param_t*>(&ctx->ast_builder->temp_params.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build parameter: %s", e.what());
        return nullptr;
    }
}

ast_param_list_t* parser_build_param_list_single(parser_context_t* ctx, ast_param_t* param) {
    if (!ctx || !param) {
        return nullptr;
    }

    try {
        auto* list = new param_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::param_list_guard guard(list);
        list->params.push_back(param);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_param_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_MEMORY, "Failed to create parameter list: %s", e.what());
        return nullptr;
    }
}

ast_param_list_t* parser_build_param_list_append(parser_context_t* ctx, ast_param_list_t* list, ast_param_t* param) {
    if (!ctx || !list || !param) {
        /* Clean up list on error - use guard for exception safety */
        datascript::parser::param_list_guard list_guard(reinterpret_cast <param_list_holder*>(list));
        return nullptr;
    }

    try {
        auto* param_list = reinterpret_cast <param_list_holder*>(list);
        param_list->params.push_back(param);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::param_list_guard list_guard(reinterpret_cast <param_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_MEMORY, "Failed to append parameter: %s", e.what());
        return nullptr;
    }
}

void parser_build_constraint_with_list(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* param_list,
                                       ast_expr_t* condition, token_value_t* docstring) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::param_list_guard param_guard(reinterpret_cast <param_list_holder*>(param_list));

    if (!ctx || !ctx->ast_builder || !name_tok || !condition) {
        return;
    }

    try {
        /* Extract parameters from list */
        ast_param_t** params = param_guard ? param_guard->params.data() : nullptr;
        int param_count = param_guard ? static_cast <int>(param_guard->params.size()) : 0;

        /* Build constraint */
        parser_build_constraint(ctx, name_tok, params, param_count, condition, docstring);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build constraint: %s", e.what());
    }
    // param_guard automatically deletes on scope exit
}

void parser_build_constraint(parser_context_t* ctx, token_value_t* name_tok, ast_param_t** params, int param_count,
                             ast_expr_t* condition, token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !name_tok || !condition) {
        return;
    }

    try {
        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Cast condition pointer back to C++ type */
        auto* cond_ptr = reinterpret_cast <expr*>(condition);

        /* Build parameter vector */
        std::vector <param> param_vec;
        if (params && param_count > 0) {
            param_vec.reserve(static_cast<size_t>(param_count));
            for (int i = 0; i < param_count; i++) {
                auto* param_ptr = reinterpret_cast <param*>(params[i]);
                param_vec.push_back(std::move(*param_ptr));
            }
        }

        /* Create constraint definition */
        constraint_def constraint{
            make_pos(ctx),
            name,
            std::move(param_vec),
            std::move(*cond_ptr),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->constraints.push_back(std::move(constraint));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build constraint: %s", e.what());
    }
}

/* Package and import builders */
void parser_build_package(parser_context_t* ctx, ast_qualified_name_t* qname) {
    if (!ctx || !ctx->ast_builder || !qname) {
        return;
    }

    try {
        auto* holder = ctx->ast_builder;

        /* Cast qualified name pointer back to C++ type */
        auto* qname_ptr = reinterpret_cast <qualified_name*>(qname);

        /* Check if package already declared */
        if (holder->module->package.has_value()) {
            parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Package already declared at line %d", ctx->m_scanner->line);
            return;
        }

        /* Create package declaration */
        package_decl package{
            make_pos(ctx),
            qname_ptr->parts
        };

        /* Set package in module */
        holder->module->package = std::move(package);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build package: %s", e.what());
    }
}

void parser_build_import(parser_context_t* ctx, ast_qualified_name_t* qname, int is_wildcard) {
    if (!ctx || !ctx->ast_builder || !qname) {
        return;
    }

    try {
        /* Cast qualified name pointer back to C++ type */
        auto* qname_ptr = reinterpret_cast <qualified_name*>(qname);

        /* Create import declaration */
        import_decl import_stmt{
            make_pos(ctx),
            qname_ptr->parts,
            is_wildcard != 0
        };

        /* Add to module */
        ctx->ast_builder->module->imports.push_back(std::move(import_stmt));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build import: %s", e.what());
    }
}

/* Global endianness directive */
void parser_set_default_endianness(parser_context_t* ctx, int endian) {
    if (!ctx || !ctx->ast_builder) {
        return;
    }

    try {
        auto* holder = ctx->ast_builder;

        /* Set default endianness: 1 = little, 2 = big */
        if (endian == 1) {
            holder->module->default_endianness = endian::little;
        } else if (endian == 2) {
            holder->module->default_endianness = endian::big;
        } else {
            parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Invalid endianness value: %d", endian);
        }
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to set default endianness: %s", e.what());
    }
}

/* Helper struct to hold enum item list during parsing */
struct enum_item_list_holder {
    std::vector <ast_enum_item_t*> items;
};

struct field_list_holder {
    std::vector <ast_field_def_t*> fields;
};

struct expr_list_holder {
    std::vector <ast_expr_t*> exprs;
};

struct choice_case_list_holder {
    std::vector <ast_choice_case_t*> cases;
};

/* Range case clause holder - for "case >= expr:", "case < expr:", etc. */
struct range_case_clause_holder {
    int selector_kind;  // parser_case_selector_kind enum value
    ast_expr_t* bound_expr;  // The comparison bound expression
};

struct union_case_list_holder {
    std::vector <ast_union_case_t*> cases;
};

/* Statement list holder */
struct statement_list_holder {
    std::vector <ast_statement_t*> statements;
};

/* Struct body list holder */
struct struct_body_list_holder {
    std::vector <datascript::ast::struct_body_item> items;
};

namespace datascript::parser {
    struct enum_item_list_deleter {
        void operator()(enum_item_list_holder* p) const { delete p; }
    };

    struct field_list_deleter {
        void operator()(field_list_holder* p) const { delete p; }
    };

    struct struct_body_list_deleter {
        void operator()(struct_body_list_holder* p) const { delete p; }
    };

    struct expr_list_deleter {
        void operator()(expr_list_holder* p) const { delete p; }
    };

    struct choice_case_list_deleter {
        void operator()(choice_case_list_holder* p) const { delete p; }
    };

    struct range_case_clause_deleter {
        void operator()(range_case_clause_holder* p) const { delete p; }
    };

    struct union_case_list_deleter {
        void operator()(union_case_list_holder* p) const { delete p; }
    };

    struct statement_list_deleter {
        void operator()(statement_list_holder* p) const { delete p; }
    };

    using enum_item_list_guard = std::unique_ptr <enum_item_list_holder, enum_item_list_deleter>;
    using field_list_guard = std::unique_ptr <field_list_holder, field_list_deleter>;
    using struct_body_list_guard = std::unique_ptr <struct_body_list_holder, struct_body_list_deleter>;
    using expr_list_guard = std::unique_ptr <expr_list_holder, expr_list_deleter>;
    using choice_case_list_guard = std::unique_ptr <choice_case_list_holder, choice_case_list_deleter>;
    using range_case_clause_guard = std::unique_ptr <range_case_clause_holder, range_case_clause_deleter>;
    using union_case_list_guard = std::unique_ptr <union_case_list_holder, union_case_list_deleter>;
    using statement_list_guard = std::unique_ptr <statement_list_holder, statement_list_deleter>;
}

/* Type instantiation builder */
ast_type_instantiation_t* parser_build_type_instantiation(parser_context_t* ctx, ast_qualified_name_t* base_type,
                                                          ast_arg_list_t* arguments) {
    /* RAII guard for argument list */
    datascript::parser::arg_list_guard arg_guard(reinterpret_cast <arg_list_holder*>(arguments));

    if (!ctx || !ctx->ast_builder || !base_type) {
        return nullptr;
    }

    try {
        /* Cast base_type back to C++ type */
        auto* qname_ptr = reinterpret_cast <qualified_name*>(base_type);

        /* Extract argument expressions */
        std::vector <expr> arg_exprs;
        if (arg_guard) {
            arg_exprs.reserve(arg_guard->args.size());
            for (auto* expr_ptr : arg_guard->args) {
                auto* expr_cpp_ptr = reinterpret_cast <expr*>(expr_ptr);
                arg_exprs.push_back(std::move(*expr_cpp_ptr));
            }
        }

        /* Create type instantiation in temporary storage */
        ctx->ast_builder->temp_types.emplace_back(type{
            type_instantiation{
                make_pos(ctx),
                std::move(*qname_ptr),
                std::move(arg_exprs)
            }
        });

        /* Return pointer to the type_instantiation variant */
        auto& type_node = ctx->ast_builder->temp_types.back();
        auto* inst_ptr = std::get_if <type_instantiation>(&type_node.node);
        return reinterpret_cast <ast_type_instantiation_t*>(inst_ptr);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build type instantiation: %s", e.what());
        return nullptr;
    }
}

ast_type_t* parser_type_instantiation_to_type(ast_type_instantiation_t* inst) {
    return reinterpret_cast <ast_type_t*>(inst);
}

/* Enum builders */
ast_enum_item_t* parser_build_enum_item(parser_context_t* ctx, token_value_t* name_tok, ast_expr_t* value_expr,
                                        token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !name_tok) {
        return nullptr;
    }

    try {
        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Build optional value expression */
        std::optional <expr> value;
        if (value_expr) {
            auto* expr_ptr = reinterpret_cast <expr*>(value_expr);
            value = std::move(*expr_ptr);
        }

        /* Create enum item and store in temp deque */
        ctx->ast_builder->temp_enum_items.emplace_back(
            enum_item{
                make_pos(ctx),
                name,
                std::move(value),
                process_docstring(docstring)
            }
        );

        /* Return pointer to element in deque */
        return reinterpret_cast <ast_enum_item_t*>(&ctx->ast_builder->temp_enum_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build enum item: %s", e.what());
        return nullptr;
    }
}

ast_enum_item_list_t* parser_build_enum_item_list_single(parser_context_t* ctx, ast_enum_item_t* item) {
    if (!ctx || !item) {
        return nullptr;
    }

    try {
        auto* list = new enum_item_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::enum_item_list_guard guard(list);
        list->items.push_back(item);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_enum_item_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_MEMORY, "Failed to create enum item list: %s", e.what());
        return nullptr;
    }
}

ast_enum_item_list_t* parser_build_enum_item_list_append(parser_context_t* ctx, ast_enum_item_list_t* list,
                                                         ast_enum_item_t* item) {
    if (!ctx || !list || !item) {
        /* Clean up list on error - use guard for exception safety */
        datascript::parser::enum_item_list_guard list_guard(reinterpret_cast <enum_item_list_holder*>(list));
        return nullptr;
    }

    try {
        auto* item_list = reinterpret_cast <enum_item_list_holder*>(list);
        item_list->items.push_back(item);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::enum_item_list_guard list_guard(reinterpret_cast <enum_item_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_MEMORY, "Failed to append enum item: %s", e.what());
        return nullptr;
    }
}

void parser_build_enum(parser_context_t* ctx, int is_bitmask, ast_type_t* base_type, token_value_t* name_tok,
                       ast_enum_item_list_t* items, token_value_t* docstring) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::enum_item_list_guard item_guard(reinterpret_cast <enum_item_list_holder*>(items));

    if (!ctx || !ctx->ast_builder || !base_type || !name_tok || !item_guard) {
        return;
    }

    try {
        /* Extract name from token */
        std::string name = extract_string(name_tok);

        /* Cast base type pointer back to C++ type */
        auto* type_ptr = reinterpret_cast <type*>(base_type);

        /* Extract enum items from list */
        std::vector <enum_item> enum_items;
        enum_items.reserve(item_guard->items.size());

        /* Move items from temp storage */
        for (auto* item_ptr : item_guard->items) {
            auto* enum_item_ptr = reinterpret_cast <enum_item*>(item_ptr);
            enum_items.push_back(std::move(*enum_item_ptr));
        }

        /* Create enum definition */
        enum_def enum_def{
            make_pos(ctx),
            name,
            std::move(*type_ptr),
            std::move(enum_items),
            is_bitmask != 0,
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->enums.push_back(std::move(enum_def));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build enum: %s", e.what());
    }
    // item_guard automatically deletes on scope exit
}

/* Struct builders */
ast_field_def_t* parser_build_field(parser_context_t* ctx, ast_type_t* field_type, token_value_t* name_tok,
                                    token_value_t* docstring) {
    return parser_build_field_with_condition(ctx, field_type, name_tok, nullptr, docstring);
}

/* Label directive builder */
ast_label_directive_t* parser_build_label_directive(parser_context_t* ctx, ast_expr_t* label_expr) {
    if (!ctx || !ctx->ast_builder || !label_expr) {
        return nullptr;
    }

    try {
        auto* expr_ptr = reinterpret_cast <expr*>(label_expr);

        ctx->ast_builder->temp_labels.emplace_back(
            label_directive{
                make_pos(ctx),
                std::move(*expr_ptr)
            }
        );

        return reinterpret_cast <ast_label_directive_t*>(&ctx->ast_builder->temp_labels.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build label directive: %s", e.what());
        return nullptr;
    }
}

/* Alignment directive builder */
ast_alignment_directive_t* parser_build_alignment_directive(parser_context_t* ctx, ast_expr_t* alignment_expr) {
    if (!ctx || !ctx->ast_builder || !alignment_expr) {
        return nullptr;
    }

    try {
        auto* expr_ptr = reinterpret_cast <expr*>(alignment_expr);

        ctx->ast_builder->temp_alignments.emplace_back(
            alignment_directive{
                make_pos(ctx),
                std::move(*expr_ptr)
            }
        );

        return reinterpret_cast <ast_alignment_directive_t*>(&ctx->ast_builder->temp_alignments.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build alignment directive: %s", e.what());
        return nullptr;
    }
}

/* Convert label directive to body item */
ast_struct_body_item_t* parser_label_to_body_item(parser_context_t* ctx, ast_label_directive_t* label) {
    if (!ctx || !ctx->ast_builder || !label) {
        return nullptr;
    }

    try {
        auto* label_ptr = reinterpret_cast <label_directive*>(label);

        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*label_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert label to body item: %s", e.what());
        return nullptr;
    }
}

/* Convert alignment directive to body item */
ast_struct_body_item_t* parser_alignment_to_body_item(parser_context_t* ctx, ast_alignment_directive_t* alignment) {
    if (!ctx || !ctx->ast_builder || !alignment) {
        return nullptr;
    }

    try {
        auto* align_ptr = reinterpret_cast <alignment_directive*>(alignment);

        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*align_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert alignment to body item: %s", e.what());
        return nullptr;
    }
}

/* Convert field to body item */
ast_struct_body_item_t* parser_field_to_body_item(parser_context_t* ctx, ast_field_def_t* field) {
    if (!ctx || !ctx->ast_builder || !field) {
        return nullptr;
    }

    try {
        auto* field_ptr = reinterpret_cast <field_def*>(field);

        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*field_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert field to body item: %s", e.what());
        return nullptr;
    }
}

/* Convert field to body item with docstring */
ast_struct_body_item_t* parser_field_to_body_item_with_docstring(parser_context_t* ctx, ast_field_def_t* field,
                                                                 token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !field) {
        return nullptr;
    }

    try {
        auto* field_ptr = reinterpret_cast <field_def*>(field);

        /* Apply docstring to the field */
        field_ptr->docstring = process_docstring(docstring);

        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*field_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert field to body item with docstring: %s",
                         e.what());
        return nullptr;
    }
}

/* Build inline union field */
ast_inline_union_field_t* parser_build_inline_union_field(parser_context_t* ctx, ast_union_case_list_t* cases,
                                                          token_value_t* name_tok, token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !cases || !name_tok) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        const auto* cases_holder = reinterpret_cast <union_case_list_holder*>(cases);
        std::string field_name(name_tok->start, name_tok->end);

        // Convert pointer list to value vector
        std::vector <union_case> case_defs;
        case_defs.reserve(cases_holder->cases.size());
        for (auto* case_ptr : cases_holder->cases) {
            auto* union_case_ptr = reinterpret_cast <union_case*>(case_ptr);
            case_defs.push_back(std::move(*union_case_ptr));
        }

        // Build inline_union_field with aggregate initialization
        inline_union_field inline_union{
            make_pos(ctx),
            std::move(case_defs),
            std::move(field_name),
            std::nullopt, // condition
            std::nullopt, // constraint
            process_docstring(docstring)
        };

        // Delete the now-empty holder
        delete cases_holder;

        // Store in temp storage and return pointer
        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(inline_union)
        );

        return reinterpret_cast <ast_inline_union_field_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build inline union field: %s", e.what());
        return nullptr;
    }
}

/* Build inline struct field */
ast_inline_struct_field_t* parser_build_inline_struct_field(parser_context_t* ctx, ast_struct_body_list_t* body,
                                                            token_value_t* name_tok, token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !body || !name_tok) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        auto* body_holder = reinterpret_cast <struct_body_list_holder*>(body);
        std::string field_name(name_tok->start, name_tok->end);

        // Create body storage
        auto body_storage = std::make_unique <struct_body_item_storage>();
        body_storage->items = std::move(body_holder->items);

        // Build inline_struct_field with aggregate initialization
        inline_struct_field inline_struct{
            make_pos(ctx),
            std::move(body_storage),
            std::move(field_name),
            std::nullopt, // condition
            std::nullopt, // constraint
            process_docstring(docstring)
        };

        // Delete the now-empty holder
        delete body_holder;

        // Store in temp storage and return pointer
        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(inline_struct)
        );

        return reinterpret_cast <ast_inline_struct_field_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build inline struct field: %s", e.what());
        return nullptr;
    }
}

/* Convert inline union to body item (already in temp storage) */
ast_struct_body_item_t*
parser_inline_union_to_body_item(parser_context_t* ctx, ast_inline_union_field_t* inline_union) {
    if (!ctx || !inline_union) {
        return nullptr;
    }
    // The inline_union pointer already points into temp_body_items, just cast it
    return reinterpret_cast <ast_struct_body_item_t*>(inline_union);
}

/* Convert inline struct to body item (already in temp storage) */
ast_struct_body_item_t* parser_inline_struct_to_body_item(parser_context_t* ctx,
                                                          ast_inline_struct_field_t* inline_struct) {
    if (!ctx || !inline_struct) {
        return nullptr;
    }
    // The inline_struct pointer already points into temp_body_items, just cast it
    return reinterpret_cast <ast_struct_body_item_t*>(inline_struct);
}

/* Build struct body list - single item */
ast_struct_body_list_t* parser_build_struct_body_list_single(parser_context_t* ctx, ast_struct_body_item_t* item) {
    if (!ctx || !item) {
        return nullptr;
    }

    try {
        auto* list = new struct_body_list_holder();
        datascript::parser::struct_body_list_guard guard(list);

        auto* item_ptr = reinterpret_cast <struct_body_item*>(item);
        list->items.push_back(std::move(*item_ptr));

        return reinterpret_cast <ast_struct_body_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build struct body list: %s", e.what());
        return nullptr;
    }
}

/* Build struct body list - append item */
ast_struct_body_list_t* parser_build_struct_body_list_append(parser_context_t* ctx,
                                                             ast_struct_body_list_t* list,
                                                             ast_struct_body_item_t* item) {
    if (!ctx || !list || !item) {
        return nullptr;
    }

    try {
        auto* list_holder = reinterpret_cast <struct_body_list_holder*>(list);
        auto* item_ptr = reinterpret_cast <struct_body_item*>(item);

        list_holder->items.push_back(std::move(*item_ptr));

        return list;
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append to struct body list: %s", e.what());
        return nullptr;
    }
}

ast_field_def_t* parser_build_field_with_condition(parser_context_t* ctx, ast_type_t* field_type,
                                                   token_value_t* name_tok, ast_expr_t* condition,
                                                   token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !field_type || !name_tok) {
        return nullptr;
    }

    try {
        /* Extract field name from token */
        std::string name = extract_string(name_tok);

        /* Cast type pointer back to C++ type */
        auto* type_ptr = reinterpret_cast <type*>(field_type);

        /* Optional condition expression */
        std::optional <expr> cond_opt;
        if (condition) {
            auto* cond_ptr = reinterpret_cast <expr*>(condition);
            cond_opt = std::move(*cond_ptr);
        }

        /* Create field definition */
        ctx->ast_builder->temp_fields.emplace_back(
            field_def{
                make_pos(ctx),
                std::move(*type_ptr),
                name,
                std::move(cond_opt),
                std::nullopt, // No inline constraint
                std::nullopt, // No default value
                process_docstring(docstring)
            }
        );

        /* Return pointer to field in deque */
        return reinterpret_cast <ast_field_def_t*>(&ctx->ast_builder->temp_fields.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field: %s", e.what());
        return nullptr;
    }
}

ast_field_def_t* parser_build_field_with_constraint(parser_context_t* ctx, ast_type_t* field_type,
                                                    token_value_t* name_tok, ast_expr_t* constraint,
                                                    token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !field_type || !name_tok || !constraint) {
        return nullptr;
    }

    try {
        std::string name = extract_string(name_tok);
        auto* type_ptr = reinterpret_cast <type*>(field_type);
        auto* constraint_ptr = reinterpret_cast <expr*>(constraint);

        ctx->ast_builder->temp_fields.emplace_back(
            field_def{
                make_pos(ctx),
                std::move(*type_ptr),
                name,
                std::nullopt, // No if condition
                std::move(*constraint_ptr), // Inline constraint
                std::nullopt, // No default value
                process_docstring(docstring)
            }
        );

        return reinterpret_cast <ast_field_def_t*>(&ctx->ast_builder->temp_fields.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field with constraint: %s", e.what());
        return nullptr;
    }
}

ast_field_def_t* parser_build_field_with_default(parser_context_t* ctx, ast_type_t* field_type,
                                                 token_value_t* name_tok, ast_expr_t* default_value,
                                                 token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !field_type || !name_tok || !default_value) {
        return nullptr;
    }

    try {
        std::string name = extract_string(name_tok);
        auto* type_ptr = reinterpret_cast <type*>(field_type);
        auto* default_ptr = reinterpret_cast <expr*>(default_value);

        ctx->ast_builder->temp_fields.emplace_back(
            field_def{
                make_pos(ctx),
                std::move(*type_ptr),
                name,
                std::nullopt, // No if condition
                std::nullopt, // No inline constraint
                std::move(*default_ptr), // Default value
                process_docstring(docstring)
            }
        );

        return reinterpret_cast <ast_field_def_t*>(&ctx->ast_builder->temp_fields.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field with default: %s", e.what());
        return nullptr;
    }
}

ast_field_def_t* parser_build_field_with_constraint_and_default(parser_context_t* ctx, ast_type_t* field_type,
                                                                token_value_t* name_tok, ast_expr_t* constraint,
                                                                ast_expr_t* default_value, token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !field_type || !name_tok || !constraint || !default_value) {
        return nullptr;
    }

    try {
        std::string name = extract_string(name_tok);
        auto* type_ptr = reinterpret_cast <type*>(field_type);
        auto* constraint_ptr = reinterpret_cast <expr*>(constraint);
        auto* default_ptr = reinterpret_cast <expr*>(default_value);

        ctx->ast_builder->temp_fields.emplace_back(
            field_def{
                make_pos(ctx),
                std::move(*type_ptr),
                name,
                std::nullopt, // No if condition
                std::move(*constraint_ptr), // Inline constraint
                std::move(*default_ptr), // Default value
                process_docstring(docstring)
            }
        );

        return reinterpret_cast <ast_field_def_t*>(&ctx->ast_builder->temp_fields.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field with constraint and default: %s", e.what());
        return nullptr;
    }
}

ast_field_list_t* parser_build_field_list_single(parser_context_t* ctx, ast_field_def_t* field) {
    if (!field) {
        return nullptr;
    }

    try {
        /* Create new list holder with single field */
        auto* list = new field_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::field_list_guard guard(list);
        list->fields.push_back(field);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_field_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build field list: %s", e.what());
        return nullptr;
    }
}

ast_field_list_t*
parser_build_field_list_append(parser_context_t* ctx, ast_field_list_t* list, ast_field_def_t* field) {
    if (!list || !field) {
        /* Clean up list on error - use guard for exception safety */
        datascript::parser::field_list_guard list_guard(reinterpret_cast <field_list_holder*>(list));
        return nullptr;
    }

    try {
        /* Append field to existing list */
        auto* field_list = reinterpret_cast <field_list_holder*>(list);
        field_list->fields.push_back(field);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::field_list_guard list_guard(reinterpret_cast <field_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append field: %s", e.what());
        return nullptr;
    }
}

void parser_build_struct(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params,
                         ast_struct_body_list_t* body, token_value_t* docstring) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::param_list_guard param_guard(reinterpret_cast <param_list_holder*>(params));
    datascript::parser::struct_body_list_guard body_guard(reinterpret_cast <struct_body_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !name_tok) {
        return;
    }

    try {
        /* Extract struct name from token */
        std::string name = extract_string(name_tok);

        /* Extract parameters from list (if any) */
        std::vector <param> param_defs;
        if (param_guard) {
            param_defs.reserve(param_guard->params.size());

            /* Move parameters from temp storage */
            for (auto* param_ptr : param_guard->params) {
                auto* param_def_ptr = reinterpret_cast <param*>(param_ptr);
                param_defs.push_back(std::move(*param_def_ptr));
            }
        }

        /* Extract body items from list (if any) */
        std::vector <struct_body_item> body_items;
        if (body_guard) {
            body_items = std::move(body_guard->items);
        }

        /* Create struct definition */
        struct_def struct_def{
            make_pos(ctx),
            name,
            std::move(param_defs),
            std::move(body_items),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->structs.push_back(std::move(struct_def));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build struct: %s", e.what());
    }
    // Guards automatically delete on scope exit
}

/* Union case builders */
ast_union_case_t* parser_build_union_case_simple(parser_context_t* ctx, ast_field_def_t* field) {
    if (!ctx || !ctx->ast_builder || !field) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Cast field pointer back to C++ type */
        auto* field_ptr = reinterpret_cast <field_def*>(field);

        /* Capture values we need before moving */
        auto pos = field_ptr->pos;
        auto name = field_ptr->name;
        auto docstring = field_ptr->docstring;

        /* Extract constraint from field to use as union case condition */
        std::optional <expr> condition = std::move(field_ptr->constraint);

        /* Clear the constraint from the field since it's now the union case condition */
        field_ptr->constraint = std::nullopt;

        /* Create vector with moved field as struct_body_item */
        std::vector <struct_body_item> items;
        items.emplace_back(std::move(*field_ptr));

        /* Create union case with single field */
        ctx->ast_builder->temp_union_cases.emplace_back(
            union_case{
                pos,
                name, // Use field name as case name
                std::move(items), // Single item (field)
                std::move(condition), // Moved from field constraint
                false, // Not an anonymous block
                docstring
            }
        );

        /* Return pointer to the newly created case */
        return reinterpret_cast <ast_union_case_t*>(&ctx->ast_builder->temp_union_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build simple union case: %s", e.what());
        return nullptr;
    }
}

ast_union_case_t* parser_build_union_case_anonymous(parser_context_t* ctx, ast_struct_body_list_t* body,
                                                    token_value_t* name_tok, ast_expr_t* condition,
                                                    token_value_t* docstring) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::struct_body_list_guard body_guard(reinterpret_cast <struct_body_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !name_tok) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Extract case name from token */
        std::string name = extract_string(name_tok);

        /* Extract body items from list */
        std::vector <struct_body_item> items;
        if (body_guard) {
            items = std::move(body_guard->items);
        }

        /* Build optional condition expression */
        std::optional <expr> cond;
        if (condition) {
            auto* cond_ptr = reinterpret_cast <expr*>(condition);
            cond = std::move(*cond_ptr);
        }

        /* Create union case */
        ctx->ast_builder->temp_union_cases.emplace_back(
            union_case{
                make_pos(ctx),
                name,
                std::move(items),
                std::move(cond),
                true, // Is an anonymous block
                process_docstring(docstring)
            }
        );

        /* Return pointer to the newly created case */
        return reinterpret_cast <ast_union_case_t*>(&ctx->ast_builder->temp_union_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build anonymous union case: %s", e.what());
        return nullptr;
    }
    // body_guard automatically deletes on scope exit
}

ast_union_case_list_t* parser_build_union_case_list_single(parser_context_t* ctx, ast_union_case_t* union_case) {
    if (!ctx || !union_case) {
        return nullptr;
    }

    try {
        auto* list = new union_case_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::union_case_list_guard guard(list);

        list->cases.push_back(union_case);

        /* Release ownership - caller now responsible */
        return reinterpret_cast <ast_union_case_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build union case list: %s", e.what());
        return nullptr;
    }
}

ast_union_case_list_t* parser_build_union_case_list_append(parser_context_t* ctx, ast_union_case_list_t* list,
                                                           ast_union_case_t* union_case) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::union_case_list_guard guard(reinterpret_cast <union_case_list_holder*>(list));

    if (!ctx || !guard || !union_case) {
        return nullptr;
    }

    try {
        guard->cases.push_back(union_case);

        /* Release ownership - caller now responsible */
        return reinterpret_cast <ast_union_case_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append to union case list: %s", e.what());
        return nullptr;
    }
}

void parser_build_union(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params,
                        ast_union_case_list_t* cases, token_value_t* docstring) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::param_list_guard param_guard(reinterpret_cast <param_list_holder*>(params));
    datascript::parser::union_case_list_guard case_guard(reinterpret_cast <union_case_list_holder*>(cases));

    if (!ctx || !ctx->ast_builder || !name_tok) {
        return;
    }

    try {
        /* Extract union name from token */
        std::string name = extract_string(name_tok);

        /* Extract parameters from list (if any) */
        std::vector <param> param_defs;
        if (param_guard) {
            param_defs.reserve(param_guard->params.size());

            /* Move parameters from temp storage */
            for (auto* param_ptr : param_guard->params) {
                auto* param_def_ptr = reinterpret_cast <param*>(param_ptr);
                param_defs.push_back(std::move(*param_def_ptr));
            }
        }

        /* Extract cases from list (if any) */
        std::vector <union_case> case_defs;
        if (case_guard) {
            case_defs.reserve(case_guard->cases.size());

            /* Move cases from temp storage */
            for (auto* case_ptr : case_guard->cases) {
                auto* union_case_ptr = reinterpret_cast <union_case*>(case_ptr);
                case_defs.push_back(std::move(*union_case_ptr));
            }
        }

        /* Create union definition */
        union_def union_def{
            make_pos(ctx),
            name,
            std::move(param_defs),
            std::move(case_defs),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->unions.push_back(std::move(union_def));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build union: %s", e.what());
    }
    // case_guard automatically deletes on scope exit
}

/* Choice builders */
ast_expr_list_t* parser_build_expr_list_single(parser_context_t* ctx, ast_expr_t* expr) {
    if (!ctx || !expr) {
        return nullptr;
    }

    try {
        auto* list = new expr_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::expr_list_guard guard(list);
        list->exprs.push_back(expr);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_expr_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build expression list: %s", e.what());
        return nullptr;
    }
}

ast_expr_list_t* parser_build_expr_list_append(parser_context_t* ctx, ast_expr_list_t* list, ast_expr_t* expr) {
    if (!ctx || !list || !expr) {
        /* RAII guard ensures cleanup on error */
        datascript::parser::expr_list_guard list_guard(reinterpret_cast <expr_list_holder*>(list));
        return nullptr;
    }

    try {
        auto* holder = reinterpret_cast <expr_list_holder*>(list);
        holder->exprs.push_back(expr);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::expr_list_guard list_guard(reinterpret_cast <expr_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append expression to list: %s", e.what());
        return nullptr;
    }
}

ast_choice_case_t*
parser_build_choice_case(parser_context_t* ctx, ast_expr_list_t* case_exprs, ast_field_def_t* field) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::expr_list_guard expr_guard(reinterpret_cast <expr_list_holder*>(case_exprs));

    if (!ctx || !ctx->ast_builder || !expr_guard || !field) {
        return nullptr;
    }

    try {
        /* Extract expressions from list */
        std::vector <expr> exprs;
        exprs.reserve(expr_guard->exprs.size());

        for (auto* expr_ptr : expr_guard->exprs) {
            auto* e = reinterpret_cast <expr*>(expr_ptr);
            exprs.push_back(std::move(*e));
        }

        /* Get field from temp storage */
        auto* field_def_ptr = reinterpret_cast <field_def*>(field);

        /* Extract field name and wrap field in items vector */
        std::string fname = field_def_ptr->name;
        std::vector<struct_body_item> items;
        items.push_back(std::move(*field_def_ptr));

        /* Create choice case in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            std::move(exprs),
            case_selector_kind::exact,  // exact match (default)
            std::nullopt,  // no range bound
            false, // not default
            std::move(items),
            std::move(fname),
            false // not anonymous block
        });

        return reinterpret_cast <ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build choice case: %s", e.what());
        return nullptr;
    }
    // expr_guard automatically deletes on scope exit
}

ast_choice_case_t* parser_build_choice_default(parser_context_t* ctx, ast_field_def_t* field) {
    if (!ctx || !ctx->ast_builder || !field) {
        return nullptr;
    }

    try {
        /* Get field from temp storage */
        auto* field_def_ptr = reinterpret_cast <field_def*>(field);

        /* Extract field name and wrap field in items vector */
        std::string fname = field_def_ptr->name;
        std::vector<struct_body_item> items;
        items.push_back(std::move(*field_def_ptr));

        /* Create default choice case in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {}, // empty case_exprs
            case_selector_kind::exact,  // not used for default, but keep consistent
            std::nullopt,  // no range bound
            true, // is default
            std::move(items),
            std::move(fname),
            false // not anonymous block
        });

        return reinterpret_cast <ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build default choice case: %s", e.what());
        return nullptr;
    }
}

ast_choice_case_t* parser_build_choice_case_inline(parser_context_t* ctx, ast_expr_list_t* case_exprs,
                                                    ast_struct_body_list_t* body, token_value_t* name) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::expr_list_guard expr_guard(reinterpret_cast<expr_list_holder*>(case_exprs));
    datascript::parser::struct_body_list_guard body_guard(reinterpret_cast<struct_body_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !expr_guard || !name) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Extract expressions from list */
        std::vector<expr> exprs;
        exprs.reserve(expr_guard->exprs.size());

        for (auto* expr_ptr : expr_guard->exprs) {
            auto* e = reinterpret_cast<expr*>(expr_ptr);
            exprs.push_back(std::move(*e));
        }

        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Extract body items from list */
        std::vector<struct_body_item> items;
        if (body_guard) {
            items = std::move(body_guard->items);
        }

        /* Create choice case with inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            std::move(exprs),
            case_selector_kind::exact,  // exact match (default)
            std::nullopt,  // no range bound
            false, // not default
            std::move(items),
            std::move(fname),
            true // anonymous block
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build inline choice case: %s", e.what());
        return nullptr;
    }
    // Guards automatically delete on scope exit
}

ast_choice_case_t* parser_build_choice_default_inline(parser_context_t* ctx, ast_struct_body_list_t* body,
                                                       token_value_t* name) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::struct_body_list_guard body_guard(reinterpret_cast<struct_body_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !name) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Extract body items from list */
        std::vector<struct_body_item> items;
        if (body_guard) {
            items = std::move(body_guard->items);
        }

        /* Create default choice case with inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {}, // empty case_exprs
            case_selector_kind::exact,  // not used for default, but keep consistent
            std::nullopt,  // no range bound
            true, // is default
            std::move(items),
            std::move(fname),
            true // anonymous block
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build inline default choice case: %s", e.what());
        return nullptr;
    }
    // Guard automatically deletes on scope exit
}

ast_choice_case_t* parser_build_choice_case_inline_empty(parser_context_t* ctx, ast_expr_list_t* case_exprs,
                                                          token_value_t* name) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::expr_list_guard expr_guard(reinterpret_cast<expr_list_holder*>(case_exprs));

    if (!ctx || !ctx->ast_builder || !expr_guard || !name) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Extract expressions from list */
        std::vector<expr> exprs;
        exprs.reserve(expr_guard->exprs.size());

        for (auto* expr_ptr : expr_guard->exprs) {
            auto* e = reinterpret_cast<expr*>(expr_ptr);
            exprs.push_back(std::move(*e));
        }

        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Create choice case with empty inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            std::move(exprs),
            case_selector_kind::exact,  // exact match (default)
            std::nullopt,  // no range bound
            false, // not default
            {}, // empty items
            std::move(fname),
            true // anonymous block (even though empty)
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build empty inline choice case: %s", e.what());
        return nullptr;
    }
    // Guard automatically deletes on scope exit
}

ast_choice_case_t* parser_build_choice_default_inline_empty(parser_context_t* ctx, token_value_t* name) {
    if (!ctx || !ctx->ast_builder || !name) {
        return nullptr;
    }

    try {
        using namespace datascript::ast;

        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Create default choice case with empty inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {}, // empty case_exprs
            case_selector_kind::exact,  // not used for default, but keep consistent
            std::nullopt,  // no range bound
            true, // is default
            {}, // empty items
            std::move(fname),
            true // anonymous block (even though empty)
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build empty inline default choice case: %s", e.what());
        return nullptr;
    }
}

ast_choice_case_list_t* parser_build_choice_case_list_single(parser_context_t* ctx, ast_choice_case_t* choice_case) {
    if (!ctx || !choice_case) {
        return nullptr;
    }

    try {
        auto* list = new choice_case_list_holder();
        /* RAII guard for exception safety - ensures cleanup if push_back throws */
        datascript::parser::choice_case_list_guard guard(list);
        list->cases.push_back(choice_case);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_choice_case_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build choice case list: %s", e.what());
        return nullptr;
    }
}

ast_choice_case_list_t* parser_build_choice_case_list_append(parser_context_t* ctx, ast_choice_case_list_t* list,
                                                             ast_choice_case_t* choice_case) {
    if (!ctx || !list || !choice_case) {
        /* RAII guard ensures cleanup on error */
        datascript::parser::choice_case_list_guard list_guard(reinterpret_cast <choice_case_list_holder*>(list));
        return nullptr;
    }

    try {
        auto* holder = reinterpret_cast <choice_case_list_holder*>(list);
        holder->cases.push_back(choice_case);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::choice_case_list_guard list_guard(reinterpret_cast <choice_case_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append choice case to list: %s", e.what());
        return nullptr;
    }
}

void parser_build_choice(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params, ast_expr_t* selector,
                         ast_choice_case_list_t* cases, token_value_t* docstring) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::param_list_guard param_guard(reinterpret_cast <param_list_holder*>(params));
    datascript::parser::choice_case_list_guard case_guard(reinterpret_cast <choice_case_list_holder*>(cases));

    if (!ctx || !ctx->ast_builder || !name_tok || !case_guard) {
        return;
    }

    try {
        /* Extract choice name from token */
        std::string name = extract_string(name_tok);

        /* Extract parameters from list (if any) */
        std::vector <param> param_defs;
        if (param_guard) {
            param_defs.reserve(param_guard->params.size());

            /* Move parameters from temp storage */
            for (auto* param_ptr : param_guard->params) {
                auto* param_def_ptr = reinterpret_cast <param*>(param_ptr);
                param_defs.push_back(std::move(*param_def_ptr));
            }
        }

        /* Get selector expression (optional for inline discriminator) */
        std::optional<expr> selector_expr;
        if (selector) {
            auto* expr_ptr = reinterpret_cast <expr*>(selector);
            selector_expr = std::move(*expr_ptr);
        }

        /* Extract cases from list */
        std::vector <choice_case> case_defs;
        case_defs.reserve(case_guard->cases.size());

        /* Move cases from temp storage */
        for (auto* case_ptr : case_guard->cases) {
            auto* choice_case_ptr = reinterpret_cast <choice_case*>(case_ptr);
            case_defs.push_back(std::move(*choice_case_ptr));
        }

        /* Create choice definition (external discriminator) */
        choice_def choice_def{
            make_pos(ctx),
            name,
            std::move(param_defs),
            std::move(selector_expr),
            std::nullopt,  // No inline discriminator type for external discriminator
            std::move(case_defs),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->choices.push_back(std::move(choice_def));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build choice: %s", e.what());
    }
    // Guards automatically delete on scope exit
}

/* Build a choice with inline discriminator (explicit type required) */
void parser_build_choice_inline(parser_context_t* ctx, token_value_t* name_tok, ast_param_list_t* params, ast_type_t* discriminator_type,
                                 ast_choice_case_list_t* cases, token_value_t* docstring) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::param_list_guard param_guard(reinterpret_cast<param_list_holder*>(params));
    datascript::parser::choice_case_list_guard case_guard(reinterpret_cast<choice_case_list_holder*>(cases));

    if (!ctx || !ctx->ast_builder || !name_tok || !discriminator_type || !case_guard) {
        return;
    }

    try {
        /* Extract choice name from token */
        std::string name = extract_string(name_tok);

        /* Extract parameters from list (if any) */
        std::vector<param> param_defs;
        if (param_guard) {
            param_defs.reserve(param_guard->params.size());

            /* Move parameters from temp storage */
            for (auto* param_ptr : param_guard->params) {
                auto* param_def_ptr = reinterpret_cast<param*>(param_ptr);
                param_defs.push_back(std::move(*param_def_ptr));
            }
        }

        /* Extract inline discriminator type */
        auto* type_ptr = reinterpret_cast<type*>(discriminator_type);
        type inline_disc_type = std::move(*type_ptr);

        /* Extract cases from list */
        std::vector<choice_case> case_defs;
        case_defs.reserve(case_guard->cases.size());

        /* Move cases from temp storage */
        for (auto* case_ptr : case_guard->cases) {
            auto* choice_case_ptr = reinterpret_cast<choice_case*>(case_ptr);
            case_defs.push_back(std::move(*choice_case_ptr));
        }

        /* Create choice definition (inline discriminator) */
        choice_def choice_def{
            make_pos(ctx),
            name,
            std::move(param_defs),
            std::nullopt,  // No selector expression for inline discriminator
            std::move(inline_disc_type),  // Explicit inline discriminator type
            std::move(case_defs),
            process_docstring(docstring)
        };

        /* Add to module */
        ctx->ast_builder->module->choices.push_back(std::move(choice_def));
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build inline choice: %s", e.what());
    }
    // Guards automatically delete on scope exit
}

/* ============================================================================ */
/* Range-Based Case Clause Builders */
/* ============================================================================ */

/* Helper to convert parser_case_selector_kind to ast::case_selector_kind */
static case_selector_kind convert_selector_kind(int parser_kind) {
    switch (parser_kind) {
        case PARSER_CASE_EXACT: return case_selector_kind::exact;
        case PARSER_CASE_GE:    return case_selector_kind::range_ge;
        case PARSER_CASE_GT:    return case_selector_kind::range_gt;
        case PARSER_CASE_LE:    return case_selector_kind::range_le;
        case PARSER_CASE_LT:    return case_selector_kind::range_lt;
        case PARSER_CASE_NE:    return case_selector_kind::range_ne;
        default:                return case_selector_kind::exact;
    }
}

/* Build a range case clause (e.g., "case >= 0x80:") */
ast_range_case_clause_t* parser_build_range_case_clause(parser_context_t* ctx, int selector_kind, ast_expr_t* bound_expr) {
    if (!ctx || !bound_expr) {
        return nullptr;
    }

    try {
        auto* holder = new range_case_clause_holder();
        holder->selector_kind = selector_kind;
        holder->bound_expr = bound_expr;
        return reinterpret_cast<ast_range_case_clause_t*>(holder);
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build range case clause: %s", e.what());
        return nullptr;
    }
}

/* Destroy a range case clause holder */
void parser_destroy_range_case_clause(ast_range_case_clause_t* clause) {
    delete reinterpret_cast<range_case_clause_holder*>(clause);
}

/* Build a choice case with range selector and field definition */
ast_choice_case_t* parser_build_choice_case_range(parser_context_t* ctx, ast_range_case_clause_t* range_clause, ast_field_def_t* field) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::range_case_clause_guard range_guard(reinterpret_cast<range_case_clause_holder*>(range_clause));

    if (!ctx || !ctx->ast_builder || !range_guard || !field) {
        return nullptr;
    }

    try {
        /* Get field from temp storage */
        auto* field_def_ptr = reinterpret_cast<field_def*>(field);

        /* Extract field name and wrap field in items vector */
        std::string fname = field_def_ptr->name;
        std::vector<struct_body_item> items;
        items.push_back(std::move(*field_def_ptr));

        /* Extract bound expression */
        auto* bound_expr_ptr = reinterpret_cast<expr*>(range_guard->bound_expr);
        expr bound_expr_copy = std::move(*bound_expr_ptr);

        /* Create choice case in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {},  // empty case_exprs (range uses range_bound instead)
            convert_selector_kind(range_guard->selector_kind),
            std::move(bound_expr_copy),  // range bound expression
            false, // not default
            std::move(items),
            std::move(fname),
            false // not anonymous block
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build range choice case: %s", e.what());
        return nullptr;
    }
    // range_guard automatically deletes on scope exit
}

/* Build a choice case with range selector and inline struct block */
ast_choice_case_t* parser_build_choice_case_range_inline(parser_context_t* ctx, ast_range_case_clause_t* range_clause,
                                                          ast_struct_body_list_t* body, token_value_t* name) {
    // RAII guards ensure automatic cleanup on all paths
    datascript::parser::range_case_clause_guard range_guard(reinterpret_cast<range_case_clause_holder*>(range_clause));
    datascript::parser::struct_body_list_guard body_guard(reinterpret_cast<struct_body_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !range_guard || !name) {
        return nullptr;
    }

    try {
        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Extract body items from list */
        std::vector<struct_body_item> items;
        if (body_guard) {
            items = std::move(body_guard->items);
        }

        /* Extract bound expression */
        auto* bound_expr_ptr = reinterpret_cast<expr*>(range_guard->bound_expr);
        expr bound_expr_copy = std::move(*bound_expr_ptr);

        /* Create choice case with inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {},  // empty case_exprs (range uses range_bound instead)
            convert_selector_kind(range_guard->selector_kind),
            std::move(bound_expr_copy),  // range bound expression
            false, // not default
            std::move(items),
            std::move(fname),
            true // anonymous block
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build range inline choice case: %s", e.what());
        return nullptr;
    }
    // Guards automatically delete on scope exit
}

/* Build a choice case with range selector and empty inline block */
ast_choice_case_t* parser_build_choice_case_range_inline_empty(parser_context_t* ctx, ast_range_case_clause_t* range_clause,
                                                                token_value_t* name) {
    // RAII guard ensures automatic cleanup on all paths
    datascript::parser::range_case_clause_guard range_guard(reinterpret_cast<range_case_clause_holder*>(range_clause));

    if (!ctx || !ctx->ast_builder || !range_guard || !name) {
        return nullptr;
    }

    try {
        /* Extract field name from token */
        std::string fname = extract_string(name);

        /* Extract bound expression */
        auto* bound_expr_ptr = reinterpret_cast<expr*>(range_guard->bound_expr);
        expr bound_expr_copy = std::move(*bound_expr_ptr);

        /* Create choice case with empty inline block in temp storage */
        ctx->ast_builder->temp_choice_cases.emplace_back(choice_case{
            make_pos(ctx),
            {},  // empty case_exprs (range uses range_bound instead)
            convert_selector_kind(range_guard->selector_kind),
            std::move(bound_expr_copy),  // range bound expression
            false, // not default
            {}, // empty items
            std::move(fname),
            true // anonymous block (even though empty)
        });

        return reinterpret_cast<ast_choice_case_t*>(&ctx->ast_builder->temp_choice_cases.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build empty range inline choice case: %s", e.what());
        return nullptr;
    }
    // range_guard automatically deletes on scope exit
}

/* ============================================================================ */
/* Function and Statement Builders */
/* ============================================================================ */

/* Build a return statement */
ast_statement_t* parser_build_return_statement(parser_context_t* ctx, ast_expr_t* value) {
    if (!ctx || !ctx->ast_builder || !value) {
        return nullptr;
    }

    try {
        auto* value_expr = reinterpret_cast <expr*>(value);

        /* Create return statement and store in temp storage */
        ctx->ast_builder->temp_statements.emplace_back(
            return_statement{
                make_pos(ctx),
                std::move(*value_expr)
            }
        );

        return reinterpret_cast <ast_statement_t*>(&ctx->ast_builder->temp_statements.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build return statement: %s", e.what());
        return nullptr;
    }
}

/* Build an expression statement */
ast_statement_t* parser_build_expression_statement(parser_context_t* ctx, ast_expr_t* expression) {
    if (!ctx || !ctx->ast_builder || !expression) {
        return nullptr;
    }

    try {
        auto* expr_ptr = reinterpret_cast <expr*>(expression);

        /* Create expression statement and store in temp storage */
        ctx->ast_builder->temp_statements.emplace_back(
            expression_statement{
                make_pos(ctx),
                std::move(*expr_ptr)
            }
        );

        return reinterpret_cast <ast_statement_t*>(&ctx->ast_builder->temp_statements.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build expression statement: %s", e.what());
        return nullptr;
    }
}

/* Build a statement list with a single statement */
ast_statement_list_t* parser_build_statement_list_single(parser_context_t* ctx, ast_statement_t* stmt) {
    if (!ctx || !stmt) {
        return nullptr;
    }

    try {
        auto* list = new statement_list_holder();
        /* RAII guard for exception safety */
        datascript::parser::statement_list_guard guard(list);
        list->statements.push_back(stmt);
        /* Release ownership to caller on success */
        return reinterpret_cast <ast_statement_list_t*>(guard.release());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build statement list: %s", e.what());
        return nullptr;
    }
}

/* Append a statement to a statement list */
ast_statement_list_t* parser_build_statement_list_append(parser_context_t* ctx, ast_statement_list_t* list,
                                                         ast_statement_t* stmt) {
    if (!ctx || !list || !stmt) {
        /* RAII guard ensures cleanup on error */
        datascript::parser::statement_list_guard list_guard(reinterpret_cast <statement_list_holder*>(list));
        return nullptr;
    }

    try {
        auto* holder = reinterpret_cast <statement_list_holder*>(list);
        holder->statements.push_back(stmt);
        return list;
    } catch (const std::exception& e) {
        /* RAII guard ensures cleanup on exception */
        datascript::parser::statement_list_guard list_guard(reinterpret_cast <statement_list_holder*>(list));
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to append statement to list: %s", e.what());
        return nullptr;
    }
}

/* Build a function definition */
ast_function_def_t* parser_build_function(parser_context_t* ctx, ast_type_t* return_type, token_value_t* name_tok,
                                          ast_param_list_t* params, ast_statement_list_t* body,
                                          token_value_t* docstring) {
    // RAII guards for automatic cleanup
    datascript::parser::param_list_guard param_guard(reinterpret_cast <param_list_holder*>(params));
    datascript::parser::statement_list_guard stmt_guard(reinterpret_cast <statement_list_holder*>(body));

    if (!ctx || !ctx->ast_builder || !return_type || !name_tok || !stmt_guard) {
        return nullptr;
    }

    try {
        /* Extract function name */
        std::string name = extract_string(name_tok);

        /* Get return type */
        auto* ret_type = reinterpret_cast <type*>(return_type);

        /* Extract parameters (optional) */
        std::vector <param> param_defs;
        if (param_guard) {
            param_defs.reserve(param_guard->params.size());
            for (auto* param_ptr : param_guard->params) {
                auto* p = reinterpret_cast <param*>(param_ptr);
                param_defs.push_back(std::move(*p));
            }
        }

        /* Extract statements from list */
        std::vector <statement> statements;
        statements.reserve(stmt_guard->statements.size());
        for (auto* stmt_ptr : stmt_guard->statements) {
            auto* s = reinterpret_cast <statement*>(stmt_ptr);
            statements.push_back(std::move(*s));
        }

        /* Create function definition and store in temp storage */
        ctx->ast_builder->temp_functions.emplace_back(
            function_def{
                make_pos(ctx),
                name,
                std::move(*ret_type),
                std::move(param_defs),
                std::move(statements),
                process_docstring(docstring)
            }
        );

        return reinterpret_cast <ast_function_def_t*>(&ctx->ast_builder->temp_functions.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to build function: %s", e.what());
        return nullptr;
    }
    // Guards automatically delete on scope exit
}

/* Convert function to struct body item (without docstring) */
ast_struct_body_item_t* parser_function_to_body_item(parser_context_t* ctx, ast_function_def_t* func) {
    if (!ctx || !ctx->ast_builder || !func) {
        return nullptr;
    }

    try {
        auto* func_ptr = reinterpret_cast <function_def*>(func);

        /* Create struct_body_item variant and store in temp storage */
        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*func_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert function to body item: %s", e.what());
        return nullptr;
    }
}

/* Convert function to struct body item (with docstring) */
ast_struct_body_item_t* parser_function_to_body_item_with_docstring(parser_context_t* ctx, ast_function_def_t* func,
                                                                    token_value_t* docstring) {
    if (!ctx || !ctx->ast_builder || !func) {
        return nullptr;
    }

    try {
        auto* func_ptr = reinterpret_cast <function_def*>(func);

        /* Apply docstring to function */
        func_ptr->docstring = process_docstring(docstring);

        /* Create struct_body_item variant and store in temp storage */
        ctx->ast_builder->temp_body_items.emplace_back(
            std::move(*func_ptr)
        );

        return reinterpret_cast <ast_struct_body_item_t*>(&ctx->ast_builder->temp_body_items.back());
    } catch (const std::exception& e) {
        parser_set_error(ctx, PARSER_ERROR_SEMANTIC, "Failed to convert function to body item with docstring: %s",
                         e.what());
        return nullptr;
    }
}

/* C-callable destructors for list holders - used by Lemon %destructor directives */
/* Defined at end of file after all holder types are complete */
extern "C" {
void parser_destroy_arg_list(ast_arg_list_t* list) {
    delete reinterpret_cast <arg_list_holder*>(list);
}

void parser_destroy_param_list(ast_param_list_t* list) {
    delete reinterpret_cast <param_list_holder*>(list);
}

void parser_destroy_enum_item_list(ast_enum_item_list_t* list) {
    delete reinterpret_cast <enum_item_list_holder*>(list);
}

void parser_destroy_field_list(ast_field_list_t* list) {
    delete reinterpret_cast <field_list_holder*>(list);
}

void parser_destroy_expr_list(ast_expr_list_t* list) {
    delete reinterpret_cast <expr_list_holder*>(list);
}

void parser_destroy_choice_case_list(ast_choice_case_list_t* list) {
    delete reinterpret_cast <choice_case_list_holder*>(list);
}

void parser_destroy_union_case_list(ast_union_case_list_t* list) {
    delete reinterpret_cast <union_case_list_holder*>(list);
}

void parser_destroy_statement_list(ast_statement_list_t* list) {
    delete reinterpret_cast <statement_list_holder*>(list);
}

void parser_destroy_inline_union_field(ast_inline_union_field_t* field) {
    // Inline fields are stored in temp_body_items and cleaned up automatically
    // Nothing to do here - the parser context cleanup handles it
    (void)field;
}

void parser_destroy_inline_struct_field(ast_inline_struct_field_t* field) {
    // Inline fields are stored in temp_body_items and cleaned up automatically
    // Nothing to do here - the parser context cleanup handles it
    (void)field;
}
}

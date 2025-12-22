/*
 * DataScript Parser - C++ Interface
 */

#include <fstream>
#include <sstream>
#include <stdexcept>
#include <array>
#include <cstring>

#include <datascript/parser.hh>
#include <datascript/ast.hh>
#include <datascript/parser_error.hh>

#define DATASCRIPT_PARSER_CONTEXT_VISIBLE
#include "parser/ast_builder.h"
#include "parser/ast_holder.hh"
#include "parser/scanner_context.h"
#include "parser/parser_context.h"

/* Forward declarations for Lemon */
extern "C" {
void* ParseAlloc(void* (* allocProc)(size_t));
void Parse(void* parser, int token, token_value_t* value, parser_context_t* ctx);
void ParseFree(void* parser, void (* freeProc)(void*));
void ParseTrace(FILE* TraceFILE, char* zTracePrompt);
int parser_scan_token(scanner_context_t* ctx, parser_context_t* pctx, token_value_t* token);
}

namespace {
    class scanner {
        public:
            scanner(const char* input, size_t length, const char* filename) {
                m_ctx = new scanner_context_t;
                constexpr size_t PADDING = 32;
                char* padded_input = new char[length + PADDING];

                memcpy(padded_input, input, length);
                memset(padded_input + length, 0, PADDING); /* Fill padding with nulls */

                m_ctx->input = padded_input;
                m_ctx->cursor = padded_input;
                m_ctx->eof = padded_input + length; /* End of actual data */
                m_ctx->limit = padded_input + length + PADDING; /* End of buffer with padding */
                m_ctx->marker = padded_input;
                m_ctx->line = 1;
                m_ctx->column = 1;
                m_ctx->filename = filename ? filename : "<input>";
            }

            ~scanner() {
                delete [] m_ctx->input;
                delete m_ctx;
            }

            int operator ()(token_value_t* token, parser_context_t* pctx) {
                return parser_scan_token(m_ctx, pctx, token);
            }

            scanner_context_t* get() {
                return m_ctx;
            }

        private:
            scanner_context_t* m_ctx;
    };

    class tokens_buffer {
        public:
            static constexpr int PARSER_TOKEN_POOL_SIZE = 256;
            static constexpr int PARSER_MAX_STACK_DEPTH = 64;

        public:
            tokens_buffer()
                : token_pool_index(0),
                  token_pool_start(0),
                  token_pool_max_used(0) {
            }

            std::pair <token_value_t*, int> get() {
                /* Calculate active window size */
                int active_tokens;
                if (token_pool_index >= token_pool_start) {
                    active_tokens = token_pool_index - token_pool_start;
                } else {
                    /* Wrapped around */
                    active_tokens = (PARSER_TOKEN_POOL_SIZE - token_pool_start) + token_pool_index;
                }

                /* Check if pool is full */
                if (active_tokens >= PARSER_TOKEN_POOL_SIZE - 1) {
                    /* Pool full - reclaim space conservatively */
                    /* Assume tokens older than MAX_STACK_DEPTH are consumed */
                    int reclaim_count = active_tokens - PARSER_MAX_STACK_DEPTH;
                    if (reclaim_count <= 0) {
                        return {nullptr, active_tokens};
                    }

                    /* Advance start index to reclaim space */
                    token_pool_start = (token_pool_start + reclaim_count) % PARSER_TOKEN_POOL_SIZE;
                }

                /* Allocate from token pool with wraparound */
                auto* token = &token_pool[static_cast<size_t>(token_pool_index)];
                token_pool_index = (token_pool_index + 1) % PARSER_TOKEN_POOL_SIZE;

                /* Track high water mark */
                if (active_tokens > token_pool_max_used) {
                    token_pool_max_used = active_tokens;
                }
                return {token, active_tokens};
            }

            [[nodiscard]] int get_max_used() const {
                return token_pool_max_used;
            }

        private:
            std::array <token_value_t, PARSER_TOKEN_POOL_SIZE> token_pool{};
            int token_pool_index;
            int token_pool_start; /* Start of active window */
            int token_pool_max_used; /* High water mark for tuning */
    };

    class parser {
        public:
            parser()
                : m_parser(ParseAlloc(malloc)) {
            }

            ~parser() {
                ParseFree(m_parser, free);
            }

            parser(const parser&) = delete;
            parser& operator =(const parser&) = delete;

            void operator()(int token_type, token_value_t* token, parser_context_t* ctx) {
                Parse(m_parser, token_type, token, ctx);
            }

        private:
            void* m_parser;
    };
}

/* Run parser */
int parser_parse(parser& the_parser, parser_context_t& ctx, scanner& lexer) {
    tokens_buffer tokens;
    try {
        /* Parse tokens until EOF */
        while (true) {
            auto [token, active_tokens] = tokens.get();
            if (!token) {
                parser_set_error(&ctx, PARSER_ERROR_MEMORY,
                                 "Token pool exhausted (size=%d, active=%d, max_stack=%d). "
                                 "Consider increasing PARSER_TOKEN_POOL_SIZE or PARSER_MAX_STACK_DEPTH",
                                 tokens_buffer::PARSER_TOKEN_POOL_SIZE, active_tokens,
                                 tokens_buffer::PARSER_MAX_STACK_DEPTH);
                return -1;
            }
            ctx.token_pool_max_used = tokens.get_max_used();

            int token_type = lexer(token, &ctx);

            if (token_type == 0) {
                /* EOF */
                break;
            }

            if (token_type < 0) {
                /* Scanner error */
                return -1;
            }
            the_parser(token_type, token, &ctx);

            /* Check for errors */
            if (ctx.error.code != PARSER_OK) {
                return -1;
            }
        }

        /* Send EOF to parser (token 0 in Lemon) */
        the_parser(0, nullptr, &ctx);

        return ctx.error.code == PARSER_OK ? 0 : -1;
    } catch (const std::exception& e) {
        parser_set_error(&ctx, PARSER_ERROR_INTERNAL, "C++ exception: %s", e.what());
        return -1;
    } catch (...) {
        parser_set_error(&ctx, PARSER_ERROR_INTERNAL, "Unknown C++ exception");
        return -1;
    }
}

namespace datascript {
    /* Implementation details */
    namespace {
        ast::module parse_datascript_impl(const std::string& input, const std::string& filename) {
            /* Create parser context */
            parser the_parser;

            scanner lexer(input.c_str(), input.length(), filename.c_str());
            parser_context ctx{};
            ctx.m_scanner = lexer.get();
            ast_module_holder holder;
            holder.module = new ast::module(); // Holder manages module lifetime
            ctx.ast_builder = &holder;
            ctx.error = {};
            ctx.token_pool_max_used = 0;

            /* Parse the input */
            int result = parser_parse(the_parser, ctx, lexer);

            if (result != 0) {
                /* Extract error information */
                std::string error_msg = ctx.error.message;
                int line = ctx.error.line;
                int column = ctx.error.column;

                /* Throw exception */
                throw parse_error(error_msg, line, column);
            }

            ast::module module = std::move(*ctx.ast_builder->module);
            return module;
        }
    } // anonymous namespace

    /* Public API */
    ast::module parse_datascript(const std::filesystem::path& path) {
        /* Read file contents */
        std::ifstream file(path);
        if (!file.is_open()) {
            throw std::runtime_error("Cannot open file: " + path.string());
        }

        std::stringstream buffer;
        buffer << file.rdbuf();
        std::string content = buffer.str();

        /* Parse with filename for error reporting */
        return parse_datascript_impl(content, path.string());
    }

    ast::module parse_datascript(const std::string& text) {
        /* Parse with generic filename */
        return parse_datascript_impl(text, "<string>");
    }
} // namespace datascript

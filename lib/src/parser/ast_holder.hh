//
// Created by igor on 26/11/2025.
//

#pragma once

#include <datascript/ast.hh>
#include <deque>

/* AST Module holder - temporary scaffolding for parsing */
struct ast_module_holder {
    datascript::ast::module* module;  // Pointer to module (holder owns it)

    /* Temporary storage for intermediate AST nodes during parsing */
    /* Using deque for stable pointers - elements never move on growth */
    /* Grammar rules return pointers to elements in these deques */
    /* After move into module, elements are in moved-from state (valid but empty) */
    std::deque<datascript::ast::type> temp_types;
    std::deque<datascript::ast::expr> temp_exprs;
    std::deque<datascript::ast::param> temp_params;
    std::deque<datascript::ast::enum_item> temp_enum_items;
    std::deque<datascript::ast::field_def> temp_fields;
    std::deque<datascript::ast::label_directive> temp_labels;
    std::deque<datascript::ast::alignment_directive> temp_alignments;
    std::deque<datascript::ast::struct_body_item> temp_body_items;
    std::deque<datascript::ast::choice_case> temp_choice_cases;
    std::deque<datascript::ast::union_case> temp_union_cases;
    std::deque<datascript::ast::statement> temp_statements;
    std::deque<datascript::ast::function_def> temp_functions;

    ~ast_module_holder() {
        delete module;  // Clean up module
    }
};

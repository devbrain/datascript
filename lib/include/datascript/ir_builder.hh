//
// IR Builder API
//
// Converts analyzed AST to intermediate representation
//

#pragma once

#include "ir.hh"
#include "semantic.hh"

namespace datascript::ir {

/// Build IR from semantic analysis results
bundle build_ir(const semantic::analyzed_module_set& analyzed);

} // namespace datascript::ir

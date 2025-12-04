//
// Phase 0: Desugar Inline Types
//
// Transforms inline union and struct fields into named type definitions.
// This runs before semantic analysis to simplify downstream processing.
//
// Example transformation:
//   Container {
//       union { ... } payload;
//   }
// Becomes:
//   Container__payload__type { ... }  // Generated union
//   Container {
//       Container__payload__type payload;  // Regular field
//   }
//

#include <datascript/semantic.hh>
#include <datascript/ast.hh>
#include <string>
#include <vector>
#include <variant>
#include <sstream>

namespace datascript::semantic::phases {

namespace {
    // Generate unique type name for inline type
    // Format: ParentStruct__fieldname__type
    std::string generate_inline_type_name(const std::string& parent_name,
                                          const std::string& field_name) {
        return parent_name + "__" + field_name + "__type";
    }

    // Desugar inline union field to named union + regular field
    // Returns the replacement field_def
    ast::field_def desugar_inline_union(
        const std::string& parent_name,
        ast::inline_union_field&& inline_union,  // Take by rvalue reference to move from
        ast::module& module)  // Non-const so we can add generated types
    {
        using namespace ast;

        // Generate unique type name
        std::string type_name = generate_inline_type_name(parent_name, inline_union.field_name);

        // Create union_def from inline union (move data)
        union_def generated_union{
            inline_union.pos,
            type_name,
            {},  // No parameters for generated types
            std::move(inline_union.cases),  // Move cases
            std::nullopt  // No docstring for generated types
        };

        // Add generated union to module
        module.unions.push_back(std::move(generated_union));

        // Create qualified name for the generated type
        qualified_name type_ref{
            inline_union.pos,
            {type_name}
        };

        // Create replacement field using the generated type
        field_def replacement{
            inline_union.pos,
            type{type_node{std::move(type_ref)}},
            std::move(inline_union.field_name),
            std::move(inline_union.condition),
            std::move(inline_union.constraint),
            std::nullopt,  // No default value
            std::move(inline_union.docstring)
        };

        return replacement;
    }

    // Desugar inline struct field to named struct + regular field
    ast::field_def desugar_inline_struct(
        const std::string& parent_name,
        ast::inline_struct_field&& inline_struct,  // Take by rvalue reference to move from
        ast::module& module)
    {
        using namespace ast;

        // Generate unique type name
        std::string type_name = generate_inline_type_name(parent_name, inline_struct.field_name);

        // Create struct_def from inline struct (move data)
        struct_def generated_struct{
            inline_struct.pos,
            type_name,
            {},  // No parameters for generated types
            std::move(inline_struct.body->items),  // Move body items
            std::nullopt  // No docstring for generated types
        };

        // Add generated struct to module
        module.structs.push_back(std::move(generated_struct));

        // Create qualified name for the generated type
        qualified_name type_ref{
            inline_struct.pos,
            {type_name}
        };

        // Create replacement field using the generated type
        field_def replacement{
            inline_struct.pos,
            type{type_node{std::move(type_ref)}},
            std::move(inline_struct.field_name),
            std::move(inline_struct.condition),
            std::move(inline_struct.constraint),
            std::nullopt,  // No default value
            std::move(inline_struct.docstring)
        };

        return replacement;
    }

    // Desugar inline types in a single struct
    void desugar_struct(ast::struct_def& struct_def, ast::module& module) {
        using namespace ast;

        std::vector<struct_body_item> new_body;
        new_body.reserve(struct_def.body.size());

        for (auto& item : struct_def.body) {
            if (std::holds_alternative<inline_union_field>(item)) {
                // Desugar inline union to named union + regular field
                auto inline_union = std::get<inline_union_field>(std::move(item));
                field_def replacement = desugar_inline_union(
                    struct_def.name,
                    std::move(inline_union),
                    module
                );
                new_body.emplace_back(std::move(replacement));
            }
            else if (std::holds_alternative<inline_struct_field>(item)) {
                // Desugar inline struct to named struct + regular field
                auto inline_struct = std::get<inline_struct_field>(std::move(item));
                field_def replacement = desugar_inline_struct(
                    struct_def.name,
                    std::move(inline_struct),
                    module
                );
                new_body.emplace_back(std::move(replacement));
            }
            else {
                // Keep other items as-is
                new_body.push_back(std::move(item));
            }
        }

        // Replace body with desugared version
        struct_def.body = std::move(new_body);
    }

    // Desugar inline types in a single union case
    void desugar_union_case(ast::union_case& union_case, const std::string& parent_name, ast::module& module) {
        using namespace ast;

        std::vector<struct_body_item> new_items;
        new_items.reserve(union_case.items.size());

        for (auto& item : union_case.items) {
            if (std::holds_alternative<inline_union_field>(item)) {
                // Desugar inline union to named union + regular field
                auto inline_union = std::get<inline_union_field>(std::move(item));
                // Use parent_name_casename as context
                std::string context = parent_name + "_" + union_case.case_name;
                field_def replacement = desugar_inline_union(
                    context,
                    std::move(inline_union),
                    module
                );
                new_items.emplace_back(std::move(replacement));
            }
            else if (std::holds_alternative<inline_struct_field>(item)) {
                // Desugar inline struct to named struct + regular field
                auto inline_struct = std::get<inline_struct_field>(std::move(item));
                // Use parent_name_casename as context
                std::string context = parent_name + "_" + union_case.case_name;
                field_def replacement = desugar_inline_struct(
                    context,
                    std::move(inline_struct),
                    module
                );
                new_items.emplace_back(std::move(replacement));
            }
            else {
                // Keep other items as-is
                new_items.push_back(std::move(item));
            }
        }

        // Replace items with desugared version
        union_case.items = std::move(new_items);
    }

    // Desugar inline types in a single union
    void desugar_union(ast::union_def& union_def, ast::module& module) {
        // Process all cases
        for (auto& union_case : union_def.cases) {
            desugar_union_case(union_case, union_def.name, module);
        }
    }

    // Desugar inline types in a single module
    void desugar_module(ast::module& module) {
        // Process all structs
        for (auto& struct_def : module.structs) {
            desugar_struct(struct_def, module);
        }

        // Process all unions (for inline types in anonymous case blocks)
        for (auto& union_def : module.unions) {
            desugar_union(union_def, module);
        }

        // Note: Choices don't support inline types (not in spec)
    }

} // anonymous namespace

// ============================================================================
// Public API: Inline Type Desugaring
// ============================================================================

void desugar_inline_types(module_set& modules) {
    // Desugar main module
    desugar_module(modules.main.module);

    // Desugar all imported modules
    for (auto& imported : modules.imported) {
        desugar_module(imported.module);
    }
}

} // namespace datascript::semantic::phases

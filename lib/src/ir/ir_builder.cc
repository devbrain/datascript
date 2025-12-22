//
// IR Builder: Converts analyzed AST to IR
//

#include <datascript/ir.hh>
#include <datascript/semantic.hh>
#include <chrono>
#include <iomanip>
#include <sstream>
#include <map>
#include <set>
#include <functional>

namespace datascript::ir {

namespace {

// ============================================================================
// Helper: Get current ISO 8601 timestamp
// ============================================================================

std::string get_iso8601_timestamp() {
    auto now = std::chrono::system_clock::now();
    auto time_t_now = std::chrono::system_clock::to_time_t(now);
    std::tm tm_now{};
    #ifdef _WIN32
    localtime_s(&tm_now, &time_t_now);
    #else
    localtime_r(&time_t_now, &tm_now);
    #endif

    std::ostringstream oss;
    oss << std::put_time(&tm_now, "%Y-%m-%dT%H:%M:%S");

    return oss.str() + "Z";
}

// ============================================================================
// Type Conversion
// ============================================================================

type_kind ast_primitive_to_ir_kind(const ast::primitive_type& prim) {
    if (prim.is_signed) {
        switch (prim.bits) {
            case 8: return type_kind::int8;
            case 16: return type_kind::int16;
            case 32: return type_kind::int32;
            case 64: return type_kind::int64;
            case 128: return type_kind::int128;
            default: return type_kind::int32;  // Shouldn't happen
        }
    } else {
        switch (prim.bits) {
            case 8: return type_kind::uint8;
            case 16: return type_kind::uint16;
            case 32: return type_kind::uint32;
            case 64: return type_kind::uint64;
            case 128: return type_kind::uint128;
            default: return type_kind::uint32;  // Shouldn't happen
        }
    }
}

endianness ast_endianness_to_ir(ast::endian e) {
    switch (e) {
        case ast::endian::little: return endianness::little;
        case ast::endian::big: return endianness::big;
        default: return endianness::native;
    }
}

case_selector_mode ast_selector_kind_to_ir(ast::case_selector_kind kind) {
    switch (kind) {
        case ast::case_selector_kind::exact:    return case_selector_mode::exact;
        case ast::case_selector_kind::range_ge: return case_selector_mode::ge;
        case ast::case_selector_kind::range_gt: return case_selector_mode::gt;
        case ast::case_selector_kind::range_le: return case_selector_mode::le;
        case ast::case_selector_kind::range_lt: return case_selector_mode::lt;
        case ast::case_selector_kind::range_ne: return case_selector_mode::ne;
        default: return case_selector_mode::exact;
    }
}

// ============================================================================
// Size and Alignment Calculation (Forward declarations - defined after type_index_maps)
// ============================================================================

struct type_index_maps;  // Forward declaration

// Type index maps for resolving user-defined types
struct type_index_maps {
    std::map<const ast::struct_def*, size_t> struct_indices;
    std::map<const ast::enum_def*, size_t> enum_indices;
    std::map<const ast::union_def*, size_t> union_indices;
    std::map<const ast::choice_def*, size_t> choice_indices;
    std::map<const ast::subtype_def*, size_t> subtype_indices;
};

// ============================================================================
// Monomorphization Context
// ============================================================================

// Key for caching monomorphized types: (base type pointer, argument values)
struct monomorphization_key {
    const void* base_type_ptr;  // Pointer to ast::struct_def or ast::union_def
    std::vector<uint64_t> argument_values;

    bool operator<(const monomorphization_key& other) const {
        if (base_type_ptr != other.base_type_ptr) {
            return base_type_ptr < other.base_type_ptr;
        }
        return argument_values < other.argument_values;
    }
};

// Parameter substitution context (maps parameter names to concrete values)
struct param_substitution {
    std::map<std::string, uint64_t> param_values;

    [[nodiscard]] bool has_param(const std::string& name) const {
        return param_values.find(name) != param_values.end();
    }

    [[nodiscard]] uint64_t get_param(const std::string& name) const {
        auto it = param_values.find(name);
        if (it != param_values.end()) {
            return it->second;
        }
        return 0;  // Fallback
    }
};

// Context for monomorphization (generates concrete types from parameterized definitions)
struct monomorphization_context {
    // Generated concrete structs and unions
    std::vector<struct_def> concrete_structs;
    std::vector<union_def> concrete_unions;

    // Wrapper structs generated for anonymous union case blocks
    std::vector<struct_def> wrapper_structs;

    // Cache: (base_type, args) -> index in concrete_structs/concrete_unions
    std::map<monomorphization_key, size_t> struct_cache;
    std::map<monomorphization_key, size_t> union_cache;

    // Reference to analyzed module set (for looking up definitions)
    const semantic::analyzed_module_set* analyzed = nullptr;

    // Mutable reference to index maps (we'll update them as we generate concrete types)
    type_index_maps* index_maps = nullptr;

    // Current parameter substitution (used during monomorphization)
    param_substitution* current_substitution = nullptr;
};

// ============================================================================
// Size and Alignment Calculation Helpers
// ============================================================================

/**
 * Calculate alignment requirement for a list of fields.
 * Returns the maximum alignment of all fields.
 */
size_t calculate_struct_alignment(const std::vector<field>& fields) {
    size_t max_alignment = 1;
    for (const auto& field : fields) {
        // For now, use simple alignment based on type kind
        size_t field_align = 1;
        switch (field.type.kind) {
            case type_kind::uint16:
            case type_kind::int16:
                field_align = 2;
                break;
            case type_kind::uint32:
            case type_kind::int32:
                field_align = 4;
                break;
            case type_kind::uint64:
            case type_kind::int64:
                field_align = 8;
                break;
            case type_kind::uint128:
            case type_kind::int128:
                field_align = 16;
                break;
            default:
                field_align = 1;
                break;
        }
        max_alignment = std::max(max_alignment, field_align);
    }
    return max_alignment;
}

/**
 * Calculate total size of a struct with alignment padding.
 * Returns 0 for variable-size structs.
 */
size_t calculate_struct_size(const std::vector<field>& fields) {
    size_t total_size = 0;
    size_t max_alignment = 1;

    for (const auto& field : fields) {
        // Get field size
        size_t field_size = 0;
        size_t field_align = 1;

        switch (field.type.kind) {
            case type_kind::uint8:
            case type_kind::int8:
            case type_kind::boolean:
                field_size = 1;
                field_align = 1;
                break;
            case type_kind::uint16:
            case type_kind::int16:
                field_size = 2;
                field_align = 2;
                break;
            case type_kind::uint32:
            case type_kind::int32:
                field_size = 4;
                field_align = 4;
                break;
            case type_kind::uint64:
            case type_kind::int64:
                field_size = 8;
                field_align = 8;
                break;
            case type_kind::uint128:
            case type_kind::int128:
                field_size = 16;
                field_align = 16;
                break;
            case type_kind::bitfield:
                if (field.type.bit_width) {
                    field_size = (*field.type.bit_width + 7) / 8;
                }
                field_align = 1;
                break;
            case type_kind::array_fixed:
                // Fixed arrays - would need element size, return 0 for now
                return 0;
            default:
                // Variable size or unknown
                return 0;
        }

        max_alignment = std::max(max_alignment, field_align);

        // Add padding before field if needed
        if (total_size % field_align != 0) {
            total_size += field_align - (total_size % field_align);
        }

        total_size += field_size;
    }

    // Add tail padding
    if (total_size > 0 && max_alignment > 0 && total_size % max_alignment != 0) {
        total_size += max_alignment - (total_size % max_alignment);
    }

    return total_size;
}

/**
 * Calculate size of a union (max of all case sizes).
 */
size_t calculate_union_size(const std::vector<union_case>& cases) {
    size_t max_size = 0;
    for (const auto& case_def : cases) {
        size_t case_size = calculate_struct_size(case_def.fields);
        max_size = std::max(max_size, case_size);
    }
    return max_size;
}

/**
 * Calculate alignment of a union (max of all case alignments).
 */
size_t calculate_union_alignment(const std::vector<union_case>& cases) {
    size_t max_alignment = 1;
    for (const auto& case_def : cases) {
        size_t case_align = calculate_struct_alignment(case_def.fields);
        max_alignment = std::max(max_alignment, case_align);
    }
    return max_alignment;
}

/**
 * Calculate size of a choice (max of all case field sizes plus tag).
 */
size_t calculate_choice_size(const std::vector<choice_def::case_def>& cases) {
    size_t max_size = 0;
    for (const auto& case_def : cases) {
        // Get size of case field type
        size_t case_size = 0;
        switch (case_def.case_field.type.kind) {
            case type_kind::uint8:
            case type_kind::int8:
            case type_kind::boolean:
                case_size = 1;
                break;
            case type_kind::uint16:
            case type_kind::int16:
                case_size = 2;
                break;
            case type_kind::uint32:
            case type_kind::int32:
                case_size = 4;
                break;
            case type_kind::uint64:
            case type_kind::int64:
                case_size = 8;
                break;
            default:
                case_size = 0;  // Variable or unknown
                break;
        }
        max_size = std::max(max_size, case_size);
    }
    // Add tag size
    return 4 + max_size;  // uint32 tag
}

/**
 * Calculate alignment of a choice (max of tag and all case alignments).
 */
size_t calculate_choice_alignment(const std::vector<choice_def::case_def>& cases) {
    size_t max_alignment = 4;  // Tag is uint32
    for (const auto& case_def : cases) {
        size_t case_align = 1;
        switch (case_def.case_field.type.kind) {
            case type_kind::uint16:
            case type_kind::int16:
                case_align = 2;
                break;
            case type_kind::uint32:
            case type_kind::int32:
                case_align = 4;
                break;
            case type_kind::uint64:
            case type_kind::int64:
                case_align = 8;
                break;
            default:
                case_align = 1;
                break;
        }
        max_alignment = std::max(max_alignment, case_align);
    }
    return max_alignment;
}

// ============================================================================
// Enhanced Size/Alignment Calculation with Bundle Access
// ============================================================================

/**
 * Get the size of a type, with access to the complete bundle for nested types.
 * Returns 0 for variable-size types.
 */
size_t get_type_size(const type_ref& type, const bundle& b) {
    switch (type.kind) {
        // Primitives
        case type_kind::uint8:
        case type_kind::int8:
        case type_kind::boolean:
            return 1;
        case type_kind::uint16:
        case type_kind::int16:
            return 2;
        case type_kind::uint32:
        case type_kind::int32:
            return 4;
        case type_kind::uint64:
        case type_kind::int64:
            return 8;
        case type_kind::uint128:
        case type_kind::int128:
            return 16;

        // Bitfields
        case type_kind::bitfield:
            if (type.bit_width) {
                return (*type.bit_width + 7) / 8;  // Round up to bytes
            }
            return 0;

        // Variable-size types
        case type_kind::string:
        case type_kind::array_variable:
        case type_kind::array_ranged:
            return 0;  // Variable size

        // Fixed arrays
        case type_kind::array_fixed:
            if (type.element_type && type.array_size) {
                size_t element_size = get_type_size(*type.element_type, b);
                if (element_size == 0) {
                    return 0;  // Variable-size elements
                }
                return element_size * (*type.array_size);
            }
            return 0;

        // User-defined types (lookup by index)
        case type_kind::struct_type:
            if (type.type_index && *type.type_index < b.structs.size()) {
                return b.structs[*type.type_index].total_size;
            }
            return 0;

        case type_kind::union_type:
            if (type.type_index && *type.type_index < b.unions.size()) {
                return b.unions[*type.type_index].size;
            }
            return 0;

        case type_kind::choice_type:
            if (type.type_index && *type.type_index < b.choices.size()) {
                return b.choices[*type.type_index].size;
            }
            return 0;

        case type_kind::enum_type:
            // Enums use their base type's size
            if (type.type_index && *type.type_index < b.enums.size()) {
                return get_type_size(b.enums[*type.type_index].base_type, b);
            }
            return 0;

        case type_kind::subtype_ref:
            // Subtypes use their base type's size
            if (type.type_index && *type.type_index < b.subtypes.size()) {
                return get_type_size(b.subtypes[*type.type_index].base_type, b);
            }
            return 0;

        default:
            return 0;
    }
}

/**
 * Get the alignment of a type, with access to the complete bundle for nested types.
 */
size_t get_type_alignment(const type_ref& type, const bundle& b) {
    switch (type.kind) {
        // 1-byte alignment
        case type_kind::uint8:
        case type_kind::int8:
        case type_kind::boolean:
        case type_kind::bitfield:
            return 1;

        // 2-byte alignment
        case type_kind::uint16:
        case type_kind::int16:
            return 2;

        // 4-byte alignment
        case type_kind::uint32:
        case type_kind::int32:
            return 4;

        // 8-byte alignment
        case type_kind::uint64:
        case type_kind::int64:
            return 8;

        // 16-byte alignment
        case type_kind::uint128:
        case type_kind::int128:
            return 16;

        // Variable-size types
        case type_kind::string:
            return 1;  // String pointers have pointer alignment, but content is byte-aligned

        // Arrays: use element alignment
        case type_kind::array_fixed:
        case type_kind::array_variable:
        case type_kind::array_ranged:
            if (type.element_type) {
                return get_type_alignment(*type.element_type, b);
            }
            return 1;

        // User-defined types (lookup by index)
        case type_kind::struct_type:
            if (type.type_index && *type.type_index < b.structs.size()) {
                return b.structs[*type.type_index].alignment;
            }
            return 1;

        case type_kind::union_type:
            if (type.type_index && *type.type_index < b.unions.size()) {
                return b.unions[*type.type_index].alignment;
            }
            return 1;

        case type_kind::choice_type:
            if (type.type_index && *type.type_index < b.choices.size()) {
                return b.choices[*type.type_index].alignment;
            }
            return 1;

        case type_kind::enum_type:
            // Enums use their base type's alignment
            if (type.type_index && *type.type_index < b.enums.size()) {
                return get_type_alignment(b.enums[*type.type_index].base_type, b);
            }
            return 1;

        case type_kind::subtype_ref:
            // Subtypes use their base type's alignment
            if (type.type_index && *type.type_index < b.subtypes.size()) {
                return get_type_alignment(b.subtypes[*type.type_index].base_type, b);
            }
            return 1;

        default:
            return 1;
    }
}

/**
 * Recalculate struct size with full bundle access (handles nested types and arrays).
 */
size_t recalculate_struct_size(const std::vector<field>& fields, const bundle& b) {
    size_t total_size = 0;
    size_t max_alignment = 1;

    for (const auto& field : fields) {
        size_t field_size = get_type_size(field.type, b);
        size_t field_align = get_type_alignment(field.type, b);

        max_alignment = std::max(max_alignment, field_align);

        // Add padding before field if needed
        if (field_align > 0 && total_size % field_align != 0) {
            total_size += field_align - (total_size % field_align);
        }

        // Variable-size field makes entire struct variable-size
        if (field_size == 0) {
            return 0;
        }

        total_size += field_size;
    }

    // Add tail padding to align to struct alignment
    if (total_size > 0 && max_alignment > 0 && total_size % max_alignment != 0) {
        total_size += max_alignment - (total_size % max_alignment);
    }

    return total_size;
}

/**
 * Recalculate struct alignment with full bundle access.
 */
size_t recalculate_struct_alignment(const std::vector<field>& fields, const bundle& b) {
    size_t max_alignment = 1;
    for (const auto& field : fields) {
        size_t field_align = get_type_alignment(field.type, b);
        max_alignment = std::max(max_alignment, field_align);
    }
    return max_alignment;
}

/**
 * Post-processing pass: Calculate sizes and alignments for all types in the bundle.
 * This is called after all types are built and type indices are finalized.
 */
void calculate_all_sizes_and_alignments(bundle& b) {
    // Recalculate struct sizes and alignments
    for (auto& struct_def : b.structs) {
        struct_def.total_size = recalculate_struct_size(struct_def.fields, b);
        struct_def.alignment = recalculate_struct_alignment(struct_def.fields, b);
    }

    // Recalculate union sizes and alignments
    for (auto& union_def : b.unions) {
        size_t max_size = 0;
        size_t max_alignment = 1;
        for (const auto& union_case : union_def.cases) {
            size_t case_size = recalculate_struct_size(union_case.fields, b);
            size_t case_align = recalculate_struct_alignment(union_case.fields, b);
            max_size = std::max(max_size, case_size);
            max_alignment = std::max(max_alignment, case_align);
        }
        union_def.size = max_size;
        union_def.alignment = max_alignment;
    }

    // Recalculate choice sizes and alignments
    for (auto& choice_def : b.choices) {
        size_t max_size = 0;
        size_t max_alignment = 4;  // Tag is uint32
        for (const auto& choice_case : choice_def.cases) {
            size_t case_size = get_type_size(choice_case.case_field.type, b);
            size_t case_align = get_type_alignment(choice_case.case_field.type, b);
            max_size = std::max(max_size, case_size);
            max_alignment = std::max(max_alignment, case_align);
        }
        choice_def.size = 4 + max_size;  // Tag + max case size
        choice_def.alignment = max_alignment;
    }
}

// Forward declarations
type_ref build_type_ref(const ast::type& ast_type,
                       const semantic::analyzed_module_set& analyzed,
                       const type_index_maps& index_maps,
                       monomorphization_context* mono_ctx);

field build_field(const ast::field_def& ast_field,
                 const semantic::analyzed_module_set& analyzed,
                 const type_index_maps& index_maps,
                 monomorphization_context* mono_ctx);

expr build_expr(const ast::expr& ast_expr,
               const semantic::analyzed_module_set& analyzed,
               monomorphization_context* mono_ctx = nullptr);

// Monomorphize a parameterized struct, returning the index of the concrete struct
size_t monomorphize_struct(const ast::struct_def* base_struct,
                           const std::vector<uint64_t>& arg_values,
                           monomorphization_context* mono_ctx);

// Monomorphize a parameterized union, returning the index of the concrete union
size_t monomorphize_union(const ast::union_def* base_union,
                          const std::vector<uint64_t>& arg_values,
                          monomorphization_context* mono_ctx);

expr::op_type ast_binary_op_to_ir(ast::binary_op op) {
    switch (op) {
        case ast::binary_op::add: return expr::add;
        case ast::binary_op::sub: return expr::sub;
        case ast::binary_op::mul: return expr::mul;
        case ast::binary_op::div: return expr::div;
        case ast::binary_op::mod: return expr::mod;
        case ast::binary_op::eq: return expr::eq;
        case ast::binary_op::ne: return expr::ne;
        case ast::binary_op::lt: return expr::lt;
        case ast::binary_op::gt: return expr::gt;
        case ast::binary_op::le: return expr::le;
        case ast::binary_op::ge: return expr::ge;
        case ast::binary_op::log_and: return expr::logical_and;
        case ast::binary_op::log_or: return expr::logical_or;
        case ast::binary_op::bit_and: return expr::bit_and;
        case ast::binary_op::bit_or: return expr::bit_or;
        case ast::binary_op::bit_xor: return expr::bit_xor;
        case ast::binary_op::lshift: return expr::bit_shift_left;
        case ast::binary_op::rshift: return expr::bit_shift_right;
        default: return expr::add;
    }
}

expr::op_type ast_unary_op_to_ir(ast::unary_op op) {
    switch (op) {
        case ast::unary_op::neg: return expr::negate;
        case ast::unary_op::pos: return expr::add;  // No-op
        case ast::unary_op::log_not: return expr::logical_not;
        case ast::unary_op::bit_not: return expr::bit_not;
        default: return expr::negate;
    }
}

// ============================================================================
// Helper: Extract position from variant nodes
// ============================================================================

ast::source_pos get_expr_pos(const ast::expr& e) {
    return std::visit([](const auto& node) { return node.pos; }, e.node);
}

ast::source_pos get_type_pos(const ast::type& t) {
    return std::visit([](const auto& node) { return node.pos; }, t.node);
}

// ============================================================================
// Expression Conversion
// ============================================================================

// Forward declaration for recursive usage
std::string build_field_access_path(const ast::field_access_expr& field_access);

// Helper: Check if an identifier refers to an enum type
bool is_enum_type(const std::string& name, const semantic::analyzed_module_set& analyzed) {
    // Check main module
    if (analyzed.symbols.main.enums.count(name)) {
        return true;
    }

    // Check wildcard imports
    if (analyzed.symbols.wildcard_enums.count(name)) {
        return true;
    }

    // Check imported modules (qualified names)
    for (const auto& [package_name, module_syms] : analyzed.symbols.imported) {
        if (module_syms.enums.count(name)) {
            return true;
        }
    }

    return false;
}

expr build_expr(const ast::expr& ast_expr,
               const semantic::analyzed_module_set& analyzed,
               monomorphization_context* mono_ctx) {
    expr result;
    result.source = source_location::from_ast(get_expr_pos(ast_expr));

    if (auto* lit_int = std::get_if<ast::literal_int>(&ast_expr.node)) {
        result.type = expr::literal_int;
        result.int_value = lit_int->value;
    }
    else if (auto* lit_bool = std::get_if<ast::literal_bool>(&ast_expr.node)) {
        result.type = expr::literal_bool;
        result.bool_value = lit_bool->value;
    }
    else if (auto* lit_str = std::get_if<ast::literal_string>(&ast_expr.node)) {
        result.type = expr::literal_string;
        result.string_value = lit_str->value;
    }
    else if (auto* id = std::get_if<ast::identifier>(&ast_expr.node)) {
        // Check if this is a parameter reference that should be substituted
        if (mono_ctx && mono_ctx->current_substitution &&
            mono_ctx->current_substitution->has_param(id->name)) {
            // Substitute parameter with concrete value
            result.type = expr::literal_int;
            result.int_value = mono_ctx->current_substitution->get_param(id->name);
        } else {
            // Check if this is an enum constant (search all enums for this item name)
            bool found_enum_constant = false;

            // Search main module enums
            if (analyzed.original) {
                for (const auto& enum_def : analyzed.original->main.module.enums) {
                    for (const auto& item : enum_def.items) {
                        if (item.name == id->name) {
                            // Found an enum constant - create fully qualified constant_ref
                            result.type = expr::constant_ref;
                            result.ref_name = enum_def.name + "." + item.name;
                            found_enum_constant = true;
                            break;
                        }
                    }
                    if (found_enum_constant) break;
                }

                // Search imported module enums if not found yet
                if (!found_enum_constant) {
                    for (const auto& imported : analyzed.original->imported) {
                        for (const auto& enum_def : imported.module.enums) {
                            for (const auto& item : enum_def.items) {
                                if (item.name == id->name) {
                                    // Found an enum constant - create fully qualified constant_ref
                                    result.type = expr::constant_ref;
                                    result.ref_name = enum_def.name + "." + item.name;
                                    found_enum_constant = true;
                                    break;
                                }
                            }
                            if (found_enum_constant) break;
                        }
                        if (found_enum_constant) break;
                    }
                }
            }

            if (!found_enum_constant) {
                // Could be constant reference, field, or unsubstituted parameter
                result.type = expr::parameter_ref;
                result.ref_name = id->name;
            }
        }
    }
    else if (auto* bin_expr = std::get_if<ast::binary_expr>(&ast_expr.node)) {
        result.type = expr::binary_op;
        result.op = ast_binary_op_to_ir(bin_expr->op);
        result.left = std::make_unique<expr>(build_expr(*bin_expr->left, analyzed, mono_ctx));
        result.right = std::make_unique<expr>(build_expr(*bin_expr->right, analyzed, mono_ctx));
    }
    else if (auto* un_expr = std::get_if<ast::unary_expr>(&ast_expr.node)) {
        result.type = expr::unary_op;
        result.op = ast_unary_op_to_ir(un_expr->op);
        result.left = std::make_unique<expr>(build_expr(*un_expr->operand, analyzed, mono_ctx));
    }
    else if (auto* tern_expr = std::get_if<ast::ternary_expr>(&ast_expr.node)) {
        result.type = expr::ternary_op;
        result.condition = std::make_unique<expr>(build_expr(*tern_expr->condition, analyzed, mono_ctx));
        result.true_expr = std::make_unique<expr>(build_expr(*tern_expr->true_expr, analyzed, mono_ctx));
        result.false_expr = std::make_unique<expr>(build_expr(*tern_expr->false_expr, analyzed, mono_ctx));
    }
    else if (auto* field_access = std::get_if<ast::field_access_expr>(&ast_expr.node)) {
        // Build full dotted path (e.g., "header.data.offset" or "Color.RED")
        result.ref_name = build_field_access_path(*field_access);

        // Check if this is enum member access (e.g., Color.RED)
        // If the base object is an identifier that refers to an enum, this is a constant_ref
        bool is_enum_member = false;
        if (auto* base_ident = std::get_if<ast::identifier>(&field_access->object->node)) {
            is_enum_member = is_enum_type(base_ident->name, analyzed);
        }

        // Use constant_ref for enum members (Color.RED), field_ref for struct fields (obj.field)
        result.type = is_enum_member ? expr::constant_ref : expr::field_ref;
    }
    else if (auto* array_index = std::get_if<ast::array_index_expr>(&ast_expr.node)) {
        result.type = expr::array_index;
        result.left = std::make_unique<expr>(build_expr(*array_index->array, analyzed, mono_ctx));
        result.right = std::make_unique<expr>(build_expr(*array_index->index, analyzed, mono_ctx));
    }
    else if (auto* func_call = std::get_if<ast::function_call_expr>(&ast_expr.node)) {
        result.type = expr::function_call;

        // Get function name from the function expression (usually an identifier)
        if (auto* func_ident = std::get_if<ast::identifier>(&func_call->function->node)) {
            result.ref_name = func_ident->name;
        } else if (auto* func_field_access = std::get_if<ast::field_access_expr>(&func_call->function->node)) {
            // For method calls like obj.method(), build the full path
            result.ref_name = build_field_access_path(*func_field_access);
        }

        // Build arguments
        for (const auto& arg : func_call->arguments) {
            result.arguments.push_back(std::make_unique<expr>(build_expr(arg, analyzed, mono_ctx)));
        }
    }

    return result;
}

// Helper: Build full dotted path from nested field_access_expr
std::string build_field_access_path(const ast::field_access_expr& field_access) {
    std::string path;

    // Recursively build the object path
    if (auto* nested_access = std::get_if<ast::field_access_expr>(&field_access.object->node)) {
        path = build_field_access_path(*nested_access) + ".";
    }
    else if (auto* ident = std::get_if<ast::identifier>(&field_access.object->node)) {
        path = ident->name + ".";
    }
    // For other expression types, we can't build a simple path, so just use the field name

    path += field_access.field_name;
    return path;
}

// ============================================================================
// Monomorphization Implementation
// ============================================================================

// Generate a concrete name for a monomorphized type (e.g., "Record_16_256")
std::string generate_concrete_name(const std::string& base_name,
                                   const std::vector<uint64_t>& arg_values) {
    std::string result = base_name;
    for (auto arg : arg_values) {
        result += "_" + std::to_string(arg);
    }
    return result;
}

// Monomorphize a parameterized struct
size_t monomorphize_struct(const ast::struct_def* base_struct,
                           const std::vector<uint64_t>& arg_values,
                           monomorphization_context* mono_ctx) {
    // Create cache key
    monomorphization_key key{base_struct, arg_values};

    // Check if already monomorphized
    auto cache_it = mono_ctx->struct_cache.find(key);
    if (cache_it != mono_ctx->struct_cache.end()) {
        return cache_it->second;
    }

    // Generate concrete struct
    struct_def concrete;
    concrete.name = generate_concrete_name(base_struct->name, arg_values);
    concrete.source = source_location::from_ast(base_struct->pos);

    // Set up parameter substitution
    param_substitution subst;
    for (size_t i = 0; i < base_struct->parameters.size() && i < arg_values.size(); ++i) {
        subst.param_values[base_struct->parameters[i].name] = arg_values[i];
    }

    // Save previous substitution and set current
    param_substitution* prev_subst = mono_ctx->current_substitution;
    mono_ctx->current_substitution = &subst;

    // Build fields with parameter substitution and label/alignment directives
    std::optional<expr> pending_label = std::nullopt;
    std::optional<uint64_t> pending_alignment = std::nullopt;

    for (const auto& body_item : base_struct->body) {
        if (auto* ast_label = std::get_if<ast::label_directive>(&body_item)) {
            // Label directive: evaluate expression for position
            pending_label = build_expr(ast_label->label_expr, *mono_ctx->analyzed, mono_ctx);
        }
        else if (auto* ast_align = std::get_if<ast::alignment_directive>(&body_item)) {
            // Alignment directive: evaluate expression (must be constant)
            // Try to evaluate as compile-time constant
            std::vector<semantic::diagnostic> temp_diags;
            auto align_val = semantic::phases::evaluate_constant_uint(
                ast_align->alignment_expr, *mono_ctx->analyzed, temp_diags);

            if (align_val) {
                pending_alignment = *align_val;
            } else {
                // Fallback: try to extract from built expression
                auto align_expr = build_expr(ast_align->alignment_expr, *mono_ctx->analyzed, mono_ctx);
                if (align_expr.type == expr::literal_int) {
                    pending_alignment = align_expr.int_value;
                }
                // If neither works, alignment is not applied (error already reported in Phase 5)
            }
        }
        else if (auto* ast_field = std::get_if<ast::field_def>(&body_item)) {
            // Field: build it with parameter substitution and apply pending directives
            auto ir_field = build_field(*ast_field, *mono_ctx->analyzed, *mono_ctx->index_maps, mono_ctx);

            // Apply pending label if any
            if (pending_label.has_value()) {
                ir_field.label = std::move(*pending_label);
                pending_label.reset();
            }

            // Apply pending alignment if any
            if (pending_alignment.has_value()) {
                ir_field.alignment = *pending_alignment;
                pending_alignment.reset();
            }

            concrete.fields.push_back(std::move(ir_field));
        }
        else if (auto* ast_func = std::get_if<ast::function_def>(&body_item)) {
            // Function: build it with parameter substitution
            function_def ir_func;
            ir_func.name = ast_func->name;
            ir_func.source = source_location::from_ast(ast_func->pos);
            ir_func.return_type = build_type_ref(ast_func->return_type, *mono_ctx->analyzed, *mono_ctx->index_maps, mono_ctx);

            // Build parameters
            for (const auto& ast_param : ast_func->parameters) {
                function_param ir_param;
                ir_param.name = ast_param.name;
                ir_param.param_type = build_type_ref(ast_param.param_type, *mono_ctx->analyzed, *mono_ctx->index_maps, mono_ctx);
                ir_param.source = source_location::from_ast(ast_param.pos);
                ir_func.parameters.push_back(std::move(ir_param));
            }

            // Build function body (statements) - parameter substitution is active
            for (const auto& ast_stmt : ast_func->body) {
                if (auto* ret_stmt = std::get_if<ast::return_statement>(&ast_stmt)) {
                    return_statement ir_ret;
                    ir_ret.value = build_expr(ret_stmt->value, *mono_ctx->analyzed, mono_ctx);
                    ir_ret.source = source_location::from_ast(ret_stmt->pos);
                    ir_func.body.emplace_back(std::move(ir_ret));
                } else if (auto* expr_stmt = std::get_if<ast::expression_statement>(&ast_stmt)) {
                    expression_statement ir_expr;
                    ir_expr.expression = build_expr(expr_stmt->expression, *mono_ctx->analyzed, mono_ctx);
                    ir_expr.source = source_location::from_ast(expr_stmt->pos);
                    ir_func.body.emplace_back(std::move(ir_expr));
                }
            }

            if (ast_func->docstring) {
                ir_func.documentation = ast_func->docstring.value();
            }

            concrete.functions.push_back(std::move(ir_func));
        }
    }

    // Restore previous substitution
    mono_ctx->current_substitution = prev_subst;

    if (base_struct->docstring) {
        concrete.documentation = base_struct->docstring.value();
    }

    // Add to context
    size_t index = mono_ctx->concrete_structs.size();
    mono_ctx->concrete_structs.push_back(std::move(concrete));

    // Cache the result
    mono_ctx->struct_cache[key] = index;

    return index;
}

// Monomorphize a parameterized union
size_t monomorphize_union(const ast::union_def* base_union,
                          const std::vector<uint64_t>& arg_values,
                          monomorphization_context* mono_ctx) {
    // Create cache key
    const monomorphization_key key{base_union, arg_values};

    // Check if already monomorphized
    auto cache_it = mono_ctx->union_cache.find(key);
    if (cache_it != mono_ctx->union_cache.end()) {
        return cache_it->second;
    }

    // Generate concrete union
    union_def concrete;
    concrete.name = generate_concrete_name(base_union->name, arg_values);
    concrete.source = source_location::from_ast(base_union->pos);

    // Set up parameter substitution
    param_substitution subst;
    for (size_t i = 0; i < base_union->parameters.size() && i < arg_values.size(); ++i) {
        subst.param_values[base_union->parameters[i].name] = arg_values[i];
    }

    // Save previous substitution and set current
    param_substitution* prev_subst = mono_ctx->current_substitution;
    mono_ctx->current_substitution = &subst;

    // Build cases with parameter substitution
    for (const auto& ast_case : base_union->cases) {
        union_case ir_case;
        ir_case.case_name = ast_case.case_name;
        ir_case.source = source_location::from_ast(ast_case.pos);
        ir_case.is_anonymous_block = ast_case.is_anonymous_block;

        for (const auto& item : ast_case.items) {
            if (std::holds_alternative<ast::field_def>(item)) {
                const auto& ast_field = std::get<ast::field_def>(item);
                ir_case.fields.push_back(build_field(ast_field, *mono_ctx->analyzed, *mono_ctx->index_maps, mono_ctx));
            }
        }

        if (ast_case.condition) {
            ir_case.condition = build_expr(ast_case.condition.value(), *mono_ctx->analyzed, mono_ctx);
        }

        if (ast_case.docstring) {
            ir_case.documentation = ast_case.docstring.value();
        }

        concrete.cases.push_back(std::move(ir_case));
    }

    // Restore previous substitution
    mono_ctx->current_substitution = prev_subst;

    if (base_union->docstring) {
        concrete.documentation = base_union->docstring.value();
    }

    // Add to context
    size_t index = mono_ctx->concrete_unions.size();
    mono_ctx->concrete_unions.push_back(std::move(concrete));

    // Cache the result
    mono_ctx->union_cache[key] = index;

    return index;
}

type_ref build_type_ref(const ast::type& ast_type,
                       const semantic::analyzed_module_set& analyzed,
                       const type_index_maps& index_maps,
                       monomorphization_context* mono_ctx) {
    type_ref result;
    result.source = source_location::from_ast(get_type_pos(ast_type));

    // Get size/alignment from semantic analysis if available
    auto it = analyzed.type_info_map.find(&ast_type);
    if (it != analyzed.type_info_map.end()) {
        result.size_bytes = it->second.size;
        result.alignment = it->second.alignment;
        result.is_variable_size = it->second.is_variable_size;
    }

    // Handle type_instantiation (parameterized types)
    if (auto* type_inst = std::get_if<ast::type_instantiation>(&ast_type.node)) {
        // Look up the base type from the qualified name
        const auto& qname = type_inst->base_type;

        // Evaluate argument expressions to concrete values
        std::vector<uint64_t> arg_values;
        for (const auto& arg_expr : type_inst->arguments) {
            // Try to evaluate as compile-time constant expression
            std::vector<semantic::diagnostic> temp_diags;
            auto arg_val = semantic::phases::evaluate_constant_uint(arg_expr, analyzed, temp_diags);

            if (arg_val) {
                // Successfully evaluated as constant
                arg_values.push_back(*arg_val);
            } else {
                // Fallback: try literal integers directly
                if (auto* lit_int = std::get_if<ast::literal_int>(&arg_expr.node)) {
                    arg_values.push_back(lit_int->value);
                } else {
                    // Can't evaluate - use 0 as fallback (error already reported in Phase 5)
                    arg_values.push_back(0);
                }
            }
        }

        // Try to find the base type as a struct
        if (auto* struct_def = analyzed.symbols.find_struct_qualified(qname.parts)) {
            if (mono_ctx) {
                // Monomorphize the struct
                size_t concrete_idx = monomorphize_struct(struct_def, arg_values, mono_ctx);
                result.kind = type_kind::struct_type;
                result.type_index = concrete_idx;
            }
            return result;
        }

        // Try to find the base type as a union
        if (auto* union_def = analyzed.symbols.find_union_qualified(qname.parts)) {
            if (mono_ctx) {
                // Monomorphize the union
                size_t concrete_idx = monomorphize_union(union_def, arg_values, mono_ctx);
                result.kind = type_kind::union_type;
                result.type_index = concrete_idx;
            }
            return result;
        }

        // Try to find the base type as a choice
        if (auto* choice_def = analyzed.symbols.find_choice_qualified(qname.parts)) {
            // Choices are NOT monomorphized - instead, we store the selector argument expressions
            // to be passed at runtime to the choice's read() method
            result.kind = type_kind::choice_type;

            // Find choice index
            if (mono_ctx) {
                auto choice_it = mono_ctx->index_maps->choice_indices.find(choice_def);
                if (choice_it != mono_ctx->index_maps->choice_indices.end()) {
                    result.type_index = choice_it->second;
                }
            }

            // Store selector argument expressions
            for (const auto& arg_expr : type_inst->arguments) {
                result.choice_selector_args.push_back(
                    std::make_unique<expr>(build_expr(arg_expr, analyzed, mono_ctx))
                );
            }

            return result;
        }

        // Base type not found - this should have been caught in semantic analysis
        // Return default result
        return result;
    }

    if (auto* prim = std::get_if<ast::primitive_type>(&ast_type.node)) {
        result.kind = ast_primitive_to_ir_kind(*prim);
        if (prim->byte_order != ast::endian::unspec) {
            result.byte_order = ast_endianness_to_ir(prim->byte_order);
        }
    }
    else if (std::get_if<ast::string_type>(&ast_type.node)) {
        result.kind = type_kind::string;
    }
    else if (auto* u16str = std::get_if<ast::u16_string_type>(&ast_type.node)) {
        result.kind = type_kind::u16_string;
        if (u16str->byte_order != ast::endian::unspec) {
            result.byte_order = ast_endianness_to_ir(u16str->byte_order);
        }
    }
    else if (auto* u32str = std::get_if<ast::u32_string_type>(&ast_type.node)) {
        result.kind = type_kind::u32_string;
        if (u32str->byte_order != ast::endian::unspec) {
            result.byte_order = ast_endianness_to_ir(u32str->byte_order);
        }
    }
    else if (std::get_if<ast::bool_type>(&ast_type.node)) {
        result.kind = type_kind::boolean;
    }
    else if (auto* bitfield_fixed = std::get_if<ast::bit_field_type_fixed>(&ast_type.node)) {
        result.kind = type_kind::bitfield;
        result.bit_width = bitfield_fixed->width;
    }
    else if (auto* bitfield_expr = std::get_if<ast::bit_field_type_expr>(&ast_type.node)) {
        result.kind = type_kind::bitfield;
        // Evaluate width expression as compile-time constant
        std::vector<semantic::diagnostic> temp_diags;
        auto width_val = semantic::phases::evaluate_constant_uint(
            bitfield_expr->width_expr, analyzed, temp_diags);

        if (width_val) {
            result.bit_width = *width_val;
        } else {
            // Fallback: try literal integers directly
            if (auto* lit_int = std::get_if<ast::literal_int>(&bitfield_expr->width_expr.node)) {
                result.bit_width = lit_int->value;
            } else {
                // Can't evaluate - use 0 (error already reported in Phase 5)
                result.bit_width = 0;
            }
        }
    }
    else if (auto* arr_fixed = std::get_if<ast::array_type_fixed>(&ast_type.node)) {
        result.element_type = std::make_unique<type_ref>(
            build_type_ref(*arr_fixed->element_type, analyzed, index_maps, mono_ctx));

        // Evaluate array size expression
        if (auto* lit_int = std::get_if<ast::literal_int>(&arr_fixed->size.node)) {
            // Fixed-size array with literal size
            result.kind = type_kind::array_fixed;
            result.array_size = lit_int->value;
            result.is_variable_size = false;
        }
        // Check if the size is a constant reference
        else if (auto* id = std::get_if<ast::identifier>(&arr_fixed->size.node)) {
            // Try to find this as a constant
            const ast::constant_def* const_def = analyzed.symbols.find_constant(id->name);
            if (const_def) {
                // Found a constant - look up its evaluated value
                auto const_val_it = analyzed.constant_values.find(const_def);
                if (const_val_it != analyzed.constant_values.end()) {
                    // Constant folding: treat as fixed-size array
                    result.kind = type_kind::array_fixed;
                    result.array_size = const_val_it->second;
                    result.is_variable_size = false;
                    // Store both the expression for code generation
                    result.array_size_expr = std::make_unique<expr>(build_expr(arr_fixed->size, analyzed, mono_ctx));
                } else {
                    // Constant not evaluated - treat as variable
                    result.kind = type_kind::array_variable;
                    result.array_size_expr = std::make_unique<expr>(build_expr(arr_fixed->size, analyzed, mono_ctx));
                    result.is_variable_size = true;
                }
            } else {
                // Not a constant - runtime expression (e.g., field reference)
                result.kind = type_kind::array_variable;
                result.array_size_expr = std::make_unique<expr>(build_expr(arr_fixed->size, analyzed, mono_ctx));
                result.is_variable_size = true;
            }
        }
        // Complex expression (e.g., arithmetic with constants like SIZE + 1)
        else {
            // Try to evaluate as compile-time constant expression
            std::vector<semantic::diagnostic> temp_diags;
            auto size_val = semantic::phases::evaluate_constant_uint(arr_fixed->size, analyzed, temp_diags);

            if (size_val) {
                // Successfully evaluated as compile-time constant - treat as fixed-size array
                result.kind = type_kind::array_fixed;
                result.array_size = *size_val;
                result.is_variable_size = false;
                // Also store expression for code generation
                result.array_size_expr = std::make_unique<expr>(build_expr(arr_fixed->size, analyzed, mono_ctx));
            } else {
                // Can't evaluate at compile-time - treat as variable-size array
                result.kind = type_kind::array_variable;
                result.array_size_expr = std::make_unique<expr>(build_expr(arr_fixed->size, analyzed, mono_ctx));
                result.is_variable_size = true;
            }
        }
    }
    else if (auto* arr_var = std::get_if<ast::array_type_unsized>(&ast_type.node)) {
        result.kind = type_kind::array_variable;
        result.element_type = std::make_unique<type_ref>(
            build_type_ref(*arr_var->element_type, analyzed, index_maps, mono_ctx));
    }
    else if (auto* arr_range = std::get_if<ast::array_type_range>(&ast_type.node)) {
        result.kind = type_kind::array_ranged;
        result.element_type = std::make_unique<type_ref>(
            build_type_ref(*arr_range->element_type, analyzed, index_maps, mono_ctx));

        // Build min expression (default to 0 if not specified)
        expr min_expr;
        if (arr_range->min_size.has_value()) {
            min_expr = build_expr(*arr_range->min_size, analyzed, mono_ctx);
        } else {
            min_expr.type = expr::literal_int;
            min_expr.int_value = 0;
        }

        // Build max expression
        expr max_expr = build_expr(arr_range->max_size, analyzed, mono_ctx);

        // Store min and max expressions
        result.min_size_expr = std::make_unique<expr>(expr::copy(min_expr));
        result.max_size_expr = std::make_unique<expr>(expr::copy(max_expr));

        // Array size is (max - min)
        // Create a binary subtraction expression
        expr size_expr;
        size_expr.type = expr::binary_op;
        size_expr.op = expr::sub;
        size_expr.left = std::make_unique<expr>(std::move(max_expr));
        size_expr.right = std::make_unique<expr>(std::move(min_expr));
        result.array_size_expr = std::make_unique<expr>(std::move(size_expr));

        // Ranged arrays are variable-size (use std::vector with bounds checking)
        result.is_variable_size = true;
    }
    else if (auto* qname = std::get_if<ast::qualified_name>(&ast_type.node)) {
        // Look up in resolved types
        auto resolved_it = analyzed.resolved_types.find(qname);
        if (resolved_it != analyzed.resolved_types.end()) {
            if (std::holds_alternative<const ast::struct_def*>(resolved_it->second)) {
                result.kind = type_kind::struct_type;
                auto* struct_ptr = std::get<const ast::struct_def*>(resolved_it->second);
                auto idx_it = index_maps.struct_indices.find(struct_ptr);
                if (idx_it != index_maps.struct_indices.end()) {
                    result.type_index = idx_it->second;
                } else {
                    result.type_index = 0;  // Fallback
                }
            }
            else if (std::holds_alternative<const ast::enum_def*>(resolved_it->second)) {
                result.kind = type_kind::enum_type;
                auto* enum_ptr = std::get<const ast::enum_def*>(resolved_it->second);
                auto idx_it = index_maps.enum_indices.find(enum_ptr);
                if (idx_it != index_maps.enum_indices.end()) {
                    result.type_index = idx_it->second;
                } else {
                    result.type_index = 0;  // Fallback
                }
            }
            else if (std::holds_alternative<const ast::union_def*>(resolved_it->second)) {
                result.kind = type_kind::union_type;
                auto* union_ptr = std::get<const ast::union_def*>(resolved_it->second);
                auto idx_it = index_maps.union_indices.find(union_ptr);
                if (idx_it != index_maps.union_indices.end()) {
                    result.type_index = idx_it->second;
                } else {
                    result.type_index = 0;  // Fallback
                }
            }
            else if (std::holds_alternative<const ast::choice_def*>(resolved_it->second)) {
                result.kind = type_kind::choice_type;
                auto* choice_ptr = std::get<const ast::choice_def*>(resolved_it->second);
                auto idx_it = index_maps.choice_indices.find(choice_ptr);
                if (idx_it != index_maps.choice_indices.end()) {
                    result.type_index = idx_it->second;
                } else {
                    result.type_index = 0;  // Fallback
                }
            }
            else if (std::holds_alternative<const ast::subtype_def*>(resolved_it->second)) {
                result.kind = type_kind::subtype_ref;
                auto* subtype_ptr = std::get<const ast::subtype_def*>(resolved_it->second);
                auto idx_it = index_maps.subtype_indices.find(subtype_ptr);
                if (idx_it != index_maps.subtype_indices.end()) {
                    result.type_index = idx_it->second;
                } else {
                    result.type_index = 0;  // Fallback
                }
            }
            else if (std::holds_alternative<const ast::type_alias_def*>(resolved_it->second)) {
                // Type alias: recursively resolve to target type
                auto* alias_ptr = std::get<const ast::type_alias_def*>(resolved_it->second);
                return build_type_ref(alias_ptr->target_type, analyzed, index_maps, mono_ctx);
            }
        }
    }

    return result;
}

// ============================================================================
// Constraint Building
// ============================================================================

constraint_def build_constraint(const ast::constraint_def& ast_constraint,
                               const semantic::analyzed_module_set& analyzed,
                               const type_index_maps& index_maps,
                               monomorphization_context* mono_ctx) {
    constraint_def result;
    result.name = ast_constraint.name;
    result.source = source_location::from_ast(ast_constraint.pos);

    // Build parameters
    for (const auto& param : ast_constraint.params) {
        constraint_def::parameter ir_param;
        ir_param.name = param.name;
        ir_param.type = build_type_ref(param.param_type, analyzed, index_maps, mono_ctx);
        ir_param.source = source_location::from_ast(param.pos);
        result.params.push_back(std::move(ir_param));
    }

    // Build condition expression
    result.condition = build_expr(ast_constraint.condition, analyzed, mono_ctx);

    // Generate error message template
    result.error_message_template = "Constraint '" + result.name + "' violated";

    return result;
}

// ============================================================================
// Field Building
// ============================================================================

field build_field(const ast::field_def& ast_field,
                 const semantic::analyzed_module_set& analyzed,
                 const type_index_maps& index_maps,
                 monomorphization_context* mono_ctx) {
    field result;
    result.name = ast_field.name;
    result.type = build_type_ref(ast_field.field_type, analyzed, index_maps, mono_ctx);
    result.source = source_location::from_ast(ast_field.pos);

    // Get offset from semantic analysis
    auto offset_it = analyzed.field_offsets.find(&ast_field);
    if (offset_it != analyzed.field_offsets.end()) {
        result.offset = offset_it->second;
    }

    // Handle inline constraint (field: expr)
    if (ast_field.constraint) {
        result.inline_constraint = build_expr(ast_field.constraint.value(), analyzed, mono_ctx);
    }

    // Handle default value (field = expr)
    if (ast_field.default_value) {
        result.default_value = build_expr(ast_field.default_value.value(), analyzed, mono_ctx);
    }

    // Handle condition
    if (ast_field.condition) {
        result.condition = field::runtime;
        result.runtime_condition = build_expr(ast_field.condition.value(), analyzed, mono_ctx);
    } else {
        result.condition = field::always;
    }

    // Documentation
    if (ast_field.docstring) {
        result.documentation = ast_field.docstring.value();
    }

    return result;
}

// ============================================================================
// Type Definitions
// ============================================================================

struct_def build_struct(const ast::struct_def& ast_struct,
                       const semantic::analyzed_module_set& analyzed,
                       const type_index_maps& index_maps,
                       monomorphization_context* mono_ctx) {
    struct_def result;
    result.name = ast_struct.name;
    result.source = source_location::from_ast(ast_struct.pos);

    // Build fields with labels and alignment directives
    std::optional<expr> pending_label = std::nullopt;
    std::optional<uint64_t> pending_alignment = std::nullopt;

    for (const auto& body_item : ast_struct.body) {
        if (auto* ast_label = std::get_if<ast::label_directive>(&body_item)) {
            // Label directive: evaluate expression for position
            pending_label = build_expr(ast_label->label_expr, analyzed, mono_ctx);
        }
        else if (auto* ast_align = std::get_if<ast::alignment_directive>(&body_item)) {
            // Alignment directive: evaluate expression (must be constant)
            auto align_expr = build_expr(ast_align->alignment_expr, analyzed, mono_ctx);
            // Try to extract constant value
            if (align_expr.type == expr::literal_int) {
                pending_alignment = align_expr.int_value;
            }
            // Note: Non-constant alignments are validated in Phase 4 (constant evaluation)
            // and will produce a compile error, so we never reach here with invalid alignments.
        }
        else if (auto* ast_field = std::get_if<ast::field_def>(&body_item)) {
            // Field: build it and apply pending directives
            auto ir_field = build_field(*ast_field, analyzed, index_maps, mono_ctx);

            // Apply pending label if any
            if (pending_label.has_value()) {
                ir_field.label = std::move(*pending_label);
                pending_label.reset();
            }

            // Apply pending alignment if any
            if (pending_alignment.has_value()) {
                ir_field.alignment = *pending_alignment;
                pending_alignment.reset();
            }

            result.fields.push_back(std::move(ir_field));
        }
        else if (auto* ast_func = std::get_if<ast::function_def>(&body_item)) {
            // Function: build it
            function_def ir_func;
            ir_func.name = ast_func->name;
            ir_func.source = source_location::from_ast(ast_func->pos);
            ir_func.return_type = build_type_ref(ast_func->return_type, analyzed, index_maps, mono_ctx);

            // Build parameters
            for (const auto& ast_param : ast_func->parameters) {
                function_param ir_param;
                ir_param.name = ast_param.name;
                ir_param.param_type = build_type_ref(ast_param.param_type, analyzed, index_maps, mono_ctx);
                ir_param.source = source_location::from_ast(ast_param.pos);
                ir_func.parameters.push_back(std::move(ir_param));
            }

            // Build function body (statements)
            for (const auto& ast_stmt : ast_func->body) {
                if (auto* ret_stmt = std::get_if<ast::return_statement>(&ast_stmt)) {
                    return_statement ir_ret;
                    ir_ret.value = build_expr(ret_stmt->value, analyzed, mono_ctx);
                    ir_ret.source = source_location::from_ast(ret_stmt->pos);
                    ir_func.body.emplace_back(std::move(ir_ret));
                } else if (auto* expr_stmt = std::get_if<ast::expression_statement>(&ast_stmt)) {
                    expression_statement ir_expr;
                    ir_expr.expression = build_expr(expr_stmt->expression, analyzed, mono_ctx);
                    ir_expr.source = source_location::from_ast(expr_stmt->pos);
                    ir_func.body.emplace_back(std::move(ir_expr));
                }
            }

            if (ast_func->docstring) {
                ir_func.documentation = ast_func->docstring.value();
            }

            result.functions.push_back(std::move(ir_func));
        }
    }

    // Calculate total size and alignment from fields
    result.total_size = calculate_struct_size(result.fields);
    result.alignment = calculate_struct_alignment(result.fields);

    if (ast_struct.docstring) {
        result.documentation = ast_struct.docstring.value();
    }

    return result;
}

enum_def build_enum(const ast::enum_def& ast_enum,
                   const semantic::analyzed_module_set& analyzed,
                   const type_index_maps& index_maps,
                   monomorphization_context* mono_ctx) {
    enum_def result;
    result.name = ast_enum.name;
    result.source = source_location::from_ast(ast_enum.pos);

    result.base_type = build_type_ref(ast_enum.base_type, analyzed, index_maps, mono_ctx);
    result.is_bitmask = ast_enum.is_bitmask;

    // Build items
    for (const auto& ast_item : ast_enum.items) {
        enum_def::item ir_item;
        ir_item.name = ast_item.name;
        ir_item.source = source_location::from_ast(ast_item.pos);

        // Get evaluated value from semantic analysis
        auto it = analyzed.enum_item_values.find(&ast_item);
        if (it != analyzed.enum_item_values.end()) {
            ir_item.value = it->second;
        } else {
            // Fallback: should not happen if semantic analysis completed successfully
            ir_item.value = 0;
        }

        if (ast_item.docstring) {
            ir_item.documentation = ast_item.docstring.value();
        }

        result.items.push_back(std::move(ir_item));
    }

    if (ast_enum.docstring) {
        result.documentation = ast_enum.docstring.value();
    }

    return result;
}

subtype_def build_subtype(const ast::subtype_def& ast_subtype,
                         const semantic::analyzed_module_set& analyzed,
                         const type_index_maps& index_maps,
                         monomorphization_context* mono_ctx) {
    subtype_def result;
    result.name = ast_subtype.name;
    result.source = source_location::from_ast(ast_subtype.pos);

    result.base_type = build_type_ref(ast_subtype.base_type, analyzed, index_maps, mono_ctx);
    result.constraint = build_expr(ast_subtype.constraint, analyzed, mono_ctx);

    if (ast_subtype.docstring) {
        result.documentation = ast_subtype.docstring.value();
    }

    return result;
}

union_def build_union(const ast::union_def& ast_union,
                     const semantic::analyzed_module_set& analyzed,
                     const type_index_maps& index_maps,
                     monomorphization_context* mono_ctx) {
    union_def result;
    result.name = ast_union.name;
    result.source = source_location::from_ast(ast_union.pos);

    // Build cases
    for (const auto& ast_case : ast_union.cases) {
        union_case ir_case;
        ir_case.case_name = ast_case.case_name;
        ir_case.source = source_location::from_ast(ast_case.pos);
        ir_case.is_anonymous_block = ast_case.is_anonymous_block;

        // Build fields for this case
        for (const auto& item : ast_case.items) {
            if (std::holds_alternative<ast::field_def>(item)) {
                const auto& ast_field = std::get<ast::field_def>(item);
                ir_case.fields.push_back(build_field(ast_field, analyzed, index_maps, mono_ctx));
            }
        }

        // Build condition if present
        if (ast_case.condition) {
            ir_case.condition = build_expr(ast_case.condition.value(), analyzed, mono_ctx);
        }

        if (ast_case.docstring) {
            ir_case.documentation = ast_case.docstring.value();
        }

        // If this is an anonymous block with multiple fields, generate a wrapper struct
        if (ir_case.is_anonymous_block && ir_case.fields.size() > 1 && mono_ctx) {
            std::string wrapper_name = result.name + "__" + ir_case.case_name + "__block";

            struct_def wrapper;
            wrapper.name = wrapper_name;
            wrapper.source = ir_case.source;

            // Move all fields from union case to wrapper struct
            wrapper.fields = std::move(ir_case.fields);
            wrapper.documentation = ir_case.documentation;

            // Calculate size and alignment for the wrapper struct
            wrapper.total_size = calculate_struct_size(wrapper.fields);
            wrapper.alignment = calculate_struct_alignment(wrapper.fields);

            // Add wrapper struct to context
            mono_ctx->wrapper_structs.push_back(std::move(wrapper));

            // Replace the multiple fields in the union case with a single field
            // that references the wrapper struct
            field wrapper_field;
            wrapper_field.name = ir_case.case_name;
            wrapper_field.source = ir_case.source;
            wrapper_field.type.kind = type_kind::struct_type;
            // type_index will be set after topological sorting when all structs are assembled
            wrapper_field.documentation = ir_case.documentation;

            ir_case.fields.clear();
            ir_case.fields.push_back(std::move(wrapper_field));
        }

        result.cases.push_back(std::move(ir_case));
    }

    // Calculate size and alignment from cases
    result.size = calculate_union_size(result.cases);
    result.alignment = calculate_union_alignment(result.cases);

    if (ast_union.docstring) {
        result.documentation = ast_union.docstring.value();
    }

    return result;
}

choice_def build_choice(const ast::choice_def& ast_choice,
                       const semantic::analyzed_module_set& analyzed,
                       const type_index_maps& index_maps,
                       monomorphization_context* mono_ctx) {
    choice_def result;
    result.name = ast_choice.name;
    result.source = source_location::from_ast(ast_choice.pos);

    // Build parameters
    for (const auto& ast_param : ast_choice.parameters) {
        choice_def::param param_result;
        param_result.name = ast_param.name;
        param_result.type = build_type_ref(ast_param.param_type, analyzed, index_maps, mono_ctx);
        param_result.source = source_location::from_ast(ast_param.pos);
        result.parameters.push_back(std::move(param_result));
    }

    // Build selector expression (optional for inline discriminator)
    if (ast_choice.selector.has_value()) {
        result.selector = build_expr(ast_choice.selector.value(), analyzed, mono_ctx);
    } else {
        // Inline discriminator: get explicit validated type from semantic analysis
        auto disc_type_it = analyzed.choice_discriminator_types.find(&ast_choice);
        if (disc_type_it != analyzed.choice_discriminator_types.end()) {
            // Wrap primitive_type in ast::type for build_type_ref
            ast::type wrapped_type{disc_type_it->second};
            result.inferred_discriminator_type = build_type_ref(wrapped_type, analyzed, index_maps, mono_ctx);
        }
    }

    // Build cases
    for (const auto& ast_case : ast_choice.cases) {
        choice_def::case_def case_result;
        case_result.source = source_location::from_ast(ast_case.pos);

        // Set selector mode (exact match vs range comparison)
        case_result.selector_mode = ast_selector_kind_to_ir(ast_case.selector_kind);

        // Build case values (expressions that match this case) - for exact matches
        for (const auto& case_expr : ast_case.case_exprs) {
            case_result.case_values.push_back(build_expr(case_expr, analyzed, mono_ctx));
        }

        // Build range bound expression - for range-based selectors (>= 0x80, etc.)
        if (ast_case.range_bound.has_value()) {
            case_result.range_bound = build_expr(ast_case.range_bound.value(), analyzed, mono_ctx);
        }

        // Build the field for this case
        // After desugaring, items should contain a single field_def
        if (!ast_case.items.empty() && std::holds_alternative<ast::field_def>(ast_case.items[0])) {
            const auto& field_def = std::get<ast::field_def>(ast_case.items[0]);
            case_result.case_field = build_field(field_def, analyzed, index_maps, mono_ctx);
        }

        // Copy the is_anonymous_block flag from AST to IR
        // This is needed to know if the case was defined with inline struct syntax { ... } name;
        // For inline struct cases with inline discriminators, we need to restore the read position
        // so the struct can re-read the discriminator as its first field
        case_result.is_anonymous_block = ast_case.is_anonymous_block;

        result.cases.push_back(std::move(case_result));
    }

    // Calculate size and alignment (max of all case fields plus tag)
    result.size = calculate_choice_size(result.cases);
    result.alignment = calculate_choice_alignment(result.cases);

    if (ast_choice.docstring) {
        result.documentation = ast_choice.docstring.value();
    }

    return result;
}

// ============================================================================
// Topological Sort: Order types by dependencies
// ============================================================================

/**
 * Build dependency graph for structs, unions, and choices together.
 * Returns map: type_index -> set of type indices it depends on.
 *
 * Index scheme:
 * - structs: [0, num_structs)
 * - unions: [num_structs, num_structs + num_unions)
 * - choices: [num_structs + num_unions, num_structs + num_unions + num_choices)
 */
std::map<size_t, std::set<size_t>> build_type_dependency_graph(
    const std::vector<struct_def>& structs,
    const std::vector<union_def>& unions,
    const std::vector<choice_def>& choices
) {
    std::map<size_t, std::set<size_t>> deps;
    size_t num_structs = structs.size();
    size_t num_unions = unions.size();

    // Helper to check dependencies in a type_ref (recursive)
    std::function<void(const type_ref&, size_t)> add_type_dependencies = [&](const type_ref& type, size_t from_index) {
        if (type.kind == type_kind::struct_type && type.type_index) {
            size_t dep_index = *type.type_index;
            if (dep_index < num_structs && dep_index != from_index) {
                deps[from_index].insert(dep_index);
            }
        } else if (type.kind == type_kind::union_type && type.type_index) {
            // Unions are indexed after structs
            size_t dep_index = num_structs + *type.type_index;
            if (dep_index != from_index) {
                deps[from_index].insert(dep_index);
            }
        } else if (type.kind == type_kind::choice_type && type.type_index) {
            // Choices are indexed after structs and unions
            size_t dep_index = num_structs + num_unions + *type.type_index;
            if (dep_index != from_index) {
                deps[from_index].insert(dep_index);
            }
        }
        // Recursively check array element types
        if (type.element_type) {
            add_type_dependencies(*type.element_type, from_index);
        }
    };

    // Process struct dependencies
    for (size_t i = 0; i < structs.size(); i++) {
        deps[i] = {};  // Initialize empty set for each struct

        for (const auto& field : structs[i].fields) {
            add_type_dependencies(field.type, i);
        }
    }

    // Process union dependencies
    for (size_t i = 0; i < unions.size(); i++) {
        size_t union_index = num_structs + i;
        deps[union_index] = {};  // Initialize empty set for each union

        for (const auto& union_case : unions[i].cases) {
            for (const auto& field : union_case.fields) {
                add_type_dependencies(field.type, union_index);
            }
        }
    }

    // Process choice dependencies
    for (size_t i = 0; i < choices.size(); i++) {
        size_t choice_index = num_structs + num_unions + i;
        deps[choice_index] = {};  // Initialize empty set for each choice

        for (const auto& case_def : choices[i].cases) {
            add_type_dependencies(case_def.case_field.type, choice_index);
        }
    }

    return deps;
}

/**
 * Topological sort using DFS for structs, unions, and choices together.
 * Returns sorted indices (dependencies first).
 *
 * Index scheme:
 * - structs: [0, num_structs)
 * - unions: [num_structs, num_structs + num_unions)
 * - choices: [num_structs + num_unions, num_structs + num_unions + num_choices)
 */
std::vector<size_t> topological_sort_types(
    const std::vector<struct_def>& structs,
    const std::vector<union_def>& unions,
    const std::vector<choice_def>& choices
) {
    if (structs.empty() && unions.empty() && choices.empty()) {
        return {};
    }

    size_t num_structs = structs.size();
    size_t num_unions = unions.size();
    size_t total_types = num_structs + num_unions + choices.size();

    auto deps = build_type_dependency_graph(structs, unions, choices);

    std::vector<size_t> result;
    std::set<size_t> visited;
    std::set<size_t> in_progress;  // For cycle detection

    // DFS visit function
    std::function<void(size_t)> visit = [&](size_t node) {
        if (visited.count(node)) {
            return;  // Already processed
        }

        if (in_progress.count(node)) {
            // Circular dependency detected - this is OK for C++ with forward declarations
            // Just skip to avoid infinite recursion
            return;
        }

        in_progress.insert(node);

        // Visit all dependencies first
        for (size_t dep : deps[node]) {
            visit(dep);
        }

        in_progress.erase(node);
        visited.insert(node);
        result.push_back(node);  // Add to result after all dependencies
    };

    // Visit all nodes (structs and choices)
    for (size_t i = 0; i < total_types; i++) {
        visit(i);
    }

    return result;
}

/**
 * Reorder a vector according to given indices.
 * Creates a new vector with elements in the order specified by indices.
 * Uses move semantics to handle non-copyable types.
 */
template<typename T>
std::vector<T> reorder_vector(std::vector<T>& vec, const std::vector<size_t>& indices) {
    std::vector<T> result;
    result.reserve(indices.size());

    for (size_t idx : indices) {
        if (idx < vec.size()) {
            result.push_back(std::move(vec[idx]));
        }
    }

    return result;
}

} // anonymous namespace

// ============================================================================
// Public API
// ============================================================================

bundle build_ir(const semantic::analyzed_module_set& analyzed) {
    bundle result;

    const auto& ast_module = analyzed.original->main.module;

    result.name = analyzed.original->main.package_name;
    result.source = source_location::from_ast({analyzed.original->main.file_path, 1, 1});

    // Metadata
    result.meta.datascript_version = "1.0.0";
    result.meta.generator_version = "0.1.0";
    result.meta.generated_at = get_iso8601_timestamp();
    result.meta.source_files.push_back(analyzed.original->main.file_path);

    // Build type index maps first (map AST pointers to their IR indices)
    type_index_maps index_maps;

    // Only index non-parameterized types initially
    for (size_t i = 0; i < ast_module.structs.size(); i++) {
        if (ast_module.structs[i].parameters.empty()) {
            index_maps.struct_indices[&ast_module.structs[i]] = i;
        }
    }

    for (size_t i = 0; i < ast_module.enums.size(); i++) {
        index_maps.enum_indices[&ast_module.enums[i]] = i;
    }

    for (size_t i = 0; i < ast_module.unions.size(); i++) {
        if (ast_module.unions[i].parameters.empty()) {
            index_maps.union_indices[&ast_module.unions[i]] = i;
        }
    }

    for (size_t i = 0; i < ast_module.choices.size(); i++) {
        index_maps.choice_indices[&ast_module.choices[i]] = i;
    }

    for (size_t i = 0; i < ast_module.subtypes.size(); i++) {
        index_maps.subtype_indices[&ast_module.subtypes[i]] = i;
    }

    // Create monomorphization context
    monomorphization_context mono_ctx;
    mono_ctx.analyzed = &analyzed;
    mono_ctx.index_maps = &index_maps;

    // Build constraints
    for (const auto& ast_constraint : ast_module.constraints) {
        result.constraints.push_back(build_constraint(ast_constraint, analyzed, index_maps, &mono_ctx));
    }

    // Build non-parameterized structs first (this triggers monomorphization of referenced types)
    // Store them temporarily so we can add monomorphized structs first
    std::vector<struct_def> non_param_structs;
    for (const ast::struct_def& ast_struct : ast_module.structs) {
        if (ast_struct.parameters.empty()) {
            non_param_structs.push_back(build_struct(ast_struct, analyzed, index_maps, &mono_ctx));
        }
    }

    // Now add concrete monomorphized structs to result.structs FIRST
    // This way, indices from monomorphize_struct() (which are 0-based in mono_ctx.concrete_structs)
    // will correctly point to positions in result.structs
    for (auto& concrete_struct : mono_ctx.concrete_structs) {
        result.structs.push_back(std::move(concrete_struct));
    }

    // Then add non-parameterized structs AFTER monomorphized ones
    for (auto& non_param : non_param_structs) {
        result.structs.push_back(std::move(non_param));
    }

    // Build enums
    for (const auto& ast_enum : ast_module.enums) {
        result.enums.push_back(build_enum(ast_enum, analyzed, index_maps, &mono_ctx));
    }

    // Build subtypes
    for (const auto& ast_subtype : ast_module.subtypes) {
        result.subtypes.push_back(build_subtype(ast_subtype, analyzed, index_maps, &mono_ctx));
    }

    // Build non-parameterized unions first (this triggers monomorphization)
    std::vector<union_def> non_param_unions;
    for (const auto& ast_union : ast_module.unions) {
        if (ast_union.parameters.empty()) {
            non_param_unions.push_back(build_union(ast_union, analyzed, index_maps, &mono_ctx));
        }
    }

    // Add concrete monomorphized unions to result.unions FIRST
    for (auto& concrete_union : mono_ctx.concrete_unions) {
        result.unions.push_back(std::move(concrete_union));
    }

    // Then add non-parameterized unions AFTER monomorphized ones
    for (auto& non_param : non_param_unions) {
        result.unions.push_back(std::move(non_param));
    }

    // Add wrapper structs generated for anonymous union case blocks to result.structs
    // These need to be added BEFORE topological sorting so they can be properly ordered
    for (auto& wrapper : mono_ctx.wrapper_structs) {
        result.structs.push_back(std::move(wrapper));
    }

    // Build a name-to-index map for all structs (including wrappers) for type resolution
    std::map<std::string, size_t> struct_name_to_index;
    for (size_t i = 0; i < result.structs.size(); i++) {
        struct_name_to_index[result.structs[i].name] = i;
    }

    // Resolve wrapper struct type indices in union cases
    // Wrapper fields have kind==struct_type but type_index is unset, we need to resolve by name
    for (auto& union_def : result.unions) {
        for (auto& union_case : union_def.cases) {
            for (auto& field : union_case.fields) {
                if (field.type.kind == type_kind::struct_type && !field.type.type_index.has_value()) {
                    // Wrapper struct name pattern: UnionName__case_name__block
                    // Construct expected wrapper name
                    std::string expected_wrapper_name = union_def.name + "__" + field.name + "__block";
                    auto it = struct_name_to_index.find(expected_wrapper_name);
                    if (it != struct_name_to_index.end()) {
                        field.type.type_index = it->second;
                    }
                }
            }
        }
    }

    // Build choices
    for (const auto& ast_choice : ast_module.choices) {
        result.choices.push_back(build_choice(ast_choice, analyzed, index_maps, &mono_ctx));
    }

    // Build constants map
    for (const auto& ast_const : ast_module.constants) {
        // Get evaluated value from semantic analysis
        auto it = analyzed.constant_values.find(&ast_const);
        if (it != analyzed.constant_values.end()) {
            result.constants[ast_const.name] = it->second;
        }
    }

    // Topologically sort structs by dependencies to fix forward reference issues
    // This ensures that if struct A contains field of type B, then B is declared before A
    // NOTE: We need to update type indices after monomorphization adds new structs
    // Rebuild the index map for all structs (including monomorphized ones)
    for (size_t i = 0; i < result.structs.size(); i++) {
        // Update type indices in fields to reflect actual position in result.structs
        for (auto& field : result.structs[i].fields) {
            if (field.type.kind == type_kind::struct_type && field.type.type_index) {
                // Type index should already be correct from IR builder
                // Just verify it's in range
                if (*field.type.type_index >= result.structs.size()) {
                    // Invalid index - this is a bug in monomorphization
                    field.type.type_index = std::nullopt;
                }
            }
        }
    }

    // Topologically sort structs, unions, and choices together
    size_t num_structs = result.structs.size();
    size_t num_unions = result.unions.size();
    size_t num_choices = result.choices.size();

    auto sorted_indices = topological_sort_types(result.structs, result.unions, result.choices);

    // Split sorted indices into struct, union, and choice indices
    // Also build the emission order tracking
    // type_emission_order uses: 0=struct, 1=union, 2=choice (encoded as {type_kind, index})
    std::vector<size_t> sorted_struct_indices;
    std::vector<size_t> sorted_union_indices;
    std::vector<size_t> sorted_choice_indices;
    for (size_t idx : sorted_indices) {
        if (idx < num_structs) {
            size_t struct_idx = idx;
            // Record emission order: 0 = struct, index into future reordered struct vector
            result.type_emission_order.emplace_back(0, sorted_struct_indices.size());
            sorted_struct_indices.push_back(struct_idx);
        } else if (idx < num_structs + num_unions) {
            size_t union_idx = idx - num_structs;
            // Record emission order: 1 = union, index into future reordered union vector
            result.type_emission_order.emplace_back(1, sorted_union_indices.size());
            sorted_union_indices.push_back(union_idx);
        } else {
            size_t choice_idx = idx - num_structs - num_unions;
            // Record emission order: 2 = choice, index into future reordered choice vector
            result.type_emission_order.emplace_back(2, sorted_choice_indices.size());
            sorted_choice_indices.push_back(choice_idx);
        }
    }

    // Reorder structs, unions, and choices
    result.structs = reorder_vector(result.structs, sorted_struct_indices);
    result.unions = reorder_vector(result.unions, sorted_union_indices);
    result.choices = reorder_vector(result.choices, sorted_choice_indices);

    // IMPORTANT: After reordering, we need to update all type_index values
    // Create reverse mappings: old_index -> new_index
    std::vector<size_t> struct_index_mapping(num_structs);
    for (size_t new_idx = 0; new_idx < sorted_struct_indices.size(); ++new_idx) {
        size_t old_idx = sorted_struct_indices[new_idx];
        struct_index_mapping[old_idx] = new_idx;
    }

    std::vector<size_t> union_index_mapping(num_unions);
    for (size_t new_idx = 0; new_idx < sorted_union_indices.size(); ++new_idx) {
        size_t old_idx = sorted_union_indices[new_idx];
        union_index_mapping[old_idx] = new_idx;
    }

    std::vector<size_t> choice_index_mapping(num_choices);
    for (size_t new_idx = 0; new_idx < sorted_choice_indices.size(); ++new_idx) {
        size_t old_idx = sorted_choice_indices[new_idx];
        choice_index_mapping[old_idx] = new_idx;
    }

    // Update all type indices throughout the module (struct, union, and choice)
    auto update_type_index = [&struct_index_mapping, &union_index_mapping, &choice_index_mapping](type_ref& type) {
        auto update_recursive = [&struct_index_mapping, &union_index_mapping, &choice_index_mapping](type_ref& t, auto& self_ref) -> void {
            // Update array element types
            if (t.element_type) {
                self_ref(*t.element_type, self_ref);
            }

            // Update struct type index if present
            if (t.kind == type_kind::struct_type && t.type_index.has_value()) {
                if (const size_t old_idx = *t.type_index; old_idx < struct_index_mapping.size()) {
                    t.type_index = struct_index_mapping[old_idx];
                }
            }

            // Update union type index if present
            if (t.kind == type_kind::union_type && t.type_index.has_value()) {
                if (const size_t old_idx = *t.type_index; old_idx < union_index_mapping.size()) {
                    t.type_index = union_index_mapping[old_idx];
                }
            }

            // Update choice type index if present
            if (t.kind == type_kind::choice_type && t.type_index.has_value()) {
                if (const size_t old_idx = *t.type_index; old_idx < choice_index_mapping.size()) {
                    t.type_index = choice_index_mapping[old_idx];
                }
            }
        };
        update_recursive(type, update_recursive);
    };

    // Update indices in all structs
    for (auto& struct_def : result.structs) {
        for (auto& field : struct_def.fields) {
            update_type_index(field.type);
        }
        for (auto& func : struct_def.functions) {
            update_type_index(func.return_type);
            for (auto& param : func.parameters) {
                update_type_index(param.param_type);
            }
        }
    }

    // Update indices in all unions
    for (auto& union_def : result.unions) {
        for (auto& union_case : union_def.cases) {
            for (auto& field : union_case.fields) {
                update_type_index(field.type);
            }
        }
    }

    // Update indices in all choices
    for (auto& choice_def : result.choices) {
        for (auto& choice_case : choice_def.cases) {
            update_type_index(choice_case.case_field.type);
        }
    }

    // ===========================================================================
    // Post-Processing: Calculate sizes and alignments for all types
    // ===========================================================================
    // Now that all types are built and indices are finalized, we can accurately
    // calculate sizes and alignments for nested types, arrays, and complex structures.
    calculate_all_sizes_and_alignments(result);

    return result;
}

} // namespace datascript::ir

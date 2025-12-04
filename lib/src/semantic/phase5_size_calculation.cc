//
// Phase 5: Size and Layout Calculation
//
// Calculates sizes of types, field offsets in structs, and alignment requirements.
//

#include <datascript/semantic.hh>
#include <algorithm>
#include <limits>

namespace datascript::semantic::phases {

namespace {
    // Helper: add error diagnostic
    void add_error(std::vector<diagnostic>& diags,
                  const char* code,
                  const std::string& message,
                  const ast::source_pos& pos) {
        diags.push_back(diagnostic{
            diagnostic_level::error,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });
    }

    // Helper: add warning diagnostic
    void add_warning(std::vector<diagnostic>& diags,
                    const char* code,
                    const std::string& message,
                    const ast::source_pos& pos) {
        diags.push_back(diagnostic{
            diagnostic_level::warning,
            code,
            message,
            pos,
            std::nullopt,
            std::nullopt,
            std::nullopt
        });
    }

    // Helper: align offset to alignment requirement
    size_t align_offset(size_t offset, size_t alignment) {
        if (alignment == 0) return offset;
        return (offset + alignment - 1) / alignment * alignment;
    }

    // ========================================================================
    // Type Size Calculation
    // ========================================================================

    // Forward declaration
    type_info calculate_type_info(
        const ast::type& type_node,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags);

    // Calculate primitive type size
    type_info calculate_primitive_type_info(const ast::primitive_type& prim) {
        type_info info;
        info.size = prim.bits / 8;  // Convert bits to bytes
        info.alignment = info.size;  // Primitives align to their size
        info.is_variable_size = false;
        info.is_signed = prim.is_signed;
        return info;
    }

    // Calculate bitfield type size
    type_info calculate_bitfield_type_info(uint64_t bits) {
        type_info info;
        info.size = (bits + 7) / 8;  // Round up to bytes
        info.alignment = 1;  // Bitfields have byte alignment
        info.is_variable_size = false;
        info.is_signed = false;
        return info;
    }

    // Calculate string type size
    type_info calculate_string_type_info() {
        type_info info;
        info.size = std::numeric_limits<size_t>::max();  // Variable size
        info.alignment = 1;
        info.is_variable_size = true;
        return info;
    }

    // Calculate bool type size
    type_info calculate_bool_type_info() {
        type_info info;
        info.size = 1;  // 1 byte
        info.alignment = 1;
        info.is_variable_size = false;
        info.is_signed = false;
        return info;
    }

    // Calculate array type size
    type_info calculate_array_type_info(
        const ast::type& element_type,
        std::optional<size_t> fixed_size,
        std::optional<size_t> min_size,
        std::optional<size_t> max_size,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        auto element_info = calculate_type_info(element_type, analyzed, diags);

        type_info info;
        info.is_signed = false;
        info.alignment = element_info.alignment;

        if (fixed_size.has_value()) {
            // Fixed-size array: T[n]
            if (element_info.is_variable_size) {
                info.size = std::numeric_limits<size_t>::max();
                info.is_variable_size = true;
            } else {
                info.size = element_info.size * fixed_size.value();
                info.is_variable_size = false;
            }
        } else if (max_size.has_value()) {
            // Range array: T[min..max] or T[..max]
            info.size = std::numeric_limits<size_t>::max();
            info.is_variable_size = true;
            info.min_size = min_size.has_value() ?
                (element_info.size * min_size.value()) : 0;
            info.max_size = element_info.size * max_size.value();
        } else {
            // Unsized array: T[]
            info.size = std::numeric_limits<size_t>::max();
            info.is_variable_size = true;
        }

        return info;
    }

    // Calculate user-defined type size (struct, union, enum, choice)
    type_info calculate_user_defined_type_info(
        const ast::qualified_name& qname,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags,
        const ast::source_pos& pos)
    {
        // Look up the resolved type
        auto it = analyzed.resolved_types.find(&qname);
        if (it == analyzed.resolved_types.end()) {
            // Type not resolved - error already reported in Phase 2
            type_info info;
            info.size = 0;
            info.alignment = 1;
            info.is_variable_size = false;
            info.is_signed = false;
            return info;
        }

        auto& resolved = it->second;

        // Handle different type kinds
        if (auto* struct_def = std::get_if<const ast::struct_def*>(&resolved)) {
            // Struct size already calculated (or will be)
            // For now, return placeholder
            type_info info;
            info.size = 0;  // Will be calculated separately
            info.alignment = 1;
            info.is_variable_size = false;
            info.is_signed = false;
            return info;
        }
        else if (auto* enum_def = std::get_if<const ast::enum_def*>(&resolved)) {
            // Enum has same size as its base type
            return calculate_type_info((*enum_def)->base_type, analyzed, diags);
        }
        else if (auto* union_def = std::get_if<const ast::union_def*>(&resolved)) {
            // Union size = max of all field sizes
            type_info info;
            info.size = 0;  // Will be calculated separately
            info.alignment = 1;
            info.is_variable_size = false;
            info.is_signed = false;
            return info;
        }
        else if (auto* choice_def = std::get_if<const ast::choice_def*>(&resolved)) {
            // Choice size is variable (depends on selector)
            type_info info;
            info.size = std::numeric_limits<size_t>::max();
            info.alignment = 1;
            info.is_variable_size = true;
            info.is_signed = false;
            return info;
        }

        // Unknown type
        type_info info;
        info.size = 0;
        info.alignment = 1;
        info.is_variable_size = false;
        info.is_signed = false;
        return info;
    }

    // Main type info calculation
    type_info calculate_type_info(
        const ast::type& type_node,
        const analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        if (auto* prim = std::get_if<ast::primitive_type>(&type_node.node)) {
            return calculate_primitive_type_info(*prim);
        }
        else if (auto* bf = std::get_if<ast::bit_field_type_fixed>(&type_node.node)) {
            return calculate_bitfield_type_info(bf->width);
        }
        else if (auto* bf_expr = std::get_if<ast::bit_field_type_expr>(&type_node.node)) {
            // Evaluate expression to get width
            auto width_val = phases::evaluate_constant_uint(bf_expr->width_expr, analyzed, diags);
            if (width_val) {
                return calculate_bitfield_type_info(*width_val);
            } else {
                add_error(diags, diag_codes::E_INCOMPATIBLE_TYPES,
                    "Bitfield width must be a compile-time constant expression",
                    bf_expr->pos);
                return calculate_bitfield_type_info(8);  // Fallback to 1 byte
            }
        }
        else if (std::holds_alternative<ast::string_type>(type_node.node)) {
            return calculate_string_type_info();
        }
        else if (std::holds_alternative<ast::bool_type>(type_node.node)) {
            return calculate_bool_type_info();
        }
        else if (auto* arr = std::get_if<ast::array_type_fixed>(&type_node.node)) {
            // Try to evaluate size expression as a compile-time constant
            auto size_val = phases::evaluate_constant_uint(arr->size, analyzed, diags);
            if (size_val) {
                // Constant size: fixed-size array T[n]
                return calculate_array_type_info(*arr->element_type, *size_val,
                    std::nullopt, std::nullopt, analyzed, diags);
            } else {
                // Non-constant size (field reference): variable-size array T[count]
                return calculate_array_type_info(*arr->element_type, std::nullopt,
                    std::nullopt, std::nullopt, analyzed, diags);
            }
        }
        else if (auto* arr = std::get_if<ast::array_type_range>(&type_node.node)) {
            // Try to evaluate min/max as compile-time constants
            // If they're not constants (e.g., field references), that's OK - they'll be
            // validated at runtime. We just won't have compile-time size bounds.
            std::optional<uint64_t> min_val;
            if (arr->min_size) {
                min_val = phases::evaluate_constant_uint(*arr->min_size, analyzed, diags);
            }

            auto max_val = phases::evaluate_constant_uint(arr->max_size, analyzed, diags);

            return calculate_array_type_info(*arr->element_type, std::nullopt,
                min_val, max_val, analyzed, diags);
        }
        else if (auto* arr = std::get_if<ast::array_type_unsized>(&type_node.node)) {
            return calculate_array_type_info(*arr->element_type, std::nullopt,
                std::nullopt, std::nullopt, analyzed, diags);
        }
        else if (auto* qname = std::get_if<ast::qualified_name>(&type_node.node)) {
            return calculate_user_defined_type_info(*qname, analyzed, diags, qname->pos);
        }

        // Unknown type
        type_info info;
        info.size = 0;
        info.alignment = 1;
        info.is_variable_size = false;
        info.is_signed = false;
        return info;
    }

    // ========================================================================
    // Struct Layout Calculation
    // ========================================================================

    void calculate_struct_layout(
        const ast::struct_def& struct_def,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        size_t current_offset = 0;
        size_t max_alignment = 1;

        for (const auto& body_item : struct_def.body) {
            // Only process fields for size calculation
            if (auto* field = std::get_if<ast::field_def>(&body_item)) {
                auto field_info = calculate_type_info(field->field_type, analyzed, diags);

                // Align field to its alignment requirement
                current_offset = align_offset(current_offset, field_info.alignment);

                // Store field offset
                analyzed.field_offsets[field] = current_offset;

                // Track maximum alignment for struct
                max_alignment = std::max(max_alignment, field_info.alignment);

                // Advance offset
                if (!field_info.is_variable_size) {
                    current_offset += field_info.size;
                } else {
                    // Variable-size field - struct becomes variable-size
                    current_offset = std::numeric_limits<size_t>::max();
                    break;
                }
            }
        }

        // Align struct size to its alignment
        if (current_offset != std::numeric_limits<size_t>::max()) {
            current_offset = align_offset(current_offset, max_alignment);
        }

        // Create type info for the struct type
        // Note: We can't store this easily since ast::type contains unique_ptr
        // For now, we just calculate field offsets
    }

    // ========================================================================
    // Union Layout Calculation
    // ========================================================================

    void calculate_union_layout(
        const ast::union_def& union_def,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        size_t max_size = 0;
        size_t max_alignment = 1;

        for (const auto& union_case : union_def.cases) {
            for (const auto& item : union_case.items) {
                if (std::holds_alternative<ast::field_def>(item)) {
                    const auto& field = std::get<ast::field_def>(item);
                    auto field_info = calculate_type_info(field.field_type, analyzed, diags);

                    // All union fields start at offset 0
                    analyzed.field_offsets[&field] = 0;

                    // Track maximum size and alignment
                    if (!field_info.is_variable_size) {
                        max_size = std::max(max_size, field_info.size);
                    }
                    max_alignment = std::max(max_alignment, field_info.alignment);
                }
            }
        }

        // Union size is max of all fields, aligned
        max_size = align_offset(max_size, max_alignment);
    }

    // ========================================================================
    // Module-Level Size Calculation
    // ========================================================================

    void calculate_module_sizes(
        const ast::module& mod,
        analyzed_module_set& analyzed,
        std::vector<diagnostic>& diags)
    {
        // Calculate sizes for all structs
        for (const auto& struct_def : mod.structs) {
            calculate_struct_layout(struct_def, analyzed, diags);
        }

        // Calculate sizes for all unions
        for (const auto& union_def : mod.unions) {
            calculate_union_layout(union_def, analyzed, diags);
        }

        // Enums inherit size from base type (already handled in calculate_type_info)

        // Choices are variable-size (already handled in calculate_type_info)
    }

} // anonymous namespace

// ============================================================================
// Public API: Size and Layout Calculation
// ============================================================================

void calculate_sizes(
    const module_set& modules,
    analyzed_module_set& analyzed,
    std::vector<diagnostic>& diagnostics)
{
    // Calculate sizes in main module
    calculate_module_sizes(modules.main.module, analyzed, diagnostics);

    // Calculate sizes in all imported modules
    for (const auto& imported : modules.imported) {
        calculate_module_sizes(imported.module, analyzed, diagnostics);
    }
}

} // namespace datascript::semantic::phases

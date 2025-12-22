#include <datascript/ksy/ksy_to_ir_builder.hh>
#include <fstream>
#include <sstream>

namespace datascript::ksy {

KsyToIrBuilder::KsyToIrBuilder() = default;

KsyToIrBuilder::~KsyToIrBuilder() = default;

ir::bundle KsyToIrBuilder::build_from_file(const std::string& ksy_path) {
    // Read .ksy file
    std::ifstream file(ksy_path);
    if (!file.is_open()) {
        throw std::runtime_error("Failed to open file: " + ksy_path);
    }

    // Parse YAML
    fkyaml::node root;
    try {
        root = fkyaml::node::deserialize(file);
    } catch (const std::exception& e) {
        throw std::runtime_error("Failed to parse YAML: " + std::string(e.what()));
    }

    return build_from_yaml(root);
}

ir::bundle KsyToIrBuilder::build_from_yaml(const fkyaml::node& root) {
    // Reset bundle
    bundle_ = ir::bundle{};
    module_name_ = "unknown";
    default_endianness_ = ir::endianness::little;

    // Process sections in order
    if (root.contains("meta")) {
        parse_meta(root["meta"]);
    }

    // Set bundle name
    bundle_.name = module_name_;

    // Parse enums first (they may be referenced by fields)
    if (root.contains("enums")) {
        parse_enums(root["enums"]);
    }

    // Parse types (user-defined structs)
    if (root.contains("types")) {
        parse_types(root["types"]);
    }

    // Parse seq (main struct fields) - create a main struct if seq exists
    if (root.contains("seq")) {
        // Create main struct with capitalized module name
        std::string struct_name = module_name_;
        if (!struct_name.empty()) {
            struct_name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(struct_name[0])));
        }

        ir::struct_def main_struct;
        main_struct.name = struct_name;
        main_struct.documentation = "Main structure for " + module_name_;
        main_struct.source = {"", 0, 0};

        parse_seq(root["seq"], main_struct);

        bundle_.structs.push_back(std::move(main_struct));
    }

    return std::move(bundle_);
}

void KsyToIrBuilder::parse_meta(const fkyaml::node& meta) {
    // Extract module name from 'id'
    if (meta.contains("id") && meta["id"].is_string()) {
        module_name_ = meta["id"].get_value<std::string>();
    }

    // Extract title as doc comment (store in bundle)
    if (meta.contains("title") && meta["title"].is_string()) {
        std::string title = meta["title"].get_value<std::string>();

        // Add file extension if present
        if (meta.contains("file-extension") && meta["file-extension"].is_string()) {
            std::string ext = meta["file-extension"].get_value<std::string>();
            title += "\nFile extension: ." + ext;
        }

        // TODO: Store title in bundle (bundle doesn't have doc_comment field yet)
        // For now, we'll use it as the main struct's doc comment
    }

    // Extract default endianness
    if (meta.contains("endian") && meta["endian"].is_string()) {
        std::string endian = meta["endian"].get_value<std::string>();
        if (endian == "le") {
            default_endianness_ = ir::endianness::little;
        } else if (endian == "be") {
            default_endianness_ = ir::endianness::big;
        }
    }
}

void KsyToIrBuilder::parse_seq(const fkyaml::node& seq, ir::struct_def& target_struct) {
    if (!seq.is_sequence()) {
        throw std::runtime_error("'seq' must be a sequence/array");
    }

    // Each item in seq is a field
    for (size_t i = 0; i < seq.size(); ++i) {
        const auto& item = seq[i];
        ir::field field = build_field(item);
        target_struct.fields.push_back(std::move(field));
    }
}

void KsyToIrBuilder::parse_types(const fkyaml::node& types) {
    if (!types.is_mapping()) {
        throw std::runtime_error("'types' must be a mapping/object");
    }

    // Each entry is a named type definition
    // fkYAML iteration over mappings using iterators
    for (auto it = types.begin(); it != types.end(); ++it) {
        std::string type_name = it.key().get_value<std::string>();
        const auto& type_def = (*it);

        ir::struct_def struct_def = build_struct(type_name, type_def);
        bundle_.structs.push_back(std::move(struct_def));
    }
}

void KsyToIrBuilder::parse_enums(const fkyaml::node& enums) {
    if (!enums.is_mapping()) {
        throw std::runtime_error("'enums' must be a mapping/object");
    }

    // Each entry is a named enum definition
    // fkYAML iteration over mappings using iterators
    for (auto it = enums.begin(); it != enums.end(); ++it) {
        std::string enum_name = it.key().get_value<std::string>();
        const auto& enum_def = (*it);

        ir::enum_def ir_enum = build_enum(enum_name, enum_def);
        bundle_.enums.push_back(std::move(ir_enum));
    }
}

ir::struct_def KsyToIrBuilder::build_struct(const std::string& name,
                                            const fkyaml::node& node) {
    ir::struct_def struct_def;

    // Capitalize first letter for struct name
    struct_def.name = name;
    if (!struct_def.name.empty()) {
        struct_def.name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(struct_def.name[0])));
    }

    // Extract doc comment if present
    if (node.contains("doc") && node["doc"].is_string()) {
        struct_def.documentation = node["doc"].get_value<std::string>();
    }

    struct_def.source = {"", 0, 0};

    // Parse fields from 'seq'
    if (node.contains("seq")) {
        parse_seq(node["seq"], struct_def);
    }

    return struct_def;
}

ir::enum_def KsyToIrBuilder::build_enum(const std::string& name,
                                       const fkyaml::node& node) {
    ir::enum_def enum_def;

    // Capitalize first letter for enum name
    enum_def.name = name;
    if (!enum_def.name.empty()) {
        enum_def.name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(enum_def.name[0])));
    }

    // Kaitai enums are simple mappings of value -> name
    if (!node.is_mapping()) {
        throw std::runtime_error("Enum definition must be a mapping");
    }

    // For now, default to uint8 for enums
    // TODO: Dynamically determine based on max value
    uint64_t max_value = 255;

    // Choose smallest type that fits and create type_ref properly
    enum_def.base_type.source = {"", 0, 0};
    enum_def.base_type.size_bytes = 0;
    enum_def.base_type.alignment = 0;
    enum_def.base_type.is_variable_size = false;

    if (max_value <= 255) {
        enum_def.base_type.kind = ir::type_kind::uint8;
        enum_def.base_type.size_bytes = 1;
        enum_def.base_type.alignment = 1;
    } else if (max_value <= 65535) {
        enum_def.base_type.kind = ir::type_kind::uint16;
        enum_def.base_type.byte_order = default_endianness_;
        enum_def.base_type.size_bytes = 2;
        enum_def.base_type.alignment = 2;
    } else if (max_value <= 4294967295ULL) {
        enum_def.base_type.kind = ir::type_kind::uint32;
        enum_def.base_type.byte_order = default_endianness_;
        enum_def.base_type.size_bytes = 4;
        enum_def.base_type.alignment = 4;
    } else {
        enum_def.base_type.kind = ir::type_kind::uint64;
        enum_def.base_type.byte_order = default_endianness_;
        enum_def.base_type.size_bytes = 8;
        enum_def.base_type.alignment = 8;
    }

    // Add enum items
    // fkYAML iteration over mappings using iterators
    for (auto it = node.begin(); it != node.end(); ++it) {
        // Keys are integers in Kaitai enum mappings
        uint64_t value;
        if (it.key().is_integer()) {
            value = it.key().get_value<uint64_t>();
        } else {
            // Fallback for string keys
            std::string key_str = it.key().get_value<std::string>();
            value = std::stoull(key_str);
        }

        std::string item_name = (*it).get_value<std::string>();

        ir::enum_def::item item;
        item.value = value;
        item.name = item_name;
        item.source = {"", 0, 0};

        // Capitalize first letter
        if (!item.name.empty()) {
            item.name[0] = static_cast<char>(std::toupper(static_cast<unsigned char>(item.name[0])));
        }

        enum_def.items.push_back(std::move(item));
    }

    enum_def.source = {"", 0, 0};

    return enum_def;
}

ir::field KsyToIrBuilder::build_field(const fkyaml::node& item) {
    ir::field field;
    field.source = {"", 0, 0};
    field.offset = 0;

    // Extract field name from 'id'
    if (!item.contains("id") || !item["id"].is_string()) {
        throw std::runtime_error("Field must have 'id' attribute");
    }
    field.name = item["id"].get_value<std::string>();

    // Extract type from 'type'
    if (item.contains("type") && item["type"].is_string()) {
        std::string type_str = item["type"].get_value<std::string>();
        field.type = map_type(type_str);
    } else {
        // Default to uint8 if no type specified (for size-based fields)
        field.type.kind = ir::type_kind::uint8;
        field.type.source = {"", 0, 0};
        field.type.size_bytes = 1;
        field.type.alignment = 1;
        field.type.is_variable_size = false;
    }

    // Extract doc comment if present
    if (item.contains("doc") && item["doc"].is_string()) {
        field.documentation = item["doc"].get_value<std::string>();
    }

    // Handle arrays (repeat: expr / repeat-expr)
    if (item.contains("repeat") && item["repeat"].is_string()) {
        std::string repeat = item["repeat"].get_value<std::string>();
        if (repeat == "expr" && item.contains("repeat-expr")) {
            // Create array type
            ir::type_ref array_type;
            array_type.source = {"", 0, 0};
            array_type.size_bytes = 0;
            array_type.alignment = 0;
            array_type.is_variable_size = true;

            // Store element type
            array_type.element_type = std::make_unique<ir::type_ref>(std::move(field.type));

            // Fixed or variable array
            if (item["repeat-expr"].is_integer()) {
                // Fixed size array
                array_type.kind = ir::type_kind::array_fixed;
                int64_t size = item["repeat-expr"].get_value<int64_t>();
                array_type.array_size = static_cast<uint64_t>(size);
            } else if (item["repeat-expr"].is_string()) {
                // Variable size array (field reference)
                array_type.kind = ir::type_kind::array_variable;
                std::string size_expr = item["repeat-expr"].get_value<std::string>();
                array_type.array_size_expr = std::make_unique<ir::expr>(map_expression(size_expr));
            }

            field.type = std::move(array_type);
        } else if (repeat == "eos") {
            // Read until end of stream
            unsupported("repeat: eos", "End-of-stream arrays not yet supported");
        } else if (repeat == "until") {
            // Read until condition
            unsupported("repeat: until", "Conditional arrays not yet supported");
        }
    }

    // Handle conditional fields ('if')
    if (item.contains("if") && item["if"].is_string()) {
        std::string cond_expr = item["if"].get_value<std::string>();
        field.condition = ir::field::runtime;
        field.runtime_condition = map_expression(cond_expr);
    }

    // Handle size constraint
    if (item.contains("size")) {
        if (item["size"].is_integer()) {
            int64_t size = item["size"].get_value<int64_t>();
            // For strings with size, convert to fixed array
            if (field.type.kind == ir::type_kind::string) {
                // Create fixed array type
                ir::type_ref array_type;
                array_type.kind = ir::type_kind::array_fixed;
                array_type.source = {"", 0, 0};
                array_type.size_bytes = static_cast<size_t>(size);
                array_type.alignment = 1;
                array_type.is_variable_size = false;
                array_type.array_size = static_cast<uint64_t>(size);

                // Element type is uint8
                auto elem = std::make_unique<ir::type_ref>();
                elem->kind = ir::type_kind::uint8;
                elem->source = {"", 0, 0};
                elem->size_bytes = 1;
                elem->alignment = 1;
                elem->is_variable_size = false;

                array_type.element_type = std::move(elem);
                field.type = std::move(array_type);
            }
        }
    }

    return field;
}

ir::type_ref KsyToIrBuilder::map_type(const std::string& ksy_type) {
    ir::type_ref type;
    type.source = {"", 0, 0};
    type.size_bytes = 0;
    type.alignment = 0;
    type.is_variable_size = false;

    // Basic primitive type mapping (Phase 1)
    if (ksy_type == "u1") {
        type.kind = ir::type_kind::uint8;
        type.size_bytes = 1;
        type.alignment = 1;
        return type;
    }
    if (ksy_type == "u2" || ksy_type == "u2le") {
        type.kind = ir::type_kind::uint16;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 2;
        type.alignment = 2;
        return type;
    }
    if (ksy_type == "u2be") {
        type.kind = ir::type_kind::uint16;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 2;
        type.alignment = 2;
        return type;
    }
    if (ksy_type == "u4" || ksy_type == "u4le") {
        type.kind = ir::type_kind::uint32;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 4;
        type.alignment = 4;
        return type;
    }
    if (ksy_type == "u4be") {
        type.kind = ir::type_kind::uint32;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 4;
        type.alignment = 4;
        return type;
    }
    if (ksy_type == "u8" || ksy_type == "u8le") {
        type.kind = ir::type_kind::uint64;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 8;
        type.alignment = 8;
        return type;
    }
    if (ksy_type == "u8be") {
        type.kind = ir::type_kind::uint64;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 8;
        type.alignment = 8;
        return type;
    }

    if (ksy_type == "s1") {
        type.kind = ir::type_kind::int8;
        type.size_bytes = 1;
        type.alignment = 1;
        return type;
    }
    if (ksy_type == "s2" || ksy_type == "s2le") {
        type.kind = ir::type_kind::int16;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 2;
        type.alignment = 2;
        return type;
    }
    if (ksy_type == "s2be") {
        type.kind = ir::type_kind::int16;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 2;
        type.alignment = 2;
        return type;
    }
    if (ksy_type == "s4" || ksy_type == "s4le") {
        type.kind = ir::type_kind::int32;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 4;
        type.alignment = 4;
        return type;
    }
    if (ksy_type == "s4be") {
        type.kind = ir::type_kind::int32;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 4;
        type.alignment = 4;
        return type;
    }
    if (ksy_type == "s8" || ksy_type == "s8le") {
        type.kind = ir::type_kind::int64;
        type.byte_order = ir::endianness::little;
        type.size_bytes = 8;
        type.alignment = 8;
        return type;
    }
    if (ksy_type == "s8be") {
        type.kind = ir::type_kind::int64;
        type.byte_order = ir::endianness::big;
        type.size_bytes = 8;
        type.alignment = 8;
        return type;
    }

    if (ksy_type == "str" || ksy_type == "strz") {
        type.kind = ir::type_kind::string;
        type.is_variable_size = true;
        return type;
    }

    // Unsupported types (Phase 1)
    if (ksy_type == "f4" || ksy_type == "f8") {
        unsupported("floating-point types",
                   "DataScript does not support floating-point types (f4, f8).\n"
                   "  Consider using fixed-point integers or external conversion.");
    }

    // Unknown type - assume user-defined struct
    type.kind = ir::type_kind::struct_type;
    type.is_variable_size = true;
    // TODO: We should look up the actual struct definition and store its index
    return type;
}

ir::expr KsyToIrBuilder::map_expression(const std::string& expr_str) {
    // TODO: Implement proper expression parser
    // For now, just handle simple cases

    // Check for field references (simple identifiers)
    // In Kaitai, field references are just identifiers
    // For now, create a field_ref expression

    // Simple heuristic: if it's a single word, it's probably a field reference
    bool is_simple_identifier = true;
    for (char c : expr_str) {
        if (!std::isalnum(c) && c != '_') {
            is_simple_identifier = false;
            break;
        }
    }

    ir::expr expr;
    expr.source = {"", 0, 0};

    if (is_simple_identifier) {
        // Field reference
        expr.type = ir::expr::field_ref;
        expr.ref_name = expr_str;
        return expr;
    }

    // For more complex expressions, we need a proper parser
    // For now, throw an error
    unsupported("complex expressions",
               "Complex expressions not yet supported: " + expr_str);
}

void KsyToIrBuilder::unsupported(const std::string& feature,
                               const std::string& message) {
    throw KsyError(feature, message);
}

}  // namespace datascript::ksy

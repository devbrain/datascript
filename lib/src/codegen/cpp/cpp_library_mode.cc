//
// C++ Library Mode Generator Implementation
//
// Generates three header files for library mode:
// 1. Runtime header - Exceptions and binary reading helpers
// 2. Public header - Enums, constants, forward declarations
// 3. Implementation header - Full struct definitions with methods
//

#include <datascript/codegen/cpp/cpp_library_mode.hh>
#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <datascript/codegen/cpp/cpp_helper_generator.hh>
#include <datascript/codegen/cpp/cpp_writer_context.hh>
#include <datascript/command_builder.hh>
#include <datascript/codegen.hh>
#include <sstream>
#include <algorithm>

namespace datascript::codegen {

using codegen::endl;
using codegen::blank;

// ============================================================================
// Construction
// ============================================================================

CppLibraryModeGenerator::CppLibraryModeGenerator(CppRenderer& renderer)
    : renderer_(renderer)
{
}

// ============================================================================
// Public API
// ============================================================================

std::vector<OutputFile> CppLibraryModeGenerator::generate(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Extract namespace from bundle name
    std::string namespace_name = extract_namespace(bundle);

    // Update renderer's namespace
    renderer_.set_namespace(namespace_name);

    // Get the three output filenames
    LibraryFiles files = get_filenames(bundle, output_dir);

    // Generate all three headers
    std::string runtime_content = generate_runtime_header(namespace_name, files);
    std::string public_content = generate_public_header(bundle, namespace_name, files);
    std::string impl_content = generate_impl_header(bundle, namespace_name, files);

    // Return all three files
    return {
        {files.runtime_header, runtime_content},
        {files.public_header, public_content},
        {files.impl_header, impl_content}
    };
}

// ============================================================================
// Private Helpers
// ============================================================================

std::string CppLibraryModeGenerator::extract_namespace(const ir::bundle& bundle) const {
    // Check if bundle has a name (module name from package declaration)
    if (bundle.name.empty()) {
        return "generated";
    }

    // Convert module name to namespace
    // Example: "pe.format" -> "pe::format"
    std::string ns = bundle.name;
    size_t pos = 0;
    while ((pos = ns.find('.', pos)) != std::string::npos) {
        ns.replace(pos, 1, "::");
        pos += 2;  // Skip past the "::" we just inserted
    }

    return ns;
}

CppLibraryModeGenerator::LibraryFiles CppLibraryModeGenerator::get_filenames(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir) const
{
    std::string base_name;

    // TODO: Get output_name_override from renderer if available
    // For now, derive from module name or first struct
    if (!bundle.name.empty()) {
        base_name = bundle.name;
        // Replace dots with underscores: "pe.format" -> "pe_format"
        std::replace(base_name.begin(), base_name.end(), '.', '_');
    } else if (!bundle.structs.empty()) {
        base_name = bundle.structs[0].name;
    } else if (!bundle.choices.empty()) {
        base_name = bundle.choices[0].name;
    } else if (!bundle.enums.empty()) {
        base_name = bundle.enums[0].name;
    } else {
        base_name = "generated";
    }

    // Construct the three filenames
    LibraryFiles files;
    files.public_header = (output_dir / (base_name + ".h")).string();
    files.impl_header = (output_dir / (base_name + "_impl.h")).string();
    files.runtime_header = (output_dir / (base_name + "_runtime.h")).string();

    return files;
}

std::string CppLibraryModeGenerator::generate_runtime_header(
    const std::string& namespace_name,
    const LibraryFiles& files) const
{
    std::ostringstream output;
    CppWriterContext ctx(output);

    // Write header guard
    ctx.write_pragma_once();
    ctx.write_blank_line();

    // Write includes
    ctx.write_include("cstdint", true);
    ctx.write_include("cstddef", true);
    ctx.write_include("stdexcept", true);
    ctx.write_include("string", true);
    ctx.write_blank_line();

    // Start namespace
    ctx.start_namespace(namespace_name);
    ctx.write_blank_line();

    // Generate exception classes and binary helpers using CppHelperGenerator
    CppHelperGenerator helper_gen(ctx, cpp_options::exceptions_only);
    helper_gen.generate_all();

    ctx.write_blank_line();

    // End namespace
    ctx.end_namespace();

    return output.str();
}

std::string CppLibraryModeGenerator::generate_public_header(
    const ir::bundle& bundle,
    const std::string& namespace_name,
    const LibraryFiles& files) const
{
    std::ostringstream output;
    CppWriterContext ctx(output);

    // Write header guard
    ctx.write_pragma_once();
    ctx.write_blank_line();

    // Write includes
    ctx.write_include(std::filesystem::path(files.runtime_header).filename().string(), false);
    ctx.write_include("cstdint", true);
    ctx.write_include("memory", true);
    ctx.write_include("vector", true);
    ctx.write_include("variant", true);
    ctx.write_include("string", true);
    ctx.write_blank_line();

    // Start namespace
    ctx.start_namespace(namespace_name);
    ctx.write_blank_line();

    // ========================================================================
    // Enums
    // ========================================================================
    if (!bundle.enums.empty()) {
        ctx << "// ============================================================================" << endl;
        ctx << "// Enums" << endl;
        ctx << "// ============================================================================" << endl;
        ctx.write_blank_line();

        for (const auto& enum_def : bundle.enums) {
            std::string base_type = renderer_.get_type_name(enum_def.base_type);
            ctx.start_enum(enum_def.name, base_type, enum_def.documentation);

            for (const auto& item : enum_def.items) {
                ctx << item.name << " = " << item.value << "," << endl;
            }

            ctx.end_enum();
            ctx.write_blank_line();
        }
    }

    // ========================================================================
    // Constants
    // ========================================================================
    if (!bundle.constants.empty()) {
        ctx << "// ============================================================================" << endl;
        ctx << "// Constants" << endl;
        ctx << "// ============================================================================" << endl;
        ctx.write_blank_line();

        for (const auto& [name, value] : bundle.constants) {
            // Constants in IR are uint64_t values
            ctx << "inline constexpr uint64_t " << name << " = " << value << ";" << endl;
        }
        ctx.write_blank_line();
    }

    // ========================================================================
    // Forward Declarations
    // ========================================================================
    if (!bundle.structs.empty() || !bundle.choices.empty()) {
        ctx << "// ============================================================================" << endl;
        ctx << "// Forward Declarations" << endl;
        ctx << "// ============================================================================" << endl;
        ctx.write_blank_line();

        for (const auto& struct_def : bundle.structs) {
            ctx << "struct " << struct_def.name << ";" << endl;
        }

        for (const auto& choice_def : bundle.choices) {
            ctx << "struct " << choice_def.name << ";" << endl;
        }
        ctx.write_blank_line();
    }

    // ========================================================================
    // Introspection API
    // ========================================================================
    ctx << "// ============================================================================" << endl;
    ctx << "// Introspection API" << endl;
    ctx << "// ============================================================================" << endl;
    ctx.write_blank_line();

    // Forward declare introspection types
    ctx << "class Field;" << endl;
    ctx << "class FieldIterator;" << endl;
    ctx << "class FieldRange;" << endl;
    ctx.write_blank_line();

    // StructView template declaration
    ctx << "template<typename T>" << endl;
    ctx << "class StructView {" << endl;
    ctx << "public:" << endl;
    ctx << "    explicit StructView(const T* ptr) : ptr_(ptr) {}" << endl;
    ctx.write_blank_line();
    ctx << "    const char* type_name() const;" << endl;
    ctx << "    const char* docstring() const;" << endl;
    ctx << "    bool has_docstring() const;" << endl;
    ctx << "    size_t size_in_bytes() const;" << endl;
    ctx << "    size_t field_count() const;" << endl;
    ctx.write_blank_line();
    ctx << "    Field field(size_t idx) const;" << endl;
    ctx << "    Field find_field(const char* name) const;" << endl;
    ctx << "    FieldRange fields() const;" << endl;
    ctx.write_blank_line();
    ctx << "    std::string to_json() const;" << endl;
    ctx << "    void to_json(std::ostream& out) const;" << endl;
    ctx << "    void print(std::ostream& out, int indent = 0) const;" << endl;
    ctx.write_blank_line();
    ctx << "private:" << endl;
    ctx << "    const T* ptr_;" << endl;
    ctx << "    struct Metadata;" << endl;
    ctx << "    static const Metadata* get_metadata();" << endl;
    ctx << "};" << endl;
    ctx.write_blank_line();

    // End namespace
    ctx.end_namespace();

    return output.str();
}

std::string CppLibraryModeGenerator::generate_impl_header(
    const ir::bundle& bundle,
    const std::string& namespace_name,
    const LibraryFiles& files)
{
    std::ostringstream output;

    // Write header guard and includes
    output << "#pragma once\n\n";
    output << "#include \"" << std::filesystem::path(files.public_header).filename().string() << "\"\n";
    output << "#include <array>\n";
    output << "#include <vector>\n";
    output << "#include <variant>\n";
    output << "#include <string>\n";
    output << "#include <optional>\n";
    output << "#include <span>\n";
    output << "#include <sstream>\n";
    output << "#include <iomanip>\n\n";

    // Start namespace
    output << "namespace " << namespace_name << " {\n\n";

    // Use CommandBuilder to generate only struct definitions
    // This avoids duplicating helpers, enums, and constants
    CommandBuilder builder;
    cpp_options opts;
    opts.error_handling = cpp_options::exceptions_only;

    // Set up renderer for struct generation
    renderer_.set_module(&bundle);
    renderer_.clear();

    // Build and render each struct (with read() method)
    for (const auto& struct_def : bundle.structs) {
        auto commands = builder.build_struct_reader(struct_def, true);
        renderer_.render_commands(commands);
    }

    // Build and render each choice
    for (const auto& choice_def : bundle.choices) {
        auto commands = builder.build_choice_declaration(choice_def, opts);
        renderer_.render_commands(commands);
    }

    // Build and render each union
    for (const auto& union_def : bundle.unions) {
        auto commands = builder.build_union_declaration(union_def, opts);
        renderer_.render_commands(commands);
    }

    // Get the rendered struct code
    output << renderer_.get_output();
    renderer_.clear();

    // ========================================================================
    // Parse Functions (convenience wrappers)
    // ========================================================================
    output << "\n// ============================================================================\n";
    output << "// Parse Functions\n";
    output << "// ============================================================================\n\n";

    for (const auto& struct_def : bundle.structs) {
        // Pointer + length overload
        output << "/**\n";
        output << " * Parse " << struct_def.name << " from binary data.\n";
        output << " * @param data Pointer to binary data\n";
        output << " * @param len Length of data in bytes\n";
        output << " * @return Parsed " << struct_def.name << " object\n";
        output << " * @throws UnexpectedEOF if data is too short\n";
        output << " */\n";
        output << "inline " << struct_def.name << " parse_" << struct_def.name
               << "(const uint8_t* data, size_t len) {\n";
        output << "    const uint8_t* p = data;\n";
        output << "    const uint8_t* end = data + len;\n";
        output << "    return " << struct_def.name << "::read(p, end);\n";
        output << "}\n\n";

        // std::vector overload
        output << "/**\n";
        output << " * Parse " << struct_def.name << " from std::vector.\n";
        output << " * @param data Vector of binary data\n";
        output << " * @return Parsed " << struct_def.name << " object\n";
        output << " * @throws UnexpectedEOF if data is too short\n";
        output << " */\n";
        output << "inline " << struct_def.name << " parse_" << struct_def.name
               << "(const std::vector<uint8_t>& data) {\n";
        output << "    return parse_" << struct_def.name << "(data.data(), data.size());\n";
        output << "}\n\n";

        // std::span overload (C++20)
        output << "/**\n";
        output << " * Parse " << struct_def.name << " from std::span.\n";
        output << " * @param data Span of binary data\n";
        output << " * @return Parsed " << struct_def.name << " object\n";
        output << " * @throws UnexpectedEOF if data is too short\n";
        output << " */\n";
        output << "inline " << struct_def.name << " parse_" << struct_def.name
               << "(std::span<const uint8_t> data) {\n";
        output << "    return parse_" << struct_def.name << "(data.data(), data.size());\n";
        output << "}\n\n";
    }

    // ========================================================================
    // Introspection Metadata
    // ========================================================================
    output << "\n// ============================================================================\n";
    output << "// Introspection Metadata\n";
    output << "// ============================================================================\n\n";

    output << "namespace introspection {\n\n";

    // Field type enum
    output << "enum class FieldType {\n";
    output << "    Primitive,\n";
    output << "    Struct,\n";
    output << "    Enum,\n";
    output << "    Array,\n";
    output << "    String\n";
    output << "};\n\n";

    // FieldMeta struct
    output << "struct FieldMeta {\n";
    output << "    const char* name;\n";
    output << "    const char* type_name;\n";
    output << "    const char* docstring;\n";
    output << "    size_t offset;\n";
    output << "    size_t size;\n";
    output << "    FieldType type_kind;\n";
    output << "    std::string (*format_value)(const void* struct_ptr);\n";
    output << "};\n\n";

    // StructMeta struct
    output << "struct StructMeta {\n";
    output << "    const char* type_name;\n";
    output << "    const char* docstring;\n";
    output << "    size_t size_in_bytes;\n";
    output << "    size_t field_count;\n";
    output << "    const FieldMeta* fields;\n";
    output << "};\n\n";

    // Generate metadata for each struct
    for (const auto& struct_def : bundle.structs) {
        generate_struct_metadata(output, struct_def, namespace_name);
    }

    output << "} // namespace introspection\n\n";

    // ========================================================================
    // Field, FieldIterator, FieldRange Implementation
    // ========================================================================
    output << "// ============================================================================\n";
    output << "// Field, FieldIterator, FieldRange Implementation\n";
    output << "// ============================================================================\n\n";

    // Field class implementation
    output << "class Field {\n";
    output << "public:\n";
    output << "    Field(const introspection::FieldMeta* meta, const void* struct_ptr)\n";
    output << "        : meta_(meta), struct_ptr_(struct_ptr) {}\n\n";
    output << "    const char* name() const { return meta_->name; }\n";
    output << "    const char* type_name() const { return meta_->type_name; }\n";
    output << "    const char* docstring() const { return meta_->docstring; }\n";
    output << "    bool has_docstring() const { return meta_->docstring != nullptr; }\n";
    output << "    size_t offset() const { return meta_->offset; }\n";
    output << "    size_t size() const { return meta_->size; }\n\n";
    output << "    bool is_primitive() const { return meta_->type_kind == introspection::FieldType::Primitive; }\n";
    output << "    bool is_struct() const { return meta_->type_kind == introspection::FieldType::Struct; }\n";
    output << "    bool is_enum() const { return meta_->type_kind == introspection::FieldType::Enum; }\n";
    output << "    bool is_array() const { return meta_->type_kind == introspection::FieldType::Array; }\n";
    output << "    bool is_string() const { return meta_->type_kind == introspection::FieldType::String; }\n\n";
    output << "    std::string value_as_string() const {\n";
    output << "        return meta_->format_value(struct_ptr_);\n";
    output << "    }\n\n";
    output << "private:\n";
    output << "    const introspection::FieldMeta* meta_;\n";
    output << "    const void* struct_ptr_;\n";
    output << "};\n\n";

    // FieldIterator class implementation
    output << "class FieldIterator {\n";
    output << "public:\n";
    output << "    using iterator_category = std::forward_iterator_tag;\n";
    output << "    using value_type = Field;\n";
    output << "    using difference_type = std::ptrdiff_t;\n";
    output << "    using pointer = Field;\n";
    output << "    using reference = Field;\n\n";
    output << "    FieldIterator(const introspection::FieldMeta* meta, const void* struct_ptr, size_t index)\n";
    output << "        : meta_(meta), struct_ptr_(struct_ptr), index_(index) {}\n\n";
    output << "    FieldIterator& operator++() { ++index_; return *this; }\n";
    output << "    Field operator*() const { return Field(&meta_[index_], struct_ptr_); }\n";
    output << "    bool operator==(const FieldIterator& other) const { return index_ == other.index_; }\n";
    output << "    bool operator!=(const FieldIterator& other) const { return index_ != other.index_; }\n\n";
    output << "private:\n";
    output << "    const introspection::FieldMeta* meta_;\n";
    output << "    const void* struct_ptr_;\n";
    output << "    size_t index_;\n";
    output << "};\n\n";

    // FieldRange class implementation
    output << "class FieldRange {\n";
    output << "public:\n";
    output << "    FieldRange(const introspection::FieldMeta* meta, const void* struct_ptr, size_t count)\n";
    output << "        : meta_(meta), struct_ptr_(struct_ptr), count_(count) {}\n\n";
    output << "    FieldIterator begin() const { return FieldIterator(meta_, struct_ptr_, 0); }\n";
    output << "    FieldIterator end() const { return FieldIterator(meta_, struct_ptr_, count_); }\n\n";
    output << "private:\n";
    output << "    const introspection::FieldMeta* meta_;\n";
    output << "    const void* struct_ptr_;\n";
    output << "    size_t count_;\n";
    output << "};\n\n";

    // ========================================================================
    // StructView Template Specializations
    // ========================================================================
    output << "// ============================================================================\n";
    output << "// StructView Template Specializations\n";
    output << "// ============================================================================\n\n";

    for (const auto& struct_def : bundle.structs) {
        std::string struct_name = struct_def.name;

        // Define Metadata type alias
        output << "template<>\n";
        output << "struct StructView<" << struct_name << ">::Metadata {\n";
        output << "    const char* type_name;\n";
        output << "    const char* docstring;\n";
        output << "    size_t size_in_bytes;\n";
        output << "    size_t field_count;\n";
        output << "    const introspection::FieldMeta* fields;\n";
        output << "};\n\n";

        // get_metadata() specialization
        output << "template<>\n";
        output << "inline const typename StructView<" << struct_name << ">::Metadata*\n";
        output << "StructView<" << struct_name << ">::get_metadata() {\n";
        output << "    return reinterpret_cast<const Metadata*>(&introspection::" << struct_name << "_meta);\n";
        output << "}\n\n";

        // type_name() specialization
        output << "template<>\n";
        output << "inline const char* StructView<" << struct_name << ">::type_name() const {\n";
        output << "    return get_metadata()->type_name;\n";
        output << "}\n\n";

        // docstring() specialization
        output << "template<>\n";
        output << "inline const char* StructView<" << struct_name << ">::docstring() const {\n";
        output << "    return get_metadata()->docstring;\n";
        output << "}\n\n";

        // has_docstring() specialization
        output << "template<>\n";
        output << "inline bool StructView<" << struct_name << ">::has_docstring() const {\n";
        output << "    return get_metadata()->docstring != nullptr;\n";
        output << "}\n\n";

        // size_in_bytes() specialization
        output << "template<>\n";
        output << "inline size_t StructView<" << struct_name << ">::size_in_bytes() const {\n";
        output << "    return get_metadata()->size_in_bytes;\n";
        output << "}\n\n";

        // field_count() specialization
        output << "template<>\n";
        output << "inline size_t StructView<" << struct_name << ">::field_count() const {\n";
        output << "    return get_metadata()->field_count;\n";
        output << "}\n\n";

        // field() specialization
        output << "template<>\n";
        output << "inline Field StructView<" << struct_name << ">::field(size_t idx) const {\n";
        output << "    if (idx >= get_metadata()->field_count) {\n";
        output << "        throw std::out_of_range(\"Field index out of range\");\n";
        output << "    }\n";
        output << "    return Field(&get_metadata()->fields[idx], ptr_);\n";
        output << "}\n\n";

        // find_field() specialization
        output << "template<>\n";
        output << "inline Field StructView<" << struct_name << ">::find_field(const char* name) const {\n";
        output << "    for (size_t i = 0; i < get_metadata()->field_count; ++i) {\n";
        output << "        if (std::string(get_metadata()->fields[i].name) == name) {\n";
        output << "            return Field(&get_metadata()->fields[i], ptr_);\n";
        output << "        }\n";
        output << "    }\n";
        output << "    throw std::runtime_error(\"Field not found: \" + std::string(name));\n";
        output << "}\n\n";

        // fields() specialization
        output << "template<>\n";
        output << "inline FieldRange StructView<" << struct_name << ">::fields() const {\n";
        output << "    return FieldRange(get_metadata()->fields, ptr_, get_metadata()->field_count);\n";
        output << "}\n\n";

        // to_json(ostream) specialization - MUST come before to_json()
        output << "template<>\n";
        output << "inline void StructView<" << struct_name << ">::to_json(std::ostream& out) const {\n";
        output << "    out << \"{\";\n";
        output << "    for (size_t i = 0; i < get_metadata()->field_count; ++i) {\n";
        output << "        if (i > 0) out << \",\";\n";
        output << "        out << \"\\\"\" << get_metadata()->fields[i].name << \"\\\":\";\n";
        output << "        out << \"\\\"\" << get_metadata()->fields[i].format_value(ptr_) << \"\\\"\";\n";
        output << "    }\n";
        output << "    out << \"}\";\n";
        output << "}\n\n";

        // to_json() specialization - calls to_json(ostream)
        output << "template<>\n";
        output << "inline std::string StructView<" << struct_name << ">::to_json() const {\n";
        output << "    std::ostringstream oss;\n";
        output << "    to_json(oss);\n";
        output << "    return oss.str();\n";
        output << "}\n\n";

        // print() specialization
        output << "template<>\n";
        output << "inline void StructView<" << struct_name << ">::print(std::ostream& out, int indent) const {\n";
        output << "    std::string ind(indent, ' ');\n";
        output << "    out << ind << get_metadata()->type_name << \" {\\n\";\n";
        output << "    for (size_t i = 0; i < get_metadata()->field_count; ++i) {\n";
        output << "        const auto& field = get_metadata()->fields[i];\n";
        output << "        out << ind << \"  \" << field.name << \": \" << field.format_value(ptr_) << \"\\n\";\n";
        output << "    }\n";
        output << "    out << ind << \"}\\n\";\n";
        output << "}\n\n";
    }

    // End namespace
    output << "}  // namespace " << namespace_name << "\n";

    return output.str();
}

void CppLibraryModeGenerator::generate_struct_metadata(
    std::ostream& out,
    const ir::struct_def& struct_def,
    const std::string& namespace_name) const
{
    std::string struct_name = struct_def.name;

    // Generate formatter for each field
    for (size_t i = 0; i < struct_def.fields.size(); ++i) {
        const auto& field = struct_def.fields[i];

        out << "inline std::string format_" << struct_name << "_" << field.name
            << "(const void* ptr) {\n";
        out << "    auto* s = static_cast<const ::" << namespace_name << "::" << struct_name << "*>(ptr);\n";
        out << "    std::ostringstream oss;\n";

        // Smart formatting based on field type
        bool is_primitive = (field.type.kind >= ir::type_kind::uint8 && field.type.kind <= ir::type_kind::int128);
        if (is_primitive) {
            // Format hex for types that look like offsets/addresses
            if (field.name.find("offset") != std::string::npos ||
                field.name.find("address") != std::string::npos ||
                field.name.find("pointer") != std::string::npos) {
                out << "    oss << \"0x\" << std::hex << std::setw(8) << std::setfill('0') << s->" << field.name << ";\n";
            } else {
                out << "    oss << static_cast<uint64_t>(s->" << field.name << ");\n";
            }
        } else if (field.type.kind == ir::type_kind::enum_type) {
            // For enums, show numeric value (to_string can be added later)
            out << "    oss << static_cast<int>(s->" << field.name << ");\n";
        } else if (field.type.kind == ir::type_kind::array_fixed ||
                   field.type.kind == ir::type_kind::array_variable ||
                   field.type.kind == ir::type_kind::array_ranged) {
            // Show array size
            out << "    oss << \"[\" << s->" << field.name << ".size() << \" items]\";\n";
        } else {
            // Generic fallback
            out << "    oss << \"<\" << \"" << renderer_.get_type_name(field.type) << "\" << \">\";\n";
        }

        out << "    return oss.str();\n";
        out << "}\n\n";
    }

    // Generate field metadata array
    out << "inline const FieldMeta " << struct_name << "_fields[] = {\n";
    for (size_t i = 0; i < struct_def.fields.size(); ++i) {
        const auto& field = struct_def.fields[i];

        out << "    {\n";
        out << "        \"" << field.name << "\",\n";
        out << "        \"" << renderer_.get_type_name(field.type) << "\",\n";
        if (!field.documentation.empty()) {
            out << "        \"" << field.documentation << "\",\n";
        } else {
            out << "        nullptr,\n";
        }
        out << "        offsetof(::" << namespace_name << "::" << struct_name << ", " << field.name << "),\n";
        out << "        sizeof(((::" << namespace_name << "::" << struct_name << "*)0)->" << field.name << "),\n";

        // Determine field type kind
        bool is_prim = (field.type.kind >= ir::type_kind::uint8 && field.type.kind <= ir::type_kind::int128);
        bool is_array = (field.type.kind == ir::type_kind::array_fixed ||
                         field.type.kind == ir::type_kind::array_variable ||
                         field.type.kind == ir::type_kind::array_ranged);
        if (is_prim) {
            out << "        FieldType::Primitive,\n";
        } else if (field.type.kind == ir::type_kind::enum_type) {
            out << "        FieldType::Enum,\n";
        } else if (is_array) {
            out << "        FieldType::Array,\n";
        } else if (field.type.kind == ir::type_kind::struct_type) {
            out << "        FieldType::Struct,\n";
        } else if (field.type.kind == ir::type_kind::string ||
                   field.type.kind == ir::type_kind::u16_string ||
                   field.type.kind == ir::type_kind::u32_string) {
            out << "        FieldType::String,\n";
        } else {
            out << "        FieldType::Primitive,\n";
        }

        out << "        format_" << struct_name << "_" << field.name << "\n";
        out << "    }" << (i < struct_def.fields.size() - 1 ? "," : "") << "\n";
    }
    out << "};\n\n";

    // Generate struct metadata
    out << "inline const StructMeta " << struct_name << "_meta = {\n";
    out << "    \"" << struct_name << "\",\n";
    if (!struct_def.documentation.empty()) {
        out << "    \"" << struct_def.documentation << "\",\n";
    } else {
        out << "    nullptr,\n";
    }
    out << "    sizeof(::" << namespace_name << "::" << struct_name << "),\n";
    out << "    " << struct_def.fields.size() << ",\n";
    out << "    " << struct_name << "_fields\n";
    out << "};\n\n";
}

}  // namespace datascript::codegen

#pragma once

#include <string>
#include <stdexcept>
#include <datascript/ir.hh>

// fkYAML uses versioned namespaces, so we need to include the header
#include <fkYAML/node.hpp>

namespace datascript::ksy {

/**
 * Exception thrown when unsupported Kaitai features are encountered.
 */
class KsyError : public std::runtime_error {
public:
    KsyError(const std::string& feature, const std::string& message)
        : std::runtime_error(format_error(feature, message))
        , feature_(feature) {}

    const std::string& feature() const { return feature_; }

private:
    std::string feature_;

    static std::string format_error(const std::string& feature,
                                    const std::string& message) {
        std::string result = "KsyError: Unsupported feature '";
        result += feature;
        result += "'\n  ";
        result += message;
        result += "\n\nFor more information, see: docs/KSY_TO_DATASCRIPT_TRANSLATOR.md";
        return result;
    }
};

/**
 * Builds DataScript IR from Kaitai Struct YAML files.
 *
 * This class converts Kaitai Struct (.ksy) YAML specifications into DataScript's
 * Intermediate Representation (IR). The resulting IR can then be used to generate
 * code in various target languages (C++, DataScript source, Python, etc.).
 *
 * Similar to IrBuilder but reads from fkYAML instead of AST.
 *
 * Example usage:
 *   KsyToIrBuilder builder;
 *   ir::bundle bundle = builder.build_from_file("format.ksy");
 *
 *   // Generate C++ code
 *   CppRenderer cpp_renderer;
 *   std::string code = cpp_renderer.render_module(bundle, {});
 *
 *   // Or generate DataScript source
 *   DataScriptRenderer ds_renderer;
 *   std::string ds_source = ds_renderer.render_module(bundle, {});
 */
class KsyToIrBuilder {
public:
    KsyToIrBuilder();
    ~KsyToIrBuilder();

    /**
     * Build IR bundle from .ksy file.
     *
     * @param ksy_path Path to .ksy file
     * @return Complete IR bundle ready for code generation
     * @throws KsyError for unsupported features
     * @throws std::runtime_error for file I/O errors
     */
    ir::bundle build_from_file(const std::string& ksy_path);

    /**
     * Build IR bundle from YAML node (for testing).
     *
     * @param root Parsed YAML root node
     * @return Complete IR bundle
     * @throws KsyError for unsupported features
     */
    ir::bundle build_from_yaml(const fkyaml::node& root);

private:
    // Parse meta section → module name, doc comments
    void parse_meta(const fkyaml::node& meta);

    // Parse seq section → main struct fields
    void parse_seq(const fkyaml::node& seq, ir::struct_def& target_struct);

    // Parse types section → struct definitions
    void parse_types(const fkyaml::node& types);

    // Parse enums section → enum definitions
    void parse_enums(const fkyaml::node& enums);

    // Build IR struct from YAML type definition
    ir::struct_def build_struct(const std::string& name,
                                const fkyaml::node& node);

    // Build IR enum from YAML enum definition
    ir::enum_def build_enum(const std::string& name,
                           const fkyaml::node& node);

    // Build IR field from YAML sequence item
    ir::field build_field(const fkyaml::node& item);

    // Convert Kaitai type string to IR type
    ir::type_ref map_type(const std::string& ksy_type);

    // Convert Kaitai expression string to IR expression
    ir::expr map_expression(const std::string& expr_str);

    // Throw error for unsupported feature
    [[noreturn]] void unsupported(const std::string& feature,
                                   const std::string& message);

    // IR bundle being built
    ir::bundle bundle_;

    // Current module name (from meta.id)
    std::string module_name_;

    // Default endianness (from meta.endian)
    ir::endianness default_endianness_ = ir::endianness::little;
};

}  // namespace datascript::ksy

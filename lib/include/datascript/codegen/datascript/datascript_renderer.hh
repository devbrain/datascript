#pragma once

#include <datascript/base_renderer.hh>
#include <datascript/codegen/code_writer.hh>
#include <datascript/ir.hh>
#include <string>
#include <sstream>

namespace datascript::codegen {

/**
 * Renders DataScript IR back to human-readable .ds source code.
 *
 * This renderer takes a validated IR bundle and generates clean,
 * idiomatic DataScript source code with proper indentation and formatting.
 *
 * Use cases:
 * - Convert Kaitai Struct (.ksy) files to DataScript (.ds) for better readability
 * - Pretty-print IR for debugging
 * - Generate DataScript from programmatically-built IR
 *
 * Example usage:
 *   DataScriptRenderer renderer;
 *   std::string ds_source = renderer.render_module(ir_bundle, {});
 */
class DataScriptRenderer : public BaseRenderer {
public:
    DataScriptRenderer();
    ~DataScriptRenderer() override = default;

    // ========================================================================
    // BaseRenderer Interface Implementation
    // ========================================================================

    std::string get_option_prefix() const override { return "ds"; }

    // DataScript renderer has no custom options (uses defaults)
    // Inherits default get_options() which returns empty vector

    /// Generate DataScript output files (single .ds file)
    std::vector<OutputFile> generate_files(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir) override;

    std::string render_module(const ir::bundle& bundle,
                             const RenderOptions& options) override;

    void render_command(const Command& cmd) override;

    std::string render_expression(const ir::expr* expr) override;

    LanguageMetadata get_metadata() const override;

    std::string get_language_name() const override { return "DataScript"; }
    std::string get_file_extension() const override { return ".ds"; }
    bool is_case_sensitive() const override { return true; }
    std::string get_default_object_name() const override { return "obj"; }

    bool is_reserved_keyword(const std::string& identifier) const override;
    std::string sanitize_identifier(const std::string& identifier) const override;
    std::vector<std::string> get_all_keywords() const override;
    std::string get_type_name(const ir::type_ref& type) const override;

private:
    // ========================================================================
    // Rendering Components
    // ========================================================================

    void render_package(const std::string& name);
    void render_constants(const std::map<std::string, uint64_t>& constants);
    void render_enum(const ir::enum_def& e);
    void render_struct(const ir::struct_def& s);
    void render_union(const ir::union_def& u);
    void render_choice(const ir::choice_def& c);
    void render_subtype(const ir::subtype_def& s);
    void render_field(const ir::field& f);

    std::string render_expr(const ir::expr& expr);
    std::string get_binary_op_string(ir::expr::op_type op) const;
    std::string get_unary_op_string(ir::expr::op_type op) const;

    // Documentation helpers
    void render_doc_comment(const std::string& doc);

    // ========================================================================
    // State
    // ========================================================================

    std::ostringstream output_;
    std::unique_ptr<CodeWriter> writer_;

    // DataScript keywords for identifier sanitization
    static const std::set<std::string> keywords_;
};

}  // namespace datascript::codegen

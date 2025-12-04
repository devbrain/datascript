#include <datascript/codegen/datascript/datascript_renderer.hh>
#include <datascript/codegen/code_writer.hh>
#include <sstream>

namespace datascript::codegen {

// DataScript keywords
const std::set<std::string> DataScriptRenderer::keywords_ = {
    "package", "import", "const", "struct", "union", "choice", "enum",
    "subtype", "bitmask", "function", "if", "else", "return",
    "true", "false", "this", "little", "big", "bit",
    "uint8", "uint16", "uint32", "uint64", "uint128",
    "int8", "int16", "int32", "int64", "int128",
    "string", "bool"
};

DataScriptRenderer::DataScriptRenderer() = default;

std::string DataScriptRenderer::render_module(const ir::bundle& bundle,
                                             const RenderOptions& options) {
    output_.str("");
    output_.clear();
    writer_ = std::make_unique<CodeWriter>(output_);

    // Render package declaration
    if (!bundle.name.empty()) {
        render_package(bundle.name);
        writer_->write_blank_line();
    }

    // Render constants
    if (!bundle.constants.empty()) {
        render_constants(bundle.constants);
        writer_->write_blank_line();
    }

    // Render enums
    for (const auto& enum_def : bundle.enums) {
        render_enum(enum_def);
        writer_->write_blank_line();
    }

    // Render subtypes
    for (const auto& subtype : bundle.subtypes) {
        render_subtype(subtype);
        writer_->write_blank_line();
    }

    // Render structs
    for (const auto& struct_def : bundle.structs) {
        render_struct(struct_def);
        writer_->write_blank_line();
    }

    // Render unions
    for (const auto& union_def : bundle.unions) {
        render_union(union_def);
        writer_->write_blank_line();
    }

    // Render choices
    for (const auto& choice : bundle.choices) {
        render_choice(choice);
        writer_->write_blank_line();
    }

    return output_.str();
}

std::vector<OutputFile> DataScriptRenderer::generate_files(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Determine filename from bundle name or use "formatted"
    std::string filename = bundle.name.empty() ? "formatted" : bundle.name;
    filename += ".ds";

    std::filesystem::path output_path = output_dir / filename;

    // Generate DataScript code using existing render_module() method
    RenderOptions default_options;
    std::string content = render_module(bundle, default_options);

    return {{output_path, content}};
}

void DataScriptRenderer::render_command(const Command& cmd) {
    // DataScriptRenderer doesn't use command-based rendering
    // It renders directly from IR in render_module()
    (void)cmd;  // Suppress unused parameter warning
}

std::string DataScriptRenderer::render_expression(const ir::expr* expr) {
    if (!expr) {
        return "";
    }
    return render_expr(*expr);
}

LanguageMetadata DataScriptRenderer::get_metadata() const {
    LanguageMetadata meta;
    meta.name = "DataScript";
    meta.version = "1.0";
    meta.file_extension = ".ds";
    meta.is_case_sensitive = true;
    meta.supports_generics = true;  // DataScript has parameterized types
    meta.supports_exceptions = false;
    return meta;
}

bool DataScriptRenderer::is_reserved_keyword(const std::string& identifier) const {
    return keywords_.count(identifier) > 0;
}

std::string DataScriptRenderer::sanitize_identifier(const std::string& identifier) const {
    if (is_reserved_keyword(identifier)) {
        return identifier + "_";
    }
    return identifier;
}

std::vector<std::string> DataScriptRenderer::get_all_keywords() const {
    return std::vector<std::string>(keywords_.begin(), keywords_.end());
}

std::string DataScriptRenderer::get_type_name(const ir::type_ref& type) const {
    std::ostringstream oss;

    // Endianness prefix
    if (type.byte_order.has_value()) {
        if (type.byte_order.value() == ir::endianness::little) {
            oss << "little ";
        } else if (type.byte_order.value() == ir::endianness::big) {
            oss << "big ";
        }
    }

    // Base type
    switch (type.kind) {
        case ir::type_kind::uint8: oss << "uint8"; break;
        case ir::type_kind::uint16: oss << "uint16"; break;
        case ir::type_kind::uint32: oss << "uint32"; break;
        case ir::type_kind::uint64: oss << "uint64"; break;
        case ir::type_kind::uint128: oss << "uint128"; break;
        case ir::type_kind::int8: oss << "int8"; break;
        case ir::type_kind::int16: oss << "int16"; break;
        case ir::type_kind::int32: oss << "int32"; break;
        case ir::type_kind::int64: oss << "int64"; break;
        case ir::type_kind::int128: oss << "int128"; break;
        case ir::type_kind::boolean: oss << "bool"; break;
        case ir::type_kind::string: oss << "string"; break;
        case ir::type_kind::bitfield:
            if (type.bit_width.has_value()) {
                oss << "bit<" << type.bit_width.value() << ">";
            } else {
                oss << "bit";
            }
            break;
        case ir::type_kind::array_fixed:
        case ir::type_kind::array_variable:
        case ir::type_kind::array_ranged:
            // Arrays handled separately
            break;
        default:
            oss << "UnknownType";
            break;
    }

    return oss.str();
}

// ============================================================================
// Component Rendering
// ============================================================================

void DataScriptRenderer::render_package(const std::string& name) {
    writer_->write_line("package " + name + ";");
}

void DataScriptRenderer::render_constants(const std::map<std::string, uint64_t>& constants) {
    for (const auto& [name, value] : constants) {
        writer_->write_line("const uint64 " + name + " = " + std::to_string(value) + ";");
    }
}

void DataScriptRenderer::render_enum(const ir::enum_def& e) {
    if (!e.documentation.empty()) {
        render_doc_comment(e.documentation);
    }

    std::ostringstream oss;
    oss << (e.is_bitmask ? "bitmask " : "enum ") << e.name << " : ";
    oss << get_type_name(e.base_type) << " {";
    writer_->write_line(oss.str());
    writer_->indent();

    for (const auto& item : e.items) {
        if (!item.documentation.empty()) {
            render_doc_comment(item.documentation);
        }
        writer_->write_line(item.name + " = " + std::to_string(item.value) + ",");
    }

    writer_->unindent();
    writer_->write_line("}");
}

void DataScriptRenderer::render_struct(const ir::struct_def& s) {
    if (!s.documentation.empty()) {
        render_doc_comment(s.documentation);
    }

    writer_->write_line("struct " + s.name + " {");
    writer_->indent();

    for (const auto& field : s.fields) {
        render_field(field);
    }

    writer_->unindent();
    writer_->write_line("}");
}

void DataScriptRenderer::render_union(const ir::union_def& u) {
    if (!u.documentation.empty()) {
        render_doc_comment(u.documentation);
    }

    writer_->write_line("union " + u.name + " {");
    writer_->indent();

    for (const auto& case_def : u.cases) {
        if (!case_def.documentation.empty()) {
            render_doc_comment(case_def.documentation);
        }

        for (const auto& field : case_def.fields) {
            render_field(field);
        }
    }

    writer_->unindent();
    writer_->write_line("}");
}

void DataScriptRenderer::render_choice(const ir::choice_def& c) {
    if (!c.documentation.empty()) {
        render_doc_comment(c.documentation);
    }

    std::ostringstream oss;
    oss << "choice " << c.name << "(";

    // Parameters
    bool first = true;
    for (const auto& param : c.parameters) {
        if (!first) oss << ", ";
        first = false;
        oss << get_type_name(param.type) << " " << param.name;
    }

    oss << ") on " << render_expr(c.selector) << " {";
    writer_->write_line(oss.str());
    writer_->indent();

    for (const auto& case_def : c.cases) {
        std::ostringstream case_oss;
        case_oss << "case ";

        // Case values
        bool first_val = true;
        for (const auto& val : case_def.case_values) {
            if (!first_val) case_oss << ", ";
            first_val = false;
            case_oss << render_expr(val);
        }

        case_oss << ": " << get_type_name(case_def.case_field.type)
                 << " " << case_def.case_field.name << ";";
        writer_->write_line(case_oss.str());
    }

    writer_->unindent();
    writer_->write_line("}");
}

void DataScriptRenderer::render_subtype(const ir::subtype_def& s) {
    if (!s.documentation.empty()) {
        render_doc_comment(s.documentation);
    }

    std::ostringstream oss;
    oss << "subtype " << s.name << " : " << get_type_name(s.base_type)
        << " : " << render_expr(s.constraint) << ";";
    writer_->write_line(oss.str());
}

void DataScriptRenderer::render_field(const ir::field& f) {
    if (!f.documentation.empty()) {
        render_doc_comment(f.documentation);
    }

    std::ostringstream oss;

    // Render type
    if (f.type.kind == ir::type_kind::array_fixed ||
        f.type.kind == ir::type_kind::array_variable ||
        f.type.kind == ir::type_kind::array_ranged) {
        // Array type - render element type first
        if (f.type.element_type) {
            oss << get_type_name(*f.type.element_type);
        }
        oss << " " << f.name;

        // Array size
        oss << "[";
        if (f.type.kind == ir::type_kind::array_fixed && f.type.array_size.has_value()) {
            oss << f.type.array_size.value();
        } else if (f.type.kind == ir::type_kind::array_variable && f.type.array_size_expr) {
            oss << render_expr(*f.type.array_size_expr);
        } else if (f.type.kind == ir::type_kind::array_ranged) {
            if (f.type.min_size_expr) {
                oss << render_expr(*f.type.min_size_expr);
            }
            oss << "..";
            if (f.type.max_size_expr) {
                oss << render_expr(*f.type.max_size_expr);
            }
        }
        oss << "]";
    } else {
        // Regular type
        oss << get_type_name(f.type) << " " << f.name;
    }

    // Conditional field
    if (f.condition == ir::field::runtime && f.runtime_condition.has_value()) {
        oss << " if (" << render_expr(f.runtime_condition.value()) << ")";
    }

    // Label
    if (f.label.has_value()) {
        oss << " @ " << render_expr(f.label.value());
    }

    // Alignment
    if (f.alignment.has_value()) {
        oss << " align(" << f.alignment.value() << ")";
    }

    // Inline constraint
    if (f.inline_constraint.has_value()) {
        oss << " : " << render_expr(f.inline_constraint.value());
    }

    oss << ";";
    writer_->write_line(oss.str());
}

std::string DataScriptRenderer::render_expr(const ir::expr& expr) {
    std::ostringstream oss;

    switch (expr.type) {
        case ir::expr::literal_int:
            oss << expr.int_value;
            break;

        case ir::expr::literal_bool:
            oss << (expr.bool_value ? "true" : "false");
            break;

        case ir::expr::literal_string:
            oss << "\"" << expr.string_value << "\"";
            break;

        case ir::expr::parameter_ref:
        case ir::expr::field_ref:
        case ir::expr::constant_ref:
            oss << expr.ref_name;
            break;

        case ir::expr::array_index:
            if (expr.left) {
                oss << render_expr(*expr.left) << "[";
                if (expr.right) {
                    oss << render_expr(*expr.right);
                }
                oss << "]";
            }
            break;

        case ir::expr::binary_op:
            if (expr.left && expr.right) {
                oss << "(" << render_expr(*expr.left) << " ";
                oss << get_binary_op_string(expr.op);
                oss << " " << render_expr(*expr.right) << ")";
            }
            break;

        case ir::expr::unary_op:
            if (expr.left) {
                oss << get_unary_op_string(expr.op);
                oss << "(" << render_expr(*expr.left) << ")";
            }
            break;

        case ir::expr::ternary_op:
            if (expr.condition && expr.true_expr && expr.false_expr) {
                oss << "(" << render_expr(*expr.condition) << " ? ";
                oss << render_expr(*expr.true_expr) << " : ";
                oss << render_expr(*expr.false_expr) << ")";
            }
            break;

        case ir::expr::function_call:
            oss << expr.ref_name << "(";
            // TODO: render function arguments
            oss << ")";
            break;
    }

    return oss.str();
}

std::string DataScriptRenderer::get_binary_op_string(ir::expr::op_type op) const {
    switch (op) {
        case ir::expr::add: return "+";
        case ir::expr::sub: return "-";
        case ir::expr::mul: return "*";
        case ir::expr::div: return "/";
        case ir::expr::mod: return "%";
        case ir::expr::eq: return "==";
        case ir::expr::ne: return "!=";
        case ir::expr::lt: return "<";
        case ir::expr::gt: return ">";
        case ir::expr::le: return "<=";
        case ir::expr::ge: return ">=";
        case ir::expr::logical_and: return "&&";
        case ir::expr::logical_or: return "||";
        case ir::expr::bit_and: return "&";
        case ir::expr::bit_or: return "|";
        case ir::expr::bit_xor: return "^";
        case ir::expr::bit_shift_left: return "<<";
        case ir::expr::bit_shift_right: return ">>";
        default: return "?";
    }
}

std::string DataScriptRenderer::get_unary_op_string(ir::expr::op_type op) const {
    switch (op) {
        case ir::expr::negate: return "-";
        case ir::expr::logical_not: return "!";
        case ir::expr::bit_not: return "~";
        default: return "?";
    }
}

void DataScriptRenderer::render_doc_comment(const std::string& doc) {
    if (doc.empty()) return;

    writer_->write_line("/**");

    // Split by newlines
    std::istringstream stream(doc);
    std::string line;
    while (std::getline(stream, line)) {
        writer_->write_line(" * " + line);
    }

    writer_->write_line(" */");
}

}  // namespace datascript::codegen

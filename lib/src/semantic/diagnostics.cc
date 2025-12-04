//
// Diagnostic formatting and utilities
//

#include <datascript/semantic.hh>
#include <sstream>
#include <algorithm>

namespace datascript::semantic {

// ============================================================================
// Diagnostic Formatting
// ============================================================================

std::string diagnostic::format(bool show_source_line) const {
    std::ostringstream oss;

    // Format: file:line:column: level: message [code]
    oss << position.file << ":"
        << position.line << ":"
        << position.column << ": ";

    switch (level) {
        case diagnostic_level::error:
            oss << "error: ";
            break;
        case diagnostic_level::warning:
            oss << "warning: ";
            break;
        case diagnostic_level::note:
            oss << "note: ";
            break;
        case diagnostic_level::hint:
            oss << "hint: ";
            break;
    }

    oss << message;

    // Add diagnostic code
    if (!code.empty()) {
        oss << " [" << code << "]";
    }

    oss << "\n";

    // Add related location if present
    if (related_position && related_message) {
        oss << related_position->file << ":"
            << related_position->line << ":"
            << related_position->column << ": note: "
            << related_message.value() << "\n";
    }

    // Add suggestion if present
    if (suggestion) {
        oss << "  suggestion: " << suggestion.value() << "\n";
    }

    return oss.str();
}

// ============================================================================
// Analysis Result Methods
// ============================================================================

bool analysis_result::has_errors() const {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const auto& d) { return d.level == diagnostic_level::error; });
}

bool analysis_result::has_warnings() const {
    return std::any_of(diagnostics.begin(), diagnostics.end(),
        [](const auto& d) { return d.level == diagnostic_level::warning; });
}

size_t analysis_result::error_count() const {
    return std::count_if(diagnostics.begin(), diagnostics.end(),
        [](const auto& d) { return d.level == diagnostic_level::error; });
}

size_t analysis_result::warning_count() const {
    return std::count_if(diagnostics.begin(), diagnostics.end(),
        [](const auto& d) { return d.level == diagnostic_level::warning; });
}

void analysis_result::print_diagnostics(std::ostream& os, bool use_color) const {
    for (const auto& diag : diagnostics) {
        os << diag.format(false);
    }

    // Summary
    if (!diagnostics.empty()) {
        size_t errors = error_count();
        size_t warnings = warning_count();

        os << "\n";
        if (errors > 0) {
            os << errors << " error" << (errors != 1 ? "s" : "");
        }
        if (warnings > 0) {
            if (errors > 0) os << ", ";
            os << warnings << " warning" << (warnings != 1 ? "s" : "");
        }
        os << " generated.\n";
    }
}

std::vector<diagnostic> analysis_result::get_errors() const {
    std::vector<diagnostic> result;
    std::copy_if(diagnostics.begin(), diagnostics.end(),
                std::back_inserter(result),
                [](const auto& d) { return d.level == diagnostic_level::error; });
    return result;
}

std::vector<diagnostic> analysis_result::get_warnings() const {
    std::vector<diagnostic> result;
    std::copy_if(diagnostics.begin(), diagnostics.end(),
                std::back_inserter(result),
                [](const auto& d) { return d.level == diagnostic_level::warning; });
    return result;
}

const module_symbols* analyzed_module_set::get_module_symbols(
    const std::string& package_name) const
{
    if (package_name.empty() || package_name == symbols.main.package_name) {
        return &symbols.main;
    }

    auto it = symbols.imported.find(package_name);
    return (it != symbols.imported.end()) ? &it->second : nullptr;
}

} // namespace datascript::semantic

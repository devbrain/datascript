#pragma once

#include <filesystem>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace datascript::codegen {

/// Option type enum
enum class OptionType {
    Bool,      // true/false, on/off, yes/no, 1/0
    String,    // Arbitrary string value
    Int,       // Integer value
    Choice     // One of predefined choices
};

/// Description of a generator-specific command-line option
struct OptionDescription {
    std::string name;                          // "exceptions", "typing", etc.
    OptionType type;
    std::string description;                   // Help text
    std::optional<std::string> default_value;  // Default if not specified
    std::vector<std::string> choices;          // For OptionType::Choice

    /// Check if this option is required (no default value)
    bool is_required() const {
        return !default_value.has_value();
    }
};

/// Type-safe variant for option values
using OptionValue = std::variant<bool, int64_t, std::string>;

/// Output file descriptor
struct OutputFile {
    std::filesystem::path path;  // Full path: output_dir + package_path + filename
    std::string content;         // Generated code content
};

}  // namespace datascript::codegen

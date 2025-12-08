//
// Symbol table lookup with Java-style scoping
//

#include <datascript/semantic.hh>
#include <algorithm>

namespace datascript::semantic {

namespace {
    // Helper: join vector of strings with separator
    std::string join(const std::vector<std::string>& parts, const std::string& sep) {
        if (parts.empty()) return "";

        std::string result = parts[0];
        for (size_t i = 1; i < parts.size(); ++i) {
            result += sep + parts[i];
        }
        return result;
    }
}

// ============================================================================
// Unqualified Lookup (Java-style)
// ============================================================================

// Lookup order:
// 1. Main module (unqualified)
// 2. Wildcard imports (unqualified)

const ast::struct_def* symbol_table::find_struct(const std::string& name) const {
    // 1. Check main module
    if (auto it = main.structs.find(name); it != main.structs.end()) {
        return it->second;
    }

    // 2. Check wildcard imports
    if (auto it = wildcard_structs.find(name); it != wildcard_structs.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::union_def* symbol_table::find_union(const std::string& name) const {
    if (auto it = main.unions.find(name); it != main.unions.end()) {
        return it->second;
    }

    if (auto it = wildcard_unions.find(name); it != wildcard_unions.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::enum_def* symbol_table::find_enum(const std::string& name) const {
    if (auto it = main.enums.find(name); it != main.enums.end()) {
        return it->second;
    }

    if (auto it = wildcard_enums.find(name); it != wildcard_enums.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::subtype_def* symbol_table::find_subtype(const std::string& name) const {
    if (auto it = main.subtypes.find(name); it != main.subtypes.end()) {
        return it->second;
    }

    if (auto it = wildcard_subtypes.find(name); it != wildcard_subtypes.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::type_alias_def* symbol_table::find_type_alias(const std::string& name) const {
    if (auto it = main.type_aliases.find(name); it != main.type_aliases.end()) {
        return it->second;
    }

    if (auto it = wildcard_type_aliases.find(name); it != wildcard_type_aliases.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::choice_def* symbol_table::find_choice(const std::string& name) const {
    if (auto it = main.choices.find(name); it != main.choices.end()) {
        return it->second;
    }

    if (auto it = wildcard_choices.find(name); it != wildcard_choices.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::constant_def* symbol_table::find_constant(const std::string& name) const {
    if (auto it = main.constants.find(name); it != main.constants.end()) {
        return it->second;
    }

    if (auto it = wildcard_constants.find(name); it != wildcard_constants.end()) {
        return it->second;
    }

    return nullptr;
}

const ast::constraint_def* symbol_table::find_constraint(const std::string& name) const {
    if (auto it = main.constraints.find(name); it != main.constraints.end()) {
        return it->second;
    }

    if (auto it = wildcard_constraints.find(name); it != wildcard_constraints.end()) {
        return it->second;
    }

    return nullptr;
}

// ============================================================================
// Qualified Lookup
// ============================================================================

// qualified_name: ["foo", "bar", "Point"] -> foo.bar.Point

const ast::struct_def* symbol_table::find_struct_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    // Last part is the type name
    const std::string& type_name = qualified_name.back();

    // Everything before is the package
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        // Unqualified: just "Point"
        return find_struct(type_name);
    }

    // Qualified: "foo.bar.Point"
    std::string pkg = join(package_parts, ".");

    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;  // Package not found
    }

    auto it = pkg_it->second.structs.find(type_name);
    return (it != pkg_it->second.structs.end()) ? it->second : nullptr;
}

const ast::union_def* symbol_table::find_union_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    const std::string& type_name = qualified_name.back();
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        return find_union(type_name);
    }

    std::string pkg = join(package_parts, ".");
    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;
    }

    auto it = pkg_it->second.unions.find(type_name);
    return (it != pkg_it->second.unions.end()) ? it->second : nullptr;
}

const ast::enum_def* symbol_table::find_enum_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    const std::string& type_name = qualified_name.back();
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        return find_enum(type_name);
    }

    std::string pkg = join(package_parts, ".");
    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;
    }

    auto it = pkg_it->second.enums.find(type_name);
    return (it != pkg_it->second.enums.end()) ? it->second : nullptr;
}

const ast::subtype_def* symbol_table::find_subtype_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    const std::string& type_name = qualified_name.back();
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        return find_subtype(type_name);
    }

    std::string pkg = join(package_parts, ".");
    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;
    }

    auto it = pkg_it->second.subtypes.find(type_name);
    return (it != pkg_it->second.subtypes.end()) ? it->second : nullptr;
}

const ast::type_alias_def* symbol_table::find_type_alias_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    const std::string& type_name = qualified_name.back();
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        return find_type_alias(type_name);
    }

    std::string pkg = join(package_parts, ".");
    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;
    }

    auto it = pkg_it->second.type_aliases.find(type_name);
    return (it != pkg_it->second.type_aliases.end()) ? it->second : nullptr;
}

const ast::choice_def* symbol_table::find_choice_qualified(
    const std::vector<std::string>& qualified_name) const
{
    if (qualified_name.empty()) return nullptr;

    const std::string& type_name = qualified_name.back();
    std::vector<std::string> package_parts(
        qualified_name.begin(),
        qualified_name.end() - 1);

    if (package_parts.empty()) {
        return find_choice(type_name);
    }

    std::string pkg = join(package_parts, ".");
    auto pkg_it = imported.find(pkg);
    if (pkg_it == imported.end()) {
        return nullptr;
    }

    auto it = pkg_it->second.choices.find(type_name);
    return (it != pkg_it->second.choices.end()) ? it->second : nullptr;
}

// ============================================================================
// Module Lookup
// ============================================================================

const module_symbols* symbol_table::get_module_symbols(
    const std::string& package_name) const
{
    if (package_name.empty() || package_name == main.package_name) {
        return &main;
    }

    auto it = imported.find(package_name);
    return (it != imported.end()) ? &it->second : nullptr;
}

} // namespace datascript::semantic

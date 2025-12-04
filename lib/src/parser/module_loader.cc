//
// Module loading implementation for DataScript parser
//

#include "datascript/parser.hh"
#include "datascript/parser_error.hh"
#include <filesystem>
#include <fstream>
#include <sstream>
#include <queue>
#include <set>
#include <algorithm>

namespace fs = std::filesystem;

namespace datascript {

// Build error message for import_not_found_error
std::string import_not_found_error::build_message(
    const std::string& name,
    const std::vector<std::string>& paths) {
    std::ostringstream oss;
    oss << "Import '" << name << "' not found. Searched in:\n";
    for (const auto& path : paths) {
        oss << "  - " << path << "\n";
    }
    return oss.str();
}

namespace {
    // Helper: Convert package name to file path
    // "foo.bar.baz" -> "foo/bar/baz.ds"
    std::string package_to_path(const std::vector<std::string>& name_parts) {
        if (name_parts.empty()) {
            return "";
        }

        std::string path = name_parts[0];
        for (size_t i = 1; i < name_parts.size(); ++i) {
            path += "/" + name_parts[i];
        }
        path += ".ds";
        return path;
    }

    // Helper: Convert package name parts to dot-separated string
    // ["foo", "bar", "baz"] -> "foo.bar.baz"
    std::string package_to_string(const std::vector<std::string>& name_parts) {
        if (name_parts.empty()) {
            return "";
        }

        std::string result = name_parts[0];
        for (size_t i = 1; i < name_parts.size(); ++i) {
            result += "." + name_parts[i];
        }
        return result;
    }

    // Helper: Get package name from module's package declaration
    // Returns empty string if no package declaration
    std::string get_package_name(const ast::module& mod) {
        if (mod.package) {
            return package_to_string(mod.package->name_parts);
        }
        return "";
    }

    // Helper: Resolve a single import path to a file
    // Returns canonical path if found, empty string otherwise
    std::string resolve_import(
        const std::vector<std::string>& import_parts,
        const std::vector<std::string>& search_paths,
        std::vector<std::string>& searched_paths) {

        std::string rel_path = package_to_path(import_parts);

        for (const auto& search_dir : search_paths) {
            fs::path candidate = fs::path(search_dir) / rel_path;
            searched_paths.push_back(candidate.string());

            if (fs::exists(candidate) && fs::is_regular_file(candidate)) {
                return fs::canonical(candidate).string();
            }
        }

        return "";
    }

    // Helper: Resolve wildcard import (e.g., "foo.*")
    // Returns list of canonical paths to all .ds files in the directory
    std::vector<std::string> resolve_wildcard_import(
        const std::vector<std::string>& import_parts,
        const std::vector<std::string>& search_paths) {

        std::vector<std::string> results;

        // Build directory path from import parts (remove the "*")
        if (import_parts.empty()) {
            return results;
        }

        std::string dir_path = import_parts[0];
        for (size_t i = 1; i < import_parts.size(); ++i) {
            dir_path += "/" + import_parts[i];
        }

        // Search for directory in search paths
        for (const auto& search_dir : search_paths) {
            fs::path candidate_dir = fs::path(search_dir) / dir_path;

            if (fs::exists(candidate_dir) && fs::is_directory(candidate_dir)) {
                // Find all .ds files in this directory (non-recursive)
                for (const auto& entry : fs::directory_iterator(candidate_dir)) {
                    if (entry.is_regular_file() && entry.path().extension() == ".ds") {
                        results.push_back(fs::canonical(entry.path()).string());
                    }
                }

                // Found the directory, don't search other search paths
                break;
            }
        }

        return results;
    }

    // Helper: Parse a single file and return loaded_module
    loaded_module parse_file(const std::string& file_path) {
        loaded_module result;
        result.file_path = file_path;
        result.module = parse_datascript(fs::path(file_path));
        result.package_name = get_package_name(result.module);
        return result;
    }

} // anonymous namespace

// Main module loading function
module_set load_modules_with_imports(
    const std::string& main_script_path,
    const std::vector<std::string>& user_search_paths) {

    module_set result;

    // 1. Construct search paths
    std::vector<std::string> search_paths;

    // a) Main script directory (highest priority)
    fs::path main_path = fs::path(main_script_path);
    if (main_path.has_parent_path()) {
        search_paths.push_back(main_path.parent_path().string());
    } else {
        search_paths.push_back(".");
    }

    // b) User-provided search paths
    search_paths.insert(search_paths.end(), user_search_paths.begin(), user_search_paths.end());

    // c) Current working directory (if not already included)
    std::string cwd = fs::current_path().string();
    if (std::find(search_paths.begin(), search_paths.end(), cwd) == search_paths.end()) {
        search_paths.push_back(cwd);
    }

    // d) DATASCRIPT_PATH environment variable
    const char* env_path = std::getenv("DATASCRIPT_PATH");
    if (env_path) {
        std::string env_str(env_path);
        size_t pos = 0;
        while (pos < env_str.size()) {
            size_t next = env_str.find(':', pos);
            if (next == std::string::npos) {
                next = env_str.size();
            }
            std::string path = env_str.substr(pos, next - pos);
            if (!path.empty()) {
                search_paths.push_back(path);
            }
            pos = next + 1;
        }
    }

    // 2. Parse main module
    result.main = parse_file(fs::canonical(main_script_path).string());

    // 3. BFS traversal for imports
    // Use indices instead of pointers to avoid invalidation when result.imported reallocates
    // Index format: SIZE_MAX = main module, 0+ = index into result.imported
    std::queue<size_t> to_process;
    std::set<std::string> seen_files;  // Canonical paths
    std::vector<std::string> import_chain;  // For circular import detection

    to_process.push(SIZE_MAX);  // Start with main module
    seen_files.insert(result.main.file_path);

    while (!to_process.empty()) {
        size_t current_idx = to_process.front();
        to_process.pop();

        // Get the current module (either main or from imported vector)
        ast::module* current = (current_idx == SIZE_MAX)
            ? &result.main.module
            : &result.imported[current_idx].module;

        // Process each import in this module
        for (const auto& import : current->imports) {
            if (import.is_wildcard) {
                // Wildcard import: load all .ds files in directory
                auto files = resolve_wildcard_import(import.name_parts, search_paths);

                for (const auto& file_path : files) {
                    // Skip if already seen
                    if (seen_files.count(file_path)) {
                        continue;
                    }

                    // Parse and add to result
                    loaded_module imported = parse_file(file_path);
                    seen_files.insert(file_path);

                    // Add to package index if it has a package name
                    if (!imported.package_name.empty()) {
                        result.package_index[imported.package_name] = result.imported.size();
                    }

                    result.imported.push_back(std::move(imported));
                    to_process.push(result.imported.size() - 1);  // Push index, not pointer
                }
            } else {
                // Regular import: resolve to a single file
                std::vector<std::string> searched_paths;
                std::string file_path = resolve_import(import.name_parts, search_paths, searched_paths);

                if (file_path.empty()) {
                    throw import_not_found_error(
                        package_to_string(import.name_parts),
                        searched_paths
                    );
                }

                // Check for circular imports
                if (seen_files.count(file_path)) {
                    continue;  // Already loaded, skip
                }

                // Parse and add to result
                loaded_module imported = parse_file(file_path);
                seen_files.insert(file_path);

                // Add to package index
                std::string pkg_name = package_to_string(import.name_parts);
                result.package_index[pkg_name] = result.imported.size();

                result.imported.push_back(std::move(imported));
                to_process.push(result.imported.size() - 1);  // Push index, not pointer
            }
        }
    }

    return result;
}

} // namespace datascript

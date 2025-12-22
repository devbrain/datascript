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

// Build error message for package_path_mismatch_error
std::string package_path_mismatch_error::build_message(
    const std::string& file_path,
    const std::string& package_name,
    const std::string& expected_path) {
    std::ostringstream oss;
    oss << "Package declaration mismatch:\n";
    oss << "  File location: " << file_path << "\n";
    oss << "  Package declared: '" << package_name << "'\n";
    oss << "  Expected file location: " << expected_path << "\n";
    oss << "\nDataScript requires file paths to match package declarations.\n";
    oss << "Please either:\n";
    oss << "  1. Move the file to: " << expected_path << "\n";
    oss << "  2. Change the package declaration to match the file location";
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

    // Helper: Validate that file path matches package declaration
    void validate_package_path(const std::string& file_path, const std::string& package_name) {
        if (package_name.empty()) {
            // No package declaration - no validation needed
            return;
        }

        // Convert package name to expected relative path
        // "foo.bar.baz" -> "foo/bar/baz.ds"
        std::string package_as_path = package_name;
        std::replace(package_as_path.begin(), package_as_path.end(), '.', '/');
        package_as_path += ".ds";

        // Get the file path as a fs::path for manipulation
        fs::path actual_path(file_path);

        // Extract the relative path that should match the package
        // We need to check if the file path ends with the expected package path
        std::string actual_str = actual_path.string();

        // Normalize separators for comparison (handle both / and \)
        std::replace(actual_str.begin(), actual_str.end(), '\\', '/');

        // Check if the actual path ends with the expected package path
        if (actual_str.length() >= package_as_path.length()) {
            std::string ending = actual_str.substr(actual_str.length() - package_as_path.length());
            if (ending == package_as_path) {
                // Path matches package declaration
                return;
            }
        }

        // Path doesn't match - construct expected full path for error message
        // Try to determine the base directory by removing the mismatched ending
        std::string expected_path;
        if (actual_path.has_parent_path()) {
            // Keep parent directories and append correct package path
            fs::path parent = actual_path.parent_path();
            // Try to find a reasonable base directory
            // This is best effort - we show what the path should look like
            expected_path = (parent / package_as_path).string();
        } else {
            expected_path = package_as_path;
        }

        throw package_path_mismatch_error(file_path, package_name, expected_path);
    }

    // Helper: Parse a single file and return loaded_module
    loaded_module parse_file(const std::string& file_path) {
        loaded_module result;
        result.file_path = file_path;
        result.module = parse_datascript(fs::path(file_path));
        result.package_name = get_package_name(result.module);

        // Validate that file path matches package declaration
        validate_package_path(file_path, result.package_name);

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
    // Suppress deprecation warning for getenv (safe for reading env vars)
#if defined(__clang__)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdeprecated-declarations"
#elif defined(_MSC_VER)
#pragma warning(push)
#pragma warning(disable: 4996)
#endif
    const char* env_path = std::getenv("DATASCRIPT_PATH");
#if defined(__clang__)
#pragma clang diagnostic pop
#elif defined(_MSC_VER)
#pragma warning(pop)
#endif
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

                // Add to package index if it has a package name
                if (!imported.package_name.empty()) {
                    result.package_index[imported.package_name] = result.imported.size();
                }

                result.imported.push_back(std::move(imported));
                to_process.push(result.imported.size() - 1);  // Push index, not pointer
            }
        }
    }

    return result;
}

} // namespace datascript

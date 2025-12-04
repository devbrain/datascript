//
// Created by igor on 20/11/2025.
//

#pragma once

#include <filesystem>
#include <iosfwd>
#include <string>
#include <vector>
#include <map>

#include "ast.hh"
#include "parser_error.hh"

namespace datascript {
    // Basic parsing functions
    ast::module parse_datascript(const std::filesystem::path& path);
    ast::module parse_datascript(const std::string& txt);

    // Module loading structures
    struct loaded_module {
        std::string file_path;        // Canonical path to the file
        ast::module module;            // Parsed AST
        std::string package_name;      // Full package name (e.g., "foo.bar.baz")
    };

    struct module_set {
        loaded_module main;                          // The main/entry module
        std::vector<loaded_module> imported;         // All transitively imported modules
        std::map<std::string, size_t> package_index; // Map package name → index in 'imported'
    };

    // Module loading API
    // Exception types: module_load_error, circular_import_error, import_not_found_error
    // (defined in parser_error.hh)
    module_set load_modules_with_imports(
        const std::string& main_script_path,
        const std::vector<std::string>& user_search_paths = {}
    );
}
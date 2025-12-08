//
// Created by igor on 27/11/2025.
//

#pragma once
#include <stdexcept>
#include <string>
#include <vector>

namespace datascript {
    class parse_error : public std::runtime_error {
        public:
            parse_error(const std::string& msg, int line, int column)
                : std::runtime_error(msg), line_(line), column_(column) {
            }

            [[nodiscard]] int line() const { return line_; }
            [[nodiscard]] int column() const { return column_; }

        private:
            int line_;
            int column_;
    };

    // Module loading exception types
    class module_load_error : public std::runtime_error {
    public:
        explicit module_load_error(const std::string& msg)
            : std::runtime_error(msg) {}
    };

    class circular_import_error : public module_load_error {
    public:
        explicit circular_import_error(const std::string& msg)
            : module_load_error(msg) {}
    };

    class import_not_found_error : public module_load_error {
    public:
        import_not_found_error(const std::string& import_name,
                              const std::vector<std::string>& searched_paths)
            : module_load_error(build_message(import_name, searched_paths)),
              import_name_(import_name),
              searched_paths_(searched_paths) {}

        const std::string& import_name() const { return import_name_; }
        const std::vector<std::string>& searched_paths() const { return searched_paths_; }

    private:
        std::string import_name_;
        std::vector<std::string> searched_paths_;

        static std::string build_message(const std::string& name,
                                         const std::vector<std::string>& paths);
    };

    class package_path_mismatch_error : public module_load_error {
    public:
        package_path_mismatch_error(const std::string& file_path,
                                   const std::string& package_name,
                                   const std::string& expected_path)
            : module_load_error(build_message(file_path, package_name, expected_path)),
              file_path_(file_path),
              package_name_(package_name),
              expected_path_(expected_path) {}

        const std::string& file_path() const { return file_path_; }
        const std::string& package_name() const { return package_name_; }
        const std::string& expected_path() const { return expected_path_; }

    private:
        std::string file_path_;
        std::string package_name_;
        std::string expected_path_;

        static std::string build_message(const std::string& file_path,
                                         const std::string& package_name,
                                         const std::string& expected_path);
    };
}
//
// Tests for Keyword Collision Validation (Phase 1)
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>

using namespace datascript;
using namespace datascript::semantic;

namespace {
    // Helper: create a module_set from a single source string
    module_set make_module_set(const std::string& source) {
        auto main_mod = parse_datascript(source);

        module_set modules;
        modules.main.module = std::move(main_mod);
        modules.main.file_path = "<test>";
        modules.main.package_name = "";  // Default package

        return modules;
    }
}

TEST_SUITE("Semantic - Keyword Validation") {

    TEST_CASE("Constant name conflicts with C++ keyword") {
        const char* schema = R"(
            const uint32 class = 42;
        )";

        auto modules = make_module_set(schema);

        // Analyze with default options (checks all registered languages)
        analysis_options opts;
        auto result = analyze(modules, opts);

        // Should have warnings (keyword collision + unused constant)
        CHECK(result.has_warnings());
        CHECK(result.warning_count() >= 1);

        // Find the keyword collision warning
        bool found_keyword_warning = false;
        for (const auto& w : result.get_warnings()) {
            if (w.code == diag_codes::W_KEYWORD_COLLISION &&
                w.message.find("Constant 'class'") != std::string::npos) {
                found_keyword_warning = true;
                CHECK(w.message.find("cpp") != std::string::npos);
                CHECK(w.suggestion.has_value());
                CHECK(w.suggestion->find("class_") != std::string::npos);
            }
        }
        CHECK(found_keyword_warning);
    }

    TEST_CASE("Multiple C++ keyword identifiers") {
        const char* schema = R"(
            const uint32 class = 1;
            const uint32 operator = 2;
            const uint32 explicit = 3;
        )";

        auto modules = make_module_set(schema);

        analysis_options opts;
        auto result = analyze(modules, opts);

        CHECK(result.has_warnings());

        // Count keyword collision warnings
        int keyword_warnings = 0;
        for (const auto& w : result.get_warnings()) {
            if (w.code == diag_codes::W_KEYWORD_COLLISION) {
                keyword_warnings++;
            }
        }
        // Should have 3 keyword collision warnings
        CHECK(keyword_warnings == 3);
    }

    TEST_CASE("No warning for non-keyword identifiers") {
        const char* schema = R"(
            const uint32 SIZE = 100;
            const uint32 MAX_VALUE = 255;
        )";

        auto modules = make_module_set(schema);

        analysis_options opts;
        auto result = analyze(modules, opts);

        // Should have no keyword collision warnings (may have unused warnings)
        for (const auto& w : result.get_warnings()) {
            CHECK(w.code != diag_codes::W_KEYWORD_COLLISION);
        }
    }

    TEST_CASE("Keyword validation can be disabled") {
        const char* schema = R"(
            const uint32 class = 42;
        )";

        auto modules = make_module_set(schema);

        // Disable keyword collision warnings
        analysis_options opts;
        opts.disabled_warnings.insert(diag_codes::W_KEYWORD_COLLISION);
        auto result = analyze(modules, opts);

        // Should have no keyword collision warnings
        for (const auto& w : result.get_warnings()) {
            CHECK(w.code != diag_codes::W_KEYWORD_COLLISION);
        }
    }

    TEST_CASE("Target languages filter - only check specified languages") {
        const char* schema = R"(
            const uint32 class = 42;
        )";

        auto modules = make_module_set(schema);

        // Only check C++ (class is a keyword)
        analysis_options opts;
        opts.target_languages.insert("cpp");
        auto result = analyze(modules, opts);

        CHECK(result.has_warnings());

        // Check for the keyword collision warning with cpp
        bool found_cpp_warning = false;
        for (const auto& w : result.get_warnings()) {
            if (w.code == diag_codes::W_KEYWORD_COLLISION &&
                w.message.find("Constant 'class'") != std::string::npos) {
                found_cpp_warning = true;
                CHECK(w.message.find("cpp") != std::string::npos);
            }
        }
        CHECK(found_cpp_warning);
    }

    TEST_CASE("Sanitization suggestion for keyword collision") {
        const char* schema = R"(
            const uint32 delete = 10;
        )";

        auto modules = make_module_set(schema);

        analysis_options opts;
        auto result = analyze(modules, opts);

        // Find the keyword collision warning and check suggestion
        bool found_suggestion = false;
        for (const auto& w : result.get_warnings()) {
            if (w.code == diag_codes::W_KEYWORD_COLLISION &&
                w.message.find("Constant 'delete'") != std::string::npos) {
                CHECK(w.suggestion.has_value());
                // Should suggest "delete_" (trailing underscore)
                CHECK(w.suggestion->find("delete_") != std::string::npos);
                found_suggestion = true;
            }
        }
        CHECK(found_suggestion);
    }

    TEST_CASE("Unknown target language error - single language") {
        const char* schema = R"(
            const uint32 value = 42;
        )";

        auto modules = make_module_set(schema);

        // Request an unknown language
        analysis_options opts;
        opts.target_languages.insert("rust");  // Not registered
        auto result = analyze(modules, opts);

        // Should have error about unknown language
        CHECK(result.has_errors());

        bool found_unknown_lang_error = false;
        for (const auto& e : result.get_errors()) {
            if (e.code == diag_codes::E_UNKNOWN_TARGET_LANGUAGE &&
                e.message.find("Unknown target language") != std::string::npos) {
                found_unknown_lang_error = true;
                CHECK(e.message.find("rust") != std::string::npos);
                CHECK(e.suggestion.has_value());
                CHECK(e.suggestion->find("Available languages") != std::string::npos);
                CHECK(e.suggestion->find("cpp") != std::string::npos);
            }
        }
        CHECK(found_unknown_lang_error);
    }

    TEST_CASE("Unknown target language error - multiple languages") {
        const char* schema = R"(
            const uint32 value = 42;
        )";

        auto modules = make_module_set(schema);

        // Request multiple unknown languages
        analysis_options opts;
        opts.target_languages.insert("rust");
        opts.target_languages.insert("python");
        opts.target_languages.insert("go");
        auto result = analyze(modules, opts);

        // Should have error about unknown languages
        CHECK(result.has_errors());

        bool found_unknown_lang_error = false;
        for (const auto& e : result.get_errors()) {
            if (e.code == diag_codes::E_UNKNOWN_TARGET_LANGUAGE &&
                e.message.find("Unknown target language") != std::string::npos) {
                found_unknown_lang_error = true;
                // Should list all unknown languages
                CHECK(e.message.find("rust") != std::string::npos);
                CHECK(e.message.find("python") != std::string::npos);
                CHECK(e.message.find("go") != std::string::npos);
                CHECK(e.suggestion.has_value());
                CHECK(e.suggestion->find("Available languages") != std::string::npos);
            }
        }
        CHECK(found_unknown_lang_error);
    }

    TEST_CASE("Mixed known and unknown target languages") {
        const char* schema = R"(
            const uint32 class = 42;
        )";

        auto modules = make_module_set(schema);

        // Request mix of known and unknown languages
        analysis_options opts;
        opts.target_languages.insert("cpp");    // Known
        opts.target_languages.insert("rust");   // Unknown
        auto result = analyze(modules, opts);

        // Should have errors for unknown language (no keyword checking happens due to error)
        CHECK(result.has_errors());

        bool found_unknown_lang_error = false;

        for (const auto& e : result.get_errors()) {
            if (e.code == diag_codes::E_UNKNOWN_TARGET_LANGUAGE &&
                e.message.find("Unknown target language") != std::string::npos) {
                found_unknown_lang_error = true;
                CHECK(e.message.find("rust") != std::string::npos);
                // Should NOT mention 'cpp' in the unknown languages list (it's known)
                // But 'cpp' might appear in the "Available languages" suggestion
                CHECK(e.suggestion.has_value());
            }
        }

        CHECK(found_unknown_lang_error);
    }

    TEST_CASE("No error for valid target language") {
        const char* schema = R"(
            const uint32 value = 42;
        )";

        auto modules = make_module_set(schema);

        // Request only known language
        analysis_options opts;
        opts.target_languages.insert("cpp");
        auto result = analyze(modules, opts);

        // Should NOT have unknown language error
        for (const auto& e : result.get_errors()) {
            CHECK(e.code != diag_codes::E_UNKNOWN_TARGET_LANGUAGE);
        }
    }
}

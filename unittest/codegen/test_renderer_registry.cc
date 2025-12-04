//
// Tests for Renderer Registry
//

#include <doctest/doctest.h>
#include <datascript/renderer_registry.hh>
#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <algorithm>

using namespace datascript;
using namespace datascript::codegen;

TEST_SUITE("Codegen - Renderer Registry") {

    TEST_CASE("Singleton instance access") {
        auto& registry1 = RendererRegistry::instance();
        auto& registry2 = RendererRegistry::instance();

        // Should return the same instance
        CHECK(&registry1 == &registry2);
    }

    TEST_CASE("CppRenderer automatically registered") {
        auto& registry = RendererRegistry::instance();

        // C++ renderer should be automatically registered
        CHECK(registry.has_renderer("cpp"));
        CHECK(registry.has_renderer("CPP"));  // Case-insensitive
        CHECK(registry.has_renderer("Cpp"));

        // Should be able to retrieve it
        auto* renderer = registry.get_renderer("cpp");
        CHECK(renderer != nullptr);
    }

    TEST_CASE("Case-insensitive language lookup") {
        auto& registry = RendererRegistry::instance();

        auto* cpp1 = registry.get_renderer("cpp");
        auto* cpp2 = registry.get_renderer("CPP");
        auto* cpp3 = registry.get_renderer("Cpp");
        auto* cpp4 = registry.get_renderer("CpP");

        // All should return the same renderer instance
        CHECK(cpp1 == cpp2);
        CHECK(cpp2 == cpp3);
        CHECK(cpp3 == cpp4);
    }

    TEST_CASE("Get available languages") {
        auto& registry = RendererRegistry::instance();

        auto languages = registry.get_available_languages();

        // Should have at least C++
        CHECK(!languages.empty());
        CHECK(std::find(languages.begin(), languages.end(), "cpp") != languages.end());
    }

    TEST_CASE("Get language metadata") {
        auto& registry = RendererRegistry::instance();

        auto metadata = registry.get_language_metadata("cpp");

        REQUIRE(metadata.has_value());
        CHECK(metadata->name == "C++");
        CHECK(metadata->version == "C++20");
        CHECK(metadata->file_extension == ".cc");
        CHECK(metadata->is_case_sensitive == true);
        CHECK(metadata->supports_generics == true);
        CHECK(metadata->supports_exceptions == true);
    }

    TEST_CASE("Get language metadata for unknown language") {
        auto& registry = RendererRegistry::instance();

        auto metadata = registry.get_language_metadata("rust");

        CHECK(!metadata.has_value());
    }

    TEST_CASE("Keyword validation - specific language") {
        auto& registry = RendererRegistry::instance();

        // C++ keywords
        CHECK(registry.is_keyword("cpp", "class"));
        CHECK(registry.is_keyword("cpp", "struct"));
        CHECK(registry.is_keyword("cpp", "namespace"));
        CHECK(registry.is_keyword("cpp", "template"));

        // Not C++ keywords
        CHECK(!registry.is_keyword("cpp", "Class"));  // C++ is case-sensitive
        CHECK(!registry.is_keyword("cpp", "my_var"));
        CHECK(!registry.is_keyword("cpp", "foo"));
    }

    TEST_CASE("Keyword validation - any language") {
        auto& registry = RendererRegistry::instance();

        // Keywords that exist in C++
        CHECK(registry.is_keyword_in_any_language("class"));
        CHECK(registry.is_keyword_in_any_language("if"));
        CHECK(registry.is_keyword_in_any_language("for"));
        CHECK(registry.is_keyword_in_any_language("while"));

        // Not keywords in any language
        CHECK(!registry.is_keyword_in_any_language("my_identifier"));
        CHECK(!registry.is_keyword_in_any_language("foo_bar"));
    }

    TEST_CASE("Get conflicting languages") {
        auto& registry = RendererRegistry::instance();

        auto conflicts = registry.get_conflicting_languages("class");

        // Should conflict with C++
        CHECK(!conflicts.empty());
        CHECK(std::find(conflicts.begin(), conflicts.end(), "cpp") != conflicts.end());

        // Non-keyword should have no conflicts
        auto no_conflicts = registry.get_conflicting_languages("my_var");
        CHECK(no_conflicts.empty());
    }

    TEST_CASE("Get all keywords") {
        auto& registry = RendererRegistry::instance();

        auto all_keywords = registry.get_all_keywords();

        // Should have at least C++ keywords (92 total in C++20)
        CHECK(all_keywords.size() >= 92);

        // Should contain C++ keywords
        CHECK(all_keywords.count("class") > 0);
        CHECK(all_keywords.count("struct") > 0);
        CHECK(all_keywords.count("namespace") > 0);
        CHECK(all_keywords.count("template") > 0);
        CHECK(all_keywords.count("virtual") > 0);
    }

    TEST_CASE("Get renderer for unknown language") {
        auto& registry = RendererRegistry::instance();

        auto* unknown = registry.get_renderer("python");
        CHECK(unknown == nullptr);

        CHECK(!registry.has_renderer("python"));
        CHECK(!registry.has_renderer("rust"));
        CHECK(!registry.has_renderer("go"));
    }

    TEST_CASE("Renderer provides correct interface") {
        auto& registry = RendererRegistry::instance();

        auto* renderer = registry.get_renderer("cpp");
        REQUIRE(renderer != nullptr);

        // Check interface methods are available
        CHECK(renderer->get_language_name() == "C++");
        CHECK(renderer->get_file_extension() == ".cc");
        CHECK(renderer->is_case_sensitive() == true);

        // Check keyword validation
        CHECK(renderer->is_reserved_keyword("class"));
        CHECK(!renderer->is_reserved_keyword("my_var"));

        // Check sanitization
        CHECK(renderer->sanitize_identifier("class") == "class_");
        CHECK(renderer->sanitize_identifier("my_var") == "my_var");
    }

    TEST_CASE("Get all C++ keywords via registry") {
        auto& registry = RendererRegistry::instance();

        auto* renderer = registry.get_renderer("cpp");
        REQUIRE(renderer != nullptr);

        auto keywords = renderer->get_all_keywords();

        // C++ has 92 keywords in C++20 (includes alternative tokens)
        CHECK(keywords.size() == 92);

        // Spot check some keywords
        CHECK(std::find(keywords.begin(), keywords.end(), "alignas") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "class") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "const") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "constexpr") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "namespace") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "nullptr") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "template") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "typename") != keywords.end());
        CHECK(std::find(keywords.begin(), keywords.end(), "virtual") != keywords.end());
    }
}

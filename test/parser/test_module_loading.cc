//
// Tests for module loading with imports
//

#include "doctest/doctest.h"
#include "datascript/parser.hh"
#include "datascript/parser_error.hh"
#include <filesystem>
#include <fstream>
#include <set>

using namespace datascript;
namespace fs = std::filesystem;

TEST_SUITE("Module Loading") {

    // Helper to get test data path
    static std::string get_test_data_path() {
        return UNITTEST_HOME "/testdata/modules/"; // Determined in CMakeLists.txt
    }

    TEST_CASE("load_modules_with_imports - basic import") {
        std::string main_path = get_test_data_path() + "test/main.ds";

        // Provide base directory as search path for resolving test.* imports
        std::vector<std::string> search_paths = { get_test_data_path() };

        // Should load main.ds and its imports (test.foo, test.bar, test.baz)
        module_set mods = load_modules_with_imports(main_path, search_paths);

        // Check main module
        REQUIRE(mods.main.package_name == "test.main");
        REQUIRE(mods.main.module.constants.size() == 1);
        REQUIRE(mods.main.module.constants[0].name == "MAIN_CONST");

        // Check that we have 3 imported modules (foo, bar, baz)
        CHECK(mods.imported.size() == 3);

        // Check package index
        CHECK(mods.package_index.count("test.foo") == 1);
        CHECK(mods.package_index.count("test.bar") == 1);
        CHECK(mods.package_index.count("test.baz") == 1);

        // Verify each module
        for (const auto& mod : mods.imported) {
            if (mod.package_name == "test.foo") {
                REQUIRE(mod.module.constants.size() == 1);
                CHECK(mod.module.constants[0].name == "FOO_CONST");
            } else if (mod.package_name == "test.bar") {
                REQUIRE(mod.module.constants.size() == 1);
                CHECK(mod.module.constants[0].name == "BAR_CONST");
            } else if (mod.package_name == "test.baz") {
                REQUIRE(mod.module.constants.size() == 1);
                CHECK(mod.module.constants[0].name == "BAZ_CONST");
            }
        }
    }

    TEST_CASE("load_modules_with_imports - wildcard import") {
        std::string wildcard_path = get_test_data_path() + "test/wildcard.ds";

        // Provide base directory as search path for resolving test.* imports
        std::vector<std::string> search_paths = { get_test_data_path() };

        // Should load wildcard.ds and all .ds files in test/ directory
        module_set mods = load_modules_with_imports(wildcard_path, search_paths);

        // Check main module
        REQUIRE(mods.main.package_name == "test.wildcard");
        REQUIRE(mods.main.module.constants.size() == 1);

        // Should have loaded foo.ds, bar.ds, baz.ds from test/* wildcard
        // Plus baz.ds again from bar's import (but should be deduplicated)
        CHECK(mods.imported.size() >= 3);

        // Check that all three packages are present
        CHECK(mods.package_index.count("test.foo") == 1);
        CHECK(mods.package_index.count("test.bar") == 1);
        CHECK(mods.package_index.count("test.baz") == 1);
    }

    TEST_CASE("load_modules_with_imports - deduplication") {
        std::string main_path = get_test_data_path() + "test/main.ds";

        // Provide base directory as search path for resolving test.* imports
        std::vector<std::string> search_paths = { get_test_data_path() };

        module_set mods = load_modules_with_imports(main_path, search_paths);

        // test.baz should appear only once, even though it's imported by bar
        // Check that package names are unique
        std::set<std::string> package_names;
        for (const auto& mod : mods.imported) {
            if (!mod.package_name.empty()) {
                // Should be unique
                CHECK(package_names.insert(mod.package_name).second == true);
            }
        }
    }

    TEST_CASE("load_modules_with_imports - import not found") {
        // Create a temporary file with a non-existent import
        std::string temp_path = get_test_data_path() + "/temp_not_found.ds";
        {
            std::ofstream out(temp_path);
            out << "import nonexistent.module;\n";
        }

        // Should throw import_not_found_error
        CHECK_THROWS_AS(
            load_modules_with_imports(temp_path),
            import_not_found_error
        );

        // Clean up
        fs::remove(temp_path);
    }

    TEST_CASE("import_not_found_error - message formatting") {
        std::vector<std::string> searched_paths = {
            "/path/one/foo/bar.ds",
            "/path/two/foo/bar.ds",
            "/path/three/foo/bar.ds"
        };

        import_not_found_error err("foo.bar", searched_paths);

        std::string msg = err.what();

        // Check that message contains import name
        CHECK(msg.find("foo.bar") != std::string::npos);

        // Check that message contains all searched paths
        for (const auto& path : searched_paths) {
            CHECK(msg.find(path) != std::string::npos);
        }

        // Check accessor methods
        CHECK(err.import_name() == "foo.bar");
        CHECK(err.searched_paths() == searched_paths);
    }

    TEST_CASE("load_modules_with_imports - file path resolution") {
        std::string main_path = get_test_data_path() + "test/main.ds";

        // Provide base directory as search path for resolving test.* imports
        std::vector<std::string> search_paths = { get_test_data_path() };

        module_set mods = load_modules_with_imports(main_path, search_paths);

        // All file paths should be canonical (absolute)
        CHECK(fs::path(mods.main.file_path).is_absolute());

        for (const auto& mod : mods.imported) {
            CHECK(fs::path(mod.file_path).is_absolute());
        }
    }

    TEST_CASE("load_modules_with_imports - search paths") {
        std::string main_path = get_test_data_path() + "test/main.ds";

        // Test with custom search paths
        std::vector<std::string> search_paths = {
            get_test_data_path()
        };

        module_set mods = load_modules_with_imports(main_path, search_paths);

        // Should still work with explicit search paths
        CHECK(mods.imported.size() >= 1);
    }
}

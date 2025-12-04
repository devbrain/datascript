#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Package and Import") {

    // ========================================
    // Package Declarations
    // ========================================

    TEST_CASE("Simple package declaration") {
        auto mod = datascript::parse_datascript(std::string("package foo;"));

        REQUIRE(mod.package.has_value());
        REQUIRE(mod.package->name_parts.size() == 1);
        CHECK(mod.package->name_parts[0] == "foo");
    }

    TEST_CASE("Qualified package declaration") {
        auto mod = datascript::parse_datascript(std::string("package foo.bar.baz;"));

        REQUIRE(mod.package.has_value());
        REQUIRE(mod.package->name_parts.size() == 3);
        CHECK(mod.package->name_parts[0] == "foo");
        CHECK(mod.package->name_parts[1] == "bar");
        CHECK(mod.package->name_parts[2] == "baz");
    }

    TEST_CASE("Package with two parts") {
        auto mod = datascript::parse_datascript(std::string("package com.example;"));

        REQUIRE(mod.package.has_value());
        REQUIRE(mod.package->name_parts.size() == 2);
        CHECK(mod.package->name_parts[0] == "com");
        CHECK(mod.package->name_parts[1] == "example");
    }

    TEST_CASE("No package declaration") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = 0;"));

        CHECK_FALSE(mod.package.has_value());
    }

    // ========================================
    // Import Declarations
    // ========================================

    TEST_CASE("Simple import statement") {
        auto mod = datascript::parse_datascript(std::string("import foo;"));

        REQUIRE(mod.imports.size() == 1);
        REQUIRE(mod.imports[0].name_parts.size() == 1);
        CHECK(mod.imports[0].name_parts[0] == "foo");
        CHECK(mod.imports[0].is_wildcard == false);
    }

    TEST_CASE("Qualified import statement") {
        auto mod = datascript::parse_datascript(std::string("import foo.bar.baz;"));

        REQUIRE(mod.imports.size() == 1);
        REQUIRE(mod.imports[0].name_parts.size() == 3);
        CHECK(mod.imports[0].name_parts[0] == "foo");
        CHECK(mod.imports[0].name_parts[1] == "bar");
        CHECK(mod.imports[0].name_parts[2] == "baz");
        CHECK(mod.imports[0].is_wildcard == false);
    }

    TEST_CASE("Wildcard import statement") {
        auto mod = datascript::parse_datascript(std::string("import foo.bar.*;"));

        REQUIRE(mod.imports.size() == 1);
        REQUIRE(mod.imports[0].name_parts.size() == 2);
        CHECK(mod.imports[0].name_parts[0] == "foo");
        CHECK(mod.imports[0].name_parts[1] == "bar");
        CHECK(mod.imports[0].is_wildcard == true);
    }

    TEST_CASE("Multiple import statements") {
        auto mod = datascript::parse_datascript(std::string(
            "import foo;\n"
            "import bar.baz;\n"
            "import qux.*;\n"
        ));

        REQUIRE(mod.imports.size() == 3);

        // First import
        CHECK(mod.imports[0].name_parts.size() == 1);
        CHECK(mod.imports[0].name_parts[0] == "foo");
        CHECK(mod.imports[0].is_wildcard == false);

        // Second import
        CHECK(mod.imports[1].name_parts.size() == 2);
        CHECK(mod.imports[1].name_parts[0] == "bar");
        CHECK(mod.imports[1].name_parts[1] == "baz");
        CHECK(mod.imports[1].is_wildcard == false);

        // Third import
        CHECK(mod.imports[2].name_parts.size() == 1);
        CHECK(mod.imports[2].name_parts[0] == "qux");
        CHECK(mod.imports[2].is_wildcard == true);
    }

    // ========================================
    // Package and Import Together
    // ========================================

    TEST_CASE("Package and import together") {
        auto mod = datascript::parse_datascript(std::string(
            "package com.example;\n"
            "import foo.bar;\n"
            "import baz.*;\n"
        ));

        // Check package
        REQUIRE(mod.package.has_value());
        REQUIRE(mod.package->name_parts.size() == 2);
        CHECK(mod.package->name_parts[0] == "com");
        CHECK(mod.package->name_parts[1] == "example");

        // Check imports
        REQUIRE(mod.imports.size() == 2);
        CHECK(mod.imports[0].name_parts[0] == "foo");
        CHECK(mod.imports[1].is_wildcard == true);
    }

    TEST_CASE("Package, import, and constant") {
        auto mod = datascript::parse_datascript(std::string(
            "package mypackage;\n"
            "import util.helpers;\n"
            "const uint32 VERSION = 1;\n"
        ));

        // Check all components
        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts[0] == "mypackage");

        REQUIRE(mod.imports.size() == 1);
        CHECK(mod.imports[0].name_parts.size() == 2);

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "VERSION");
    }

    TEST_CASE("Complete module with package, imports, constants, and constraints") {
        auto mod = datascript::parse_datascript(std::string(
            "package com.example.proto;\n"
            "import common.types;\n"
            "import util.*;\n"
            "const uint32 MAX_SIZE = 1024;\n"
            "constraint IsPositive { value > 0 };\n"
        ));

        // Check package
        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts.size() == 3);

        // Check imports
        REQUIRE(mod.imports.size() == 2);
        CHECK(mod.imports[0].is_wildcard == false);
        CHECK(mod.imports[1].is_wildcard == true);

        // Check constants
        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "MAX_SIZE");

        // Check constraints
        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "IsPositive");
    }

    TEST_CASE("Import before package should work") {
        // Order doesn't matter in grammar, but typically package comes first
        auto mod = datascript::parse_datascript(std::string(
            "import foo;\n"
            "package bar;\n"
        ));

        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts[0] == "bar");

        REQUIRE(mod.imports.size() == 1);
        CHECK(mod.imports[0].name_parts[0] == "foo");
    }
}

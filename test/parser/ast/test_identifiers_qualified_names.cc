#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Identifiers and Qualified Names") {

    // ========================================
    // Identifier Expressions
    // ========================================

    TEST_CASE("Identifier expression in constant") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = MY_CONST;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that value is an identifier
        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constants[0].value.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "MY_CONST");
    }

    TEST_CASE("Multiple identifier expressions") {
        auto mod = datascript::parse_datascript(std::string(
            "const uint32 A = FOO;\n"
            "const uint32 B = BAR;\n"
            "const uint32 C = BAZ;\n"
        ));

        REQUIRE(mod.constants.size() == 3);

        auto* ident_a = std::get_if<datascript::ast::identifier>(&mod.constants[0].value.node);
        REQUIRE(ident_a != nullptr);
        CHECK(ident_a->name == "FOO");

        auto* ident_b = std::get_if<datascript::ast::identifier>(&mod.constants[1].value.node);
        REQUIRE(ident_b != nullptr);
        CHECK(ident_b->name == "BAR");

        auto* ident_c = std::get_if<datascript::ast::identifier>(&mod.constants[2].value.node);
        REQUIRE(ident_c != nullptr);
        CHECK(ident_c->name == "BAZ");
    }

    TEST_CASE("Identifier with underscores") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = MY_LONG_CONSTANT_NAME;"));

        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constants[0].value.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "MY_LONG_CONSTANT_NAME");
    }

    TEST_CASE("Identifier with numbers") {
        auto mod = datascript::parse_datascript(std::string("const uint32 X = VAR123;"));

        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constants[0].value.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "VAR123");
    }

    // ========================================
    // Qualified Names (User-Defined Types)
    // ========================================

    TEST_CASE("Single identifier as type") {
        auto mod = datascript::parse_datascript(std::string("const Foo X = 1;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is a qualified_name
        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 1);
        CHECK(qname->parts[0] == "Foo");
    }

    TEST_CASE("Two-part qualified name") {
        auto mod = datascript::parse_datascript(std::string("const Foo.Bar X = 2;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 2);
        CHECK(qname->parts[0] == "Foo");
        CHECK(qname->parts[1] == "Bar");
    }

    TEST_CASE("Three-part qualified name") {
        auto mod = datascript::parse_datascript(std::string("const A.B.C X = 3;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 3);
        CHECK(qname->parts[0] == "A");
        CHECK(qname->parts[1] == "B");
        CHECK(qname->parts[2] == "C");
    }

    TEST_CASE("Four-part qualified name") {
        auto mod = datascript::parse_datascript(std::string("const std.collections.List.Node Y = 4;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 4);
        CHECK(qname->parts[0] == "std");
        CHECK(qname->parts[1] == "collections");
        CHECK(qname->parts[2] == "List");
        CHECK(qname->parts[3] == "Node");
    }

    TEST_CASE("Qualified name with underscores") {
        auto mod = datascript::parse_datascript(std::string("const My_Package.My_Type X = 5;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 2);
        CHECK(qname->parts[0] == "My_Package");
        CHECK(qname->parts[1] == "My_Type");
    }

    TEST_CASE("Multiple constants with different qualified names") {
        auto mod = datascript::parse_datascript(std::string(
            "const Foo A = 1;\n"
            "const Foo.Bar B = 2;\n"
            "const X.Y.Z C = 3;\n"
        ));

        REQUIRE(mod.constants.size() == 3);

        // First constant: Foo
        auto* qname1 = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname1 != nullptr);
        REQUIRE(qname1->parts.size() == 1);
        CHECK(qname1->parts[0] == "Foo");

        // Second constant: Foo.Bar
        auto* qname2 = std::get_if<datascript::ast::qualified_name>(&mod.constants[1].ctype.node);
        REQUIRE(qname2 != nullptr);
        REQUIRE(qname2->parts.size() == 2);
        CHECK(qname2->parts[0] == "Foo");
        CHECK(qname2->parts[1] == "Bar");

        // Third constant: X.Y.Z
        auto* qname3 = std::get_if<datascript::ast::qualified_name>(&mod.constants[2].ctype.node);
        REQUIRE(qname3 != nullptr);
        REQUIRE(qname3->parts.size() == 3);
        CHECK(qname3->parts[0] == "X");
        CHECK(qname3->parts[1] == "Y");
        CHECK(qname3->parts[2] == "Z");
    }

    // ========================================
    // Edge Cases
    // ========================================

    TEST_CASE("Looks like primitive type but is user-defined") {
        // uint7 doesn't exist as primitive, treated as user type
        auto mod = datascript::parse_datascript(std::string("const uint7 X = 1;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 1);
        CHECK(qname->parts[0] == "uint7");
    }

    TEST_CASE("Missing bit width treated as user type") {
        // uint without bit width treated as user type
        auto mod = datascript::parse_datascript(std::string("const uint X = 1;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 1);
        CHECK(qname->parts[0] == "uint");
    }

    TEST_CASE("CamelCase qualified names") {
        auto mod = datascript::parse_datascript(std::string("const MyPackage.MyType.MySubType X = 1;"));

        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 3);
        CHECK(qname->parts[0] == "MyPackage");
        CHECK(qname->parts[1] == "MyType");
        CHECK(qname->parts[2] == "MySubType");
    }

    // ========================================
    // Combined Tests
    // ========================================

    TEST_CASE("Qualified type with identifier value") {
        auto mod = datascript::parse_datascript(std::string("const Foo.Bar X = MY_VALUE;"));

        // Check type is qualified name
        auto* qname = std::get_if<datascript::ast::qualified_name>(&mod.constants[0].ctype.node);
        REQUIRE(qname != nullptr);
        REQUIRE(qname->parts.size() == 2);
        CHECK(qname->parts[0] == "Foo");
        CHECK(qname->parts[1] == "Bar");

        // Check value is identifier
        auto* ident = std::get_if<datascript::ast::identifier>(&mod.constants[0].value.node);
        REQUIRE(ident != nullptr);
        CHECK(ident->name == "MY_VALUE");
    }
}

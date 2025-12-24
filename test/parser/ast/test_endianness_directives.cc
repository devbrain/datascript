#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Global Endianness Directives") {

    TEST_CASE("Default endianness is big") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint32 X = 1;
        )"));

        // Default should be big endian
        CHECK(mod.default_endianness == datascript::ast::endian::big);
    }

    TEST_CASE("Global little endianness directive") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;
            const uint32 X = 1;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
    }

    TEST_CASE("Global big endianness directive") {
        auto mod = datascript::parse_datascript(std::string(R"(
            big;
            const uint32 X = 1;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::big);
    }

    TEST_CASE("Little endianness directive before package") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;
            package com.example;
            const uint32 X = 1;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts.size() == 2);
    }

    TEST_CASE("Little endianness directive after package") {
        auto mod = datascript::parse_datascript(std::string(R"(
            package com.example;
            little;
            const uint32 X = 1;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
        REQUIRE(mod.package.has_value());
        CHECK(mod.package->name_parts.size() == 2);
    }

    TEST_CASE("Big endianness directive with imports") {
        auto mod = datascript::parse_datascript(std::string(R"(
            big;
            import foo.bar;
            import baz.*;
            const uint32 X = 1;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::big);
        CHECK(mod.imports.size() == 2);
    }

    TEST_CASE("Little endianness directive in struct file") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;

            struct Header {
                uint32 magic;
                uint16 version;
            };
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].body.size() == 2);
    }

    TEST_CASE("Big endianness directive with enums") {
        auto mod = datascript::parse_datascript(std::string(R"(
            big;

            enum uint8 Color {
                RED,
                GREEN,
                BLUE
            };
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::big);
        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].items.size() == 3);
    }

    TEST_CASE("Little endianness directive with choices") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;

            choice Message on type {
                case 1: uint8 byte;
                case 2: string text;
            };
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].cases.size() == 2);
    }

    TEST_CASE("Endianness directive does not affect explicit type modifiers") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;
            const big uint32 X = 1;
            const little uint64 Y = 2;
            const uint16 Z = 3;
        )"));

        // Global directive is little
        CHECK(mod.default_endianness == datascript::ast::endian::little);

        REQUIRE(mod.constants.size() == 3);

        // X explicitly uses big endian
        auto* x_type = std::get_if<datascript::ast::primitive_type>(&mod.constants[0].ctype.node);
        REQUIRE(x_type != nullptr);
        CHECK(x_type->byte_order == datascript::ast::endian::big);

        // Y explicitly uses little endian
        auto* y_type = std::get_if<datascript::ast::primitive_type>(&mod.constants[1].ctype.node);
        REQUIRE(y_type != nullptr);
        CHECK(y_type->byte_order == datascript::ast::endian::little);

        // Z should use unspec (no explicit modifier)
        // Note: The global directive sets the MODULE default, but individual fields
        // without modifiers still use unspec in the AST. The semantic analyzer
        // would apply the global default later.
        auto* z_type = std::get_if<datascript::ast::primitive_type>(&mod.constants[2].ctype.node);
        REQUIRE(z_type != nullptr);
        CHECK(z_type->byte_order == datascript::ast::endian::unspec);
    }

    TEST_CASE("Multiple endianness directives - last one wins") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;
            big;
            little;
            const uint32 X = 1;
        )"));

        // Last directive should win
        CHECK(mod.default_endianness == datascript::ast::endian::little);
    }

    TEST_CASE("Empty module with little directive") {
        auto mod = datascript::parse_datascript(std::string(R"(
            little;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::little);
        CHECK(mod.constants.empty());
        CHECK(mod.structs.empty());
    }

    TEST_CASE("Empty module with big directive") {
        auto mod = datascript::parse_datascript(std::string(R"(
            big;
        )"));

        CHECK(mod.default_endianness == datascript::ast::endian::big);
        CHECK(mod.constants.empty());
        CHECK(mod.structs.empty());
    }
}

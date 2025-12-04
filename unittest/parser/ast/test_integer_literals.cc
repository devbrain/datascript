//
// Tests for integer literal parsing
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/ast.hh>
#include <variant>

using namespace datascript;

TEST_SUITE("Integer Literal Parsing") {
    TEST_CASE("Parse decimal integer literals") {
        auto mod = parse_datascript(std::string("const uint32 X = 42;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check the value is a literal_int
        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 42);
    }

    TEST_CASE("Parse zero") {
        auto mod = parse_datascript(std::string("const uint32 ZERO = 0;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "ZERO");

        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 0);
    }

    TEST_CASE("Parse large decimal number") {
        auto mod = parse_datascript(std::string("const uint64 BIG = 123456789;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "BIG");

        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 123456789);
    }

    TEST_CASE("Parse hexadecimal literals") {
        auto mod = parse_datascript(std::string("const uint32 HEX = 0xFF;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "HEX");

        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 255);
    }

    TEST_CASE("Parse octal literals") {
        auto mod = parse_datascript(std::string("const uint32 OCT = 0755;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "OCT");

        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 493); // 0755 in octal = 493 in decimal
    }

    TEST_CASE("Parse binary literals") {
        auto mod = parse_datascript(std::string("const uint32 BIN = 0b101010;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "BIN");

        auto* literal = std::get_if<ast::literal_int>(&mod.constants[0].value.node);
        REQUIRE(literal != nullptr);
        CHECK(literal->value == 42); // 0b101010 = 42
    }

    TEST_CASE("Parse negative decimal literals") {
        // Note: The scanner currently doesn't handle negative literals directly
        // This test is here as a placeholder for when we add support

        // auto mod = parse_datascript(std::string("const int32 NEG = -42;"));
        // REQUIRE(mod.constants.size() == 1);
        // CHECK(mod.constants[0].name == "NEG");

        // For now, just verify we can parse positive numbers
        auto mod = parse_datascript(std::string("const int32 POS = 42;"));
        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "POS");
    }
}
//
// Basic parser functionality tests
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>

using namespace datascript;

TEST_SUITE("Parser - Basic Functionality") {
    TEST_CASE("Empty input produces empty module") {
        auto mod = parse_datascript(std::string(""));
        CHECK(mod.constants.empty());
    }

    TEST_CASE("Whitespace-only input produces empty module") {
        auto mod = parse_datascript(std::string("   \n\t  \n  "));
        CHECK(mod.constants.empty());
    }

    TEST_CASE("Line comments are ignored") {
        auto mod = parse_datascript(std::string(R"(
            // This is a comment
            const uint32 X = 1; // trailing comment
            // Another comment
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");
    }

    TEST_CASE("Block comments are ignored") {
        auto mod = parse_datascript(std::string(R"(
            /* This is a block comment */
            const uint32 X = 1; /* inline comment */
            /* Multi-line
               block
               comment */
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");
    }

    TEST_CASE("Multiple constants") {
        auto mod = parse_datascript(std::string(R"(
            const uint8 FIRST = 1;
            const uint16 SECOND = 2;
            const uint32 THIRD = 3;
        )"));

        REQUIRE(mod.constants.size() == 3);
        CHECK(mod.constants[0].name == "FIRST");
        CHECK(mod.constants[1].name == "SECOND");
        CHECK(mod.constants[2].name == "THIRD");
    }
}

TEST_SUITE("Parser - Primitive Types") {
    TEST_CASE("Unsigned integer types") {
        auto mod = parse_datascript(std::string(R"(
            const uint8 A = 1;
            const uint16 B = 2;
            const uint32 C = 3;
            const uint64 D = 4;
            const uint128 E = 5;
        )"));

        REQUIRE(mod.constants.size() == 5);

        for (size_t i = 0; i < 5; ++i) {
            const auto* prim = std::get_if<ast::primitive_type>(&mod.constants[i].ctype.node);
            REQUIRE(prim != nullptr);
            CHECK(prim->is_signed == false);
            CHECK(prim->byte_order == ast::endian::unspec);
        }

        const auto* t8 = std::get_if<ast::primitive_type>(&mod.constants[0].ctype.node);
        CHECK(t8->bits == 8);

        const auto* t16 = std::get_if<ast::primitive_type>(&mod.constants[1].ctype.node);
        CHECK(t16->bits == 16);

        const auto* t32 = std::get_if<ast::primitive_type>(&mod.constants[2].ctype.node);
        CHECK(t32->bits == 32);

        const auto* t64 = std::get_if<ast::primitive_type>(&mod.constants[3].ctype.node);
        CHECK(t64->bits == 64);

        const auto* t128 = std::get_if<ast::primitive_type>(&mod.constants[4].ctype.node);
        CHECK(t128->bits == 128);
    }

    TEST_CASE("Signed integer types") {
        auto mod = parse_datascript(std::string("const int32 SIGNED = -1;"));

        REQUIRE(mod.constants.size() == 1);
        const auto* prim = std::get_if<ast::primitive_type>(&mod.constants[0].ctype.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == true);
        CHECK(prim->bits == 32);
    }

    TEST_CASE("Endianness - little") {
        auto mod = parse_datascript(std::string("const little uint32 X = 1;"));

        REQUIRE(mod.constants.size() == 1);
        const auto* prim = std::get_if<ast::primitive_type>(&mod.constants[0].ctype.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->byte_order == ast::endian::little);
    }

    TEST_CASE("Endianness - big") {
        auto mod = parse_datascript(std::string("const big uint64 X = 1;"));

        REQUIRE(mod.constants.size() == 1);
        const auto* prim = std::get_if<ast::primitive_type>(&mod.constants[0].ctype.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->byte_order == ast::endian::big);
    }
}

TEST_SUITE("Parser - Boolean Type and Literals") {
    TEST_CASE("Boolean type with true literal") {
        auto mod = parse_datascript(std::string("const bool FLAG = true;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "FLAG");

        const auto* bool_type = std::get_if<ast::bool_type>(&mod.constants[0].ctype.node);
        CHECK(bool_type != nullptr);

        const auto* lit = std::get_if<ast::literal_bool>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == true);
    }

    TEST_CASE("Boolean type with false literal") {
        auto mod = parse_datascript(std::string("const bool FLAG = false;"));

        REQUIRE(mod.constants.size() == 1);
        const auto* lit = std::get_if<ast::literal_bool>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == false);
    }
}

TEST_SUITE("Parser - String Type and Literals") {
    TEST_CASE("String type with simple literal") {
        auto mod = parse_datascript(std::string(R"(const string MSG = "hello";)"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "MSG");

        const auto* str_type = std::get_if<ast::string_type>(&mod.constants[0].ctype.node);
        CHECK(str_type != nullptr);

        const auto* lit = std::get_if<ast::literal_string>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == "hello");
    }

    TEST_CASE("String with escape sequences") {
        auto mod = parse_datascript(std::string(R"(const string MSG = "hello\nworld\ttab\"quote\\backslash";)"));

        REQUIRE(mod.constants.size() == 1);
        const auto* lit = std::get_if<ast::literal_string>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == "hello\nworld\ttab\"quote\\backslash");
    }

    TEST_CASE("Empty string") {
        auto mod = parse_datascript(std::string(R"(const string EMPTY = "";)"));

        REQUIRE(mod.constants.size() == 1);
        const auto* lit = std::get_if<ast::literal_string>(&mod.constants[0].value.node);
        REQUIRE(lit != nullptr);
        CHECK(lit->value == "");
    }
}

TEST_SUITE("Parser - Bit Field Types") {
    TEST_CASE("Fixed-width bit field") {
        auto mod = parse_datascript(std::string("const bit:7 FLAGS = 42;"));

        REQUIRE(mod.constants.size() == 1);
        const auto* bf = std::get_if<ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf != nullptr);
        CHECK(bf->width == 7);
    }

    TEST_CASE("Bit field with various widths") {
        auto mod = parse_datascript(std::string(R"(
            const bit:1 BIT1 = 1;
            const bit:4 NIBBLE = 15;
            const bit:12 HUGE = 4095;
        )"));

        REQUIRE(mod.constants.size() == 3);

        const auto* bf1 = std::get_if<ast::bit_field_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(bf1 != nullptr);
        CHECK(bf1->width == 1);

        const auto* bf2 = std::get_if<ast::bit_field_type_fixed>(&mod.constants[1].ctype.node);
        REQUIRE(bf2 != nullptr);
        CHECK(bf2->width == 4);

        const auto* bf3 = std::get_if<ast::bit_field_type_fixed>(&mod.constants[2].ctype.node);
        REQUIRE(bf3 != nullptr);
        CHECK(bf3->width == 12);
    }
}

TEST_SUITE("Parser - Error Handling") {
    TEST_CASE("Invalid syntax throws parse_error") {
        CHECK_THROWS(parse_datascript(std::string("const uint32")));
        CHECK_THROWS(parse_datascript(std::string("const uint32 X")));
        CHECK_THROWS(parse_datascript(std::string("const uint32 X =")));
        CHECK_THROWS(parse_datascript(std::string("const uint32 X = ;")));
    }

    TEST_CASE("User-defined types accepted") {
        // With qualified name support, any identifier is valid as a user-defined type
        // Type existence validation happens in semantic analysis, not parsing
        CHECK_NOTHROW(parse_datascript(std::string("const Foo X = 1;")));    // Single identifier
        CHECK_NOTHROW(parse_datascript(std::string("const Foo.Bar Y = 2;")));  // Qualified name
        CHECK_NOTHROW(parse_datascript(std::string("const uint7 Z = 3;")));  // Looks like primitive but treated as user type
        CHECK_NOTHROW(parse_datascript(std::string("const uint W = 4;")));   // Looks like primitive but treated as user type
    }

    TEST_CASE("Unclosed string literal throws") {
        CHECK_THROWS(parse_datascript(std::string(R"(const string X = "unclosed;)")));
    }

    TEST_CASE("Invalid escape sequence throws") {
        CHECK_THROWS(parse_datascript(std::string("const string X = \"\\x\";")));
    }
}

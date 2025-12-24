//
// Docstring parsing tests
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>

using namespace datascript;

TEST_SUITE("Parser - Docstrings") {
    TEST_CASE("Constant with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** This is a constant documentation */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");
        REQUIRE(mod.constants[0].docstring.has_value());
        CHECK(mod.constants[0].docstring.value() == "This is a constant documentation");
    }

    TEST_CASE("Constant without docstring") {
        auto mod = parse_datascript(std::string(R"(
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");
        CHECK_FALSE(mod.constants[0].docstring.has_value());
    }

    TEST_CASE("Docstring with multiple lines") {
        auto mod = parse_datascript(std::string(R"(
            /**
             * This is a multi-line
             * docstring with several
             * lines of documentation
             */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        REQUIRE(mod.constants[0].docstring.has_value());

        std::string expected = "This is a multi-line\ndocstring with several\nlines of documentation";
        CHECK(mod.constants[0].docstring.value() == expected);
    }

    TEST_CASE("Docstring with leading asterisks stripped") {
        auto mod = parse_datascript(std::string(R"(
            /**
             * First line
             * Second line
             * Third line
             */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        REQUIRE(mod.constants[0].docstring.has_value());

        std::string expected = "First line\nSecond line\nThird line";
        CHECK(mod.constants[0].docstring.value() == expected);
    }

    TEST_CASE("Struct with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** A simple structure */
            struct Point {
                uint32 x;
                uint32 y;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "Point");
        REQUIRE(mod.structs[0].docstring.has_value());
        CHECK(mod.structs[0].docstring.value() == "A simple structure");
    }

    TEST_CASE("Struct field with docstring") {
        auto mod = parse_datascript(std::string(R"(
            struct Point {
                /** X coordinate */
                uint32 x;
                /** Y coordinate */
                uint32 y;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        auto* field0 = std::get_if<ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        REQUIRE(field0->docstring.has_value());
        CHECK(field0->docstring.value() == "X coordinate");

        auto* field1 = std::get_if<ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        REQUIRE(field1->docstring.has_value());
        CHECK(field1->docstring.value() == "Y coordinate");
    }

    TEST_CASE("Union with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** A tagged union */
            union Value {
                uint32 int_val;
                string str_val;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        CHECK(mod.unions[0].name == "Value");
        REQUIRE(mod.unions[0].docstring.has_value());
        CHECK(mod.unions[0].docstring.value() == "A tagged union");
    }

    TEST_CASE("Enum with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** Status codes */
            enum uint8 Status {
                /** Operation succeeded */
                OK = 0,
                /** Operation failed */
                ERROR = 1
            };
        )"));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Status");
        REQUIRE(mod.enums[0].docstring.has_value());
        CHECK(mod.enums[0].docstring.value() == "Status codes");

        REQUIRE(mod.enums[0].items.size() == 2);
        REQUIRE(mod.enums[0].items[0].docstring.has_value());
        CHECK(mod.enums[0].items[0].docstring.value() == "Operation succeeded");

        REQUIRE(mod.enums[0].items[1].docstring.has_value());
        CHECK(mod.enums[0].items[1].docstring.value() == "Operation failed");
    }

    TEST_CASE("Choice with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** Type-based choice */
            choice MyChoice on type_field {
                case 1: uint32 int_field;
                case 2: string str_field;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "MyChoice");
        REQUIRE(mod.choices[0].docstring.has_value());
        CHECK(mod.choices[0].docstring.value() == "Type-based choice");
    }

    TEST_CASE("Constraint with docstring") {
        auto mod = parse_datascript(std::string(R"(
            /** Validates value range */
            constraint InRange(uint32 value) {
                value >= 0 && value <= 100
            };
        )"));

        REQUIRE(mod.constraints.size() == 1);
        CHECK(mod.constraints[0].name == "InRange");
        REQUIRE(mod.constraints[0].docstring.has_value());
        CHECK(mod.constraints[0].docstring.value() == "Validates value range");
    }

    TEST_CASE("Regular comment does not become docstring") {
        auto mod = parse_datascript(std::string(R"(
            /* This is a regular comment, not a docstring */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK_FALSE(mod.constants[0].docstring.has_value());
    }

    TEST_CASE("Multiple docstrings on different items") {
        auto mod = parse_datascript(std::string(R"(
            /** First constant */
            const uint32 FIRST = 1;

            /** Second constant */
            const uint32 SECOND = 2;

            /** Third constant */
            const uint32 THIRD = 3;
        )"));

        REQUIRE(mod.constants.size() == 3);

        REQUIRE(mod.constants[0].docstring.has_value());
        CHECK(mod.constants[0].docstring.value() == "First constant");

        REQUIRE(mod.constants[1].docstring.has_value());
        CHECK(mod.constants[1].docstring.value() == "Second constant");

        REQUIRE(mod.constants[2].docstring.has_value());
        CHECK(mod.constants[2].docstring.value() == "Third constant");
    }

    TEST_CASE("Docstring with only whitespace") {
        auto mod = parse_datascript(std::string(R"(
            /**
             *
             *
             */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        // Whitespace-only docstrings should result in no value
        CHECK_FALSE(mod.constants[0].docstring.has_value());
    }

    TEST_CASE("Docstring preserves internal formatting") {
        auto mod = parse_datascript(std::string(R"(
            /**
             * Example:
             *   - Item 1
             *   - Item 2
             *   - Item 3
             */
            const uint32 X = 42;
        )"));

        REQUIRE(mod.constants.size() == 1);
        REQUIRE(mod.constants[0].docstring.has_value());

        std::string expected = "Example:\n  - Item 1\n  - Item 2\n  - Item 3";
        CHECK(mod.constants[0].docstring.value() == expected);
    }

    TEST_CASE("Mixed items with and without docstrings") {
        auto mod = parse_datascript(std::string(R"(
            /** Documented constant */
            const uint32 WITH_DOC = 1;

            const uint32 WITHOUT_DOC = 2;

            /** Another documented constant */
            const uint32 WITH_DOC_2 = 3;
        )"));

        REQUIRE(mod.constants.size() == 3);

        REQUIRE(mod.constants[0].docstring.has_value());
        CHECK(mod.constants[0].docstring.value() == "Documented constant");

        CHECK_FALSE(mod.constants[1].docstring.has_value());

        REQUIRE(mod.constants[2].docstring.has_value());
        CHECK(mod.constants[2].docstring.value() == "Another documented constant");
    }
}

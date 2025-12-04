//
// Test cases for function definitions in structs
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/ast.hh>

using namespace datascript;
using namespace datascript::ast;

TEST_SUITE("Parser - Function Definitions") {
    TEST_CASE("Simple function with no parameters") {
        auto mod = parse_datascript(std::string(R"(
            struct Header {
                uint8 flags;

                function bool is_compressed() {
                    return (flags & 1) != 0;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        CHECK( s.name == "Header" );
        REQUIRE( s.body.size() == 2 ); // field + function

        // Check field
        REQUIRE( std::holds_alternative<field_def>(s.body[0]) );

        // Check function
        REQUIRE( std::holds_alternative<function_def>(s.body[1]) );
        const auto& func = std::get<function_def>(s.body[1]);
        CHECK( func.name == "is_compressed" );
        CHECK( func.parameters.empty() );
        REQUIRE( func.body.size() == 1 ); // return statement
        CHECK( std::holds_alternative<return_statement>(func.body[0]) );
    }

    TEST_CASE("Function with parameters") {
        auto mod = parse_datascript(std::string(R"(
            struct BitField {
                uint32 value;

                function bool get_bit(uint8 index) {
                    return (value >> index) & 1;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        REQUIRE( s.body.size() == 2 ); // field + function

        const auto& func = std::get<function_def>(s.body[1]);
        CHECK( func.name == "get_bit" );
        REQUIRE( func.parameters.size() == 1 );
        CHECK( func.parameters[0].name == "index" );
    }

    TEST_CASE("Function with multiple parameters") {
        auto mod = parse_datascript(std::string(R"(
            struct Data {
                uint8 flags;

                function uint32 compute(uint8 a, uint16 b) {
                    return a + b;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        const auto& func = std::get<function_def>(s.body[1]);

        REQUIRE( func.parameters.size() == 2 );
        CHECK( func.parameters[0].name == "a" );
        CHECK( func.parameters[1].name == "b" );
    }

    TEST_CASE("Function with docstring") {
        auto mod = parse_datascript(std::string(R"(
            struct Config {
                uint8 flags;

                /** Check if feature is enabled */
                function bool is_enabled() {
                    return flags != 0;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        const auto& func = std::get<function_def>(s.body[1]);

        REQUIRE( func.docstring.has_value() );
        CHECK( func.docstring->find("Check if feature is enabled") != std::string::npos );
    }

    TEST_CASE("Multiple functions in struct") {
        auto mod = parse_datascript(std::string(R"(
            struct Header {
                uint8 flags;

                function bool is_compressed() {
                    return (flags & 1) != 0;
                }

                function bool is_encrypted() {
                    return (flags & 2) != 0;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        REQUIRE( s.body.size() == 3 ); // 1 field + 2 functions

        CHECK( std::holds_alternative<field_def>(s.body[0]) );
        CHECK( std::holds_alternative<function_def>(s.body[1]) );
        CHECK( std::holds_alternative<function_def>(s.body[2]) );

        const auto& func1 = std::get<function_def>(s.body[1]);
        const auto& func2 = std::get<function_def>(s.body[2]);
        CHECK( func1.name == "is_compressed" );
        CHECK( func2.name == "is_encrypted" );
    }

    TEST_CASE("Function with expression statement") {
        auto mod = parse_datascript(std::string(R"(
            struct Test {
                uint8 value;

                function uint8 get_value() {
                    value + 1;
                    return value;
                }
            };
        )"));

        REQUIRE( mod.structs.size() == 1 );
        const auto& s = mod.structs[0];
        const auto& func = std::get<function_def>(s.body[1]);

        REQUIRE( func.body.size() == 2 );
        CHECK( std::holds_alternative<expression_statement>(func.body[0]) );
        CHECK( std::holds_alternative<return_statement>(func.body[1]) );
    }
}

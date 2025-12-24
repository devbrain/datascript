#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Docstring Edge Cases") {

    TEST_CASE("File with only docstring (no code)") {
        const char* input = R"(
            /** This is a standalone docstring with no following code */
        )";

        // EXPECTED: Parse error - docstrings must precede a construct
        // A standalone docstring with nothing following causes a parse failure
        CHECK_THROWS_AS(datascript::parse_datascript(std::string(input)),
                       datascript::parse_error);
    }

    TEST_CASE("File ending with docstring (no trailing code)") {
        const char* input = R"(
            struct Point {
                uint32 x;
            };

            /** Docstring at end with nothing following */
        )";

        // EXPECTED: Parse error - docstring at EOF with no construct following
        CHECK_THROWS_AS(datascript::parse_datascript(std::string(input)),
                       datascript::parse_error);
    }

    TEST_CASE("Multiple consecutive docstrings") {
        const char* input = R"(
            /** First docstring */
            /** Second docstring */
            /** Third docstring - this one should be attached */
            struct Point {
                uint32 x;
            };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        // Only the last docstring before the struct should be attached
        REQUIRE(mod.structs[0].docstring.has_value());
        CHECK(mod.structs[0].docstring.value().find("Third docstring") != std::string::npos);
        CHECK(mod.structs[0].docstring.value().find("First docstring") == std::string::npos);
    }

    TEST_CASE("Docstring with no space after asterisks") {
        const char* input = R"(
            /**No space after opening
             *No space on these lines either
             *Still no space
             */
            struct Point { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        // Should still extract text even without spaces
        CHECK(mod.structs[0].docstring.value().find("No space after opening") != std::string::npos);
    }

    TEST_CASE("Docstring between unrelated items") {
        const char* input = R"(
            struct A { uint32 x; };
            
            /** This docstring is orphaned between A and B */
            
            struct B { uint32 y; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 2);
        CHECK(mod.structs[0].name == "A");
        CHECK(mod.structs[1].name == "B");

        // First struct has no docstring
        CHECK_FALSE(mod.structs[0].docstring.has_value());

        // ACTUAL BEHAVIOR: The docstring DOES attach to struct B
        // This is expected - docstrings are "forward-looking", they describe what follows
        REQUIRE(mod.structs[1].docstring.has_value());
        CHECK(mod.structs[1].docstring.value().find("orphaned between A and B") != std::string::npos);
    }

    TEST_CASE("Docstring with empty lines") {
        const char* input = R"(
            /**
             * First paragraph.
             *
             * Second paragraph after empty line.
             *
             *
             * Third paragraph after multiple empty lines.
             */
            struct Point { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        
        auto& doc = mod.structs[0].docstring.value();
        // Empty lines should be preserved
        CHECK(doc.find("First paragraph") != std::string::npos);
        CHECK(doc.find("Second paragraph") != std::string::npos);
        CHECK(doc.find("Third paragraph") != std::string::npos);
    }

    TEST_CASE("Docstring with special characters") {
        const char* input = R"(
            /**
             * Special chars: @param <T> & | ^ $ # % ! ? ~ ` [ ] { }
             * Quotes: "double" 'single'
             * Backslash: \ and \n \t \r
             * Unicode: café, 日本語
             */
            struct Point { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        
        auto& doc = mod.structs[0].docstring.value();
        CHECK(doc.find("Special chars") != std::string::npos);
        CHECK(doc.find("@param") != std::string::npos);
        CHECK(doc.find("Backslash") != std::string::npos);
    }

    TEST_CASE("Unclosed docstring (syntax error)") {
        const char* input = R"(
            /** This docstring is never closed
            struct Point { uint32 x; };
        )";
        
        // Should throw parse error for unclosed comment
        CHECK_THROWS_AS(datascript::parse_datascript(std::string(input)), 
                       datascript::parse_error);
    }

    TEST_CASE("Docstring with only whitespace") {
        const char* input = R"(
            /**
             *
             *
             *
             */
            struct Point { uint32 x; };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);

        // ACTUAL BEHAVIOR: Whitespace-only docstrings are trimmed to empty
        // The docstring processing strips leading asterisks and whitespace
        // This results in an empty string which is NOT stored
        CHECK_FALSE(mod.structs[0].docstring.has_value());
    }

    TEST_CASE("Docstring immediately before EOF") {
        const char* input = R"(struct Point { uint32 x; };
/** End of file docstring */)";

        // EXPECTED: Parse error - docstring at EOF with no construct following
        CHECK_THROWS_AS(datascript::parse_datascript(std::string(input)),
                       datascript::parse_error);
    }

    TEST_CASE("Docstring in struct body (field docstring)") {
        const char* input = R"(
            struct Point {
                /** X coordinate */
                uint32 x;
                
                /** Y coordinate */
                uint32 y;
            };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);
        
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        
        REQUIRE(field1 != nullptr);
        REQUIRE(field2 != nullptr);
        
        // Field docstrings should be attached
        REQUIRE(field1->docstring.has_value());
        CHECK(field1->docstring.value().find("X coordinate") != std::string::npos);
        
        REQUIRE(field2->docstring.has_value());
        CHECK(field2->docstring.value().find("Y coordinate") != std::string::npos);
    }

    TEST_CASE("Regular comment vs docstring") {
        const char* input = R"(
            /* This is a regular comment, not a docstring */
            /** This IS a docstring */
            struct Point { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        
        // Only the docstring should be captured
        CHECK(mod.structs[0].docstring.value().find("This IS a docstring") != std::string::npos);
        CHECK(mod.structs[0].docstring.value().find("regular comment") == std::string::npos);
    }

    TEST_CASE("Docstring with nested comment-like text") {
        const char* input = R"(
            /**
             * This describes comment syntax.
             * Line comments // like this are fine.
             * But nested comment markers must be avoided.
             */
            struct Point { uint32 x; };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());

        // Text should be preserved
        auto& doc = mod.structs[0].docstring.value();
        CHECK(doc.find("comment syntax") != std::string::npos);
        CHECK(doc.find("Line comments") != std::string::npos);

        // NOTE: Nested /* */ inside docstrings is NOT supported
        // This is a known limitation of the lexer - use \/ * to document it
    }

    TEST_CASE("Docstring on choice with inline struct") {
        const char* input = R"(
            /** Choice with inline struct documentation */
            choice ResourceId : uint16 {
                case 0xFFFF: {
                    uint16 marker;
                    uint16 ordinal;
                } data;
                default:
                    string name;
            };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.choices.size() == 1);
        REQUIRE(mod.choices[0].docstring.has_value());
        CHECK(mod.choices[0].docstring.value().find("inline struct") != std::string::npos);
    }

    TEST_CASE("Very long docstring (stress test)") {
        std::string input = "/**\n";
        for (int i = 0; i < 1000; i++) {
            input += " * Line " + std::to_string(i) + " of a very long docstring.\n";
        }
        input += " */\nstruct Point { uint32 x; };";
        
        auto mod = datascript::parse_datascript(input);
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        
        // Spot check some lines
        auto& doc = mod.structs[0].docstring.value();
        CHECK(doc.find("Line 0") != std::string::npos);
        CHECK(doc.find("Line 500") != std::string::npos);
        CHECK(doc.find("Line 999") != std::string::npos);
    }

    TEST_CASE("Docstring with code examples") {
        const char* input = R"(
            /**
             * Example usage:
             * ```
             * Point p;
             * p.x = 10;
             * ```
             */
            struct Point { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].docstring.has_value());
        CHECK(mod.structs[0].docstring.value().find("Example usage") != std::string::npos);
        CHECK(mod.structs[0].docstring.value().find("```") != std::string::npos);
    }

    TEST_CASE("Single-line docstring variations") {
        const char* input = R"(
            /** Single line no newline */struct A { uint32 x; };
            
            /**Another single line*/struct B { uint32 x; };
            
            /**   Padded   */struct C { uint32 x; };
        )";
        
        auto mod = datascript::parse_datascript(std::string(input));
        
        REQUIRE(mod.structs.size() == 3);
        
        // All should have docstrings
        REQUIRE(mod.structs[0].docstring.has_value());
        REQUIRE(mod.structs[1].docstring.has_value());
        REQUIRE(mod.structs[2].docstring.has_value());
        
        CHECK(mod.structs[0].docstring.value().find("Single line no newline") != std::string::npos);
        CHECK(mod.structs[1].docstring.value().find("Another single line") != std::string::npos);
        CHECK(mod.structs[2].docstring.value().find("Padded") != std::string::npos);
    }

}

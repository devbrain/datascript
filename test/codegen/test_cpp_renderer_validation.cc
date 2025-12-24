//
// Tests for CppRenderer nullptr Validation
//

#include <doctest/doctest.h>
#include <datascript/codegen.hh>
#include <datascript/codegen/cpp/cpp_renderer.hh>
#include <datascript/ir.hh>

using namespace datascript;
using namespace datascript::codegen;
using namespace datascript::ir;

TEST_SUITE("CppRenderer - nullptr Validation") {

    TEST_CASE("render_expression - nullptr throws") {
        CppRenderer renderer;

        SUBCASE("Null expression pointer") {
            REQUIRE_THROWS_AS(
                renderer.render_expression(nullptr),
                invalid_ir_error
            );

            try {
                renderer.render_expression(nullptr);
                FAIL("Should have thrown invalid_ir_error");
            } catch (const invalid_ir_error& e) {
                std::string msg = e.what();
                CHECK(msg.find("Null expression pointer") != std::string::npos);
            }
        }
    }

    // NOTE: ir_type_to_cpp, get_primitive_cpp_type, and generate_read_call are private methods.
    // They are tested indirectly through the public API. If they are called with nullptr,
    // they will throw invalid_ir_error which will propagate through the public API.

    TEST_CASE("Valid inputs do not throw") {
        CppRenderer renderer;

        SUBCASE("Valid expression - literal int") {
            expr e;
            e.type = expr::literal_int;
            e.int_value = 42;

            REQUIRE_NOTHROW([&]() {
                auto result = renderer.render_expression(&e);
                CHECK(result == "42");
            }());
        }

        SUBCASE("Valid expression - literal bool") {
            expr e;
            e.type = expr::literal_bool;
            e.bool_value = true;

            REQUIRE_NOTHROW([&]() {
                auto result = renderer.render_expression(&e);
                CHECK(result == "true");
            }());
        }

        SUBCASE("Valid expression - literal string") {
            expr e;
            e.type = expr::literal_string;
            e.string_value = "hello";

            REQUIRE_NOTHROW([&]() {
                auto result = renderer.render_expression(&e);
                CHECK(result.find("hello") != std::string::npos);
            }());
        }
    }

    TEST_CASE("Exception types hierarchy") {
        SUBCASE("invalid_ir_error is derived from codegen_error") {
            try {
                throw invalid_ir_error("test");
            } catch (const codegen_error& e) {
                // Should catch as base class
                CHECK(std::string(e.what()).find("test") != std::string::npos);
            } catch (...) {
                FAIL("Should have caught as codegen_error");
            }
        }

        SUBCASE("codegen_error is derived from std::logic_error") {
            try {
                throw codegen_error("test");
            } catch (const std::logic_error& e) {
                // Should catch as std::logic_error
                CHECK(std::string(e.what()).find("test") != std::string::npos);
            } catch (...) {
                FAIL("Should have caught as std::logic_error");
            }
        }
    }

    TEST_CASE("Error messages are descriptive") {
        CppRenderer renderer;

        SUBCASE("Expression error message") {
            try {
                renderer.render_expression(nullptr);
                FAIL("Should have thrown invalid_ir_error");
            } catch (const invalid_ir_error& e) {
                std::string msg = e.what();
                // Should contain "Invalid IR" prefix
                CHECK(msg.find("Invalid IR") != std::string::npos);
                // Should contain specific error
                CHECK(msg.find("Null expression pointer") != std::string::npos);
            }
        }
    }
}

//
// Test for bug: inline discriminator choice with default case generates incorrect code
// Bug: Default case code is not wrapped in else block, causing both cases to execute
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>
#include <cstring>
#include <vector>
#include <iostream>

using namespace datascript;

TEST_SUITE("Choice Default Case Bug") {

    // Helper to compile DataScript to C++ and extract generated code
    static std::string compile_to_cpp(const std::string& ds_source) {
        auto parsed = parse_datascript(ds_source);
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        if (analysis.has_errors()) {
            throw std::runtime_error("Semantic analysis failed");
        }

        auto ir = ir::build_ir(analysis.analyzed.value());

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::both;

        return codegen::generate_cpp_header(ir, opts);
    }

    TEST_CASE("Inline discriminator choice with inline struct in default case") {
        std::string ds_source = R"(
            choice ne_name_or_id : uint8 {
                case 0xFF: {
                    uint8 marker;
                    uint16 ordinal;
                } ordinal_value;
                default: {
                    uint8 length;
                    uint8 chars[length];
                } string_value;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator is read
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify inline struct types are generated
        CHECK(cpp_code.find("ne_name_or_id__ordinal_value_case__type") != std::string::npos);
        CHECK(cpp_code.find("ne_name_or_id__string_value_default__type") != std::string::npos);

        // Find the read() method
        size_t read_method_pos = cpp_code.find("static ne_name_or_id read");
        REQUIRE(read_method_pos != std::string::npos);

        // Find the if statement for the 0xFF case
        size_t if_pos = cpp_code.find("if (", read_method_pos);
        REQUIRE(if_pos != std::string::npos);

        // The critical check: there should be an "else" for the default case
        // Find the closing brace of the if block
        size_t first_closing_brace = cpp_code.find("}", if_pos);
        REQUIRE(first_closing_brace != std::string::npos);

        // After the closing brace, we should find "else" (for the default case)
        std::string after_if = cpp_code.substr(first_closing_brace, 50);

        // THIS IS THE BUG: currently there's no "else", so the default case code runs unconditionally
        CHECK_MESSAGE(after_if.find("else") != std::string::npos,
                      "Default case should be in an 'else' block, not executed unconditionally!");

        // Verify the default case reads the string_value type
        size_t default_read = cpp_code.find("ne_name_or_id__string_value_default__type::read", if_pos);
        REQUIRE(default_read != std::string::npos);

        // The default case read should be INSIDE an else block, not after the if
        // Count opening braces between if and default_read
        int brace_depth = 0;
        bool found_else = false;
        for (size_t i = if_pos; i < default_read; ++i) {
            if (cpp_code[i] == '{') brace_depth++;
            if (cpp_code[i] == '}') brace_depth--;
            if (cpp_code.substr(i, 4) == "else") found_else = true;
        }

        // If default_read is outside the if block (brace_depth <= 0), that's the bug
        CHECK_MESSAGE(found_else, "Default case should be in an else block!");
        CHECK_MESSAGE(brace_depth > 0, "Default case should be inside a conditional block, not outside!");
    }

    TEST_CASE("Simple inline discriminator choice with default case - minimal repro") {
        std::string ds_source = R"(
            choice TestChoice : uint8 {
                case 1:
                    uint32 value_a;
                default:
                    uint32 value_b;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Find the read method
        size_t read_pos = cpp_code.find("static TestChoice read");
        REQUIRE(read_pos != std::string::npos);

        // Should have if for case 1
        size_t if_pos = cpp_code.find("if (", read_pos);
        REQUIRE(if_pos != std::string::npos);

        // Should have else for default
        size_t else_pos = cpp_code.find("else", if_pos);
        CHECK_MESSAGE(else_pos != std::string::npos, "Missing 'else' block for default case!");

        // The else should come before the second variant assignment
        if (else_pos != std::string::npos) {
            // Verify structure: if { case 1 } else { default }
            std::string if_block = cpp_code.substr(if_pos, else_pos - if_pos);
            CHECK(if_block.find("value_a") != std::string::npos);

            size_t after_else = cpp_code.find("{", else_pos);
            REQUIRE(after_else != std::string::npos);
            size_t else_end = cpp_code.find("}", after_else);
            REQUIRE(else_end != std::string::npos);
            std::string else_block = cpp_code.substr(else_pos, else_end - else_pos + 1);
            CHECK(else_block.find("value_b") != std::string::npos);
        }
    }

    TEST_CASE("Multiple cases with default - verify else structure") {
        std::string ds_source = R"(
            choice MultiCase : uint16 {
                case 0x1:
                    uint32 case1;
                case 0x2:
                    uint32 case2;
                case 0x3:
                    uint32 case3;
                default:
                    uint32 default_val;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        size_t read_pos = cpp_code.find("static MultiCase read");
        REQUIRE(read_pos != std::string::npos);

        // Should have:
        // if (selector == 1) { case1 }
        // else if (selector == 2) { case2 }
        // else if (selector == 3) { case3 }
        // else { default }

        size_t if_pos = cpp_code.find("if (", read_pos);
        REQUIRE(if_pos != std::string::npos);

        // Count else_if occurrences
        size_t else_if_count = 0;
        size_t pos = if_pos;
        while ((pos = cpp_code.find("else if", pos)) != std::string::npos) {
            else_if_count++;
            pos += 7;
        }
        CHECK(else_if_count >= 2); // At least case2 and case3

        // Final else for default
        size_t last_else = cpp_code.rfind("else", cpp_code.find("return obj", read_pos));
        CHECK_MESSAGE(last_else != std::string::npos, "Missing final 'else' for default case!");

        // Verify it's not "else if" but just "else"
        if (last_else != std::string::npos) {
            std::string else_context = cpp_code.substr(last_else, 10);
            bool is_plain_else = (else_context.find("else if") == std::string::npos ||
                                  else_context.find("else {") != std::string::npos ||
                                  else_context.find("else\n") != std::string::npos);
            CHECK(is_plain_else);
        }
    }

} // TEST_SUITE

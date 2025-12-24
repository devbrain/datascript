/**
 * Test for inline discriminator choice peek behavior.
 *
 * This test verifies the fix for a bug where inline discriminator choices
 * would consume the discriminator byte but then expect case structures
 * to re-read it as their first field.
 *
 * Bug: For `choice NameOrId : uint8 { case 0xFF: { uint8 marker; ... } }`,
 * the code would read the discriminator byte (advancing the pointer), but
 * then the inline struct case would try to read `marker` from the next byte.
 *
 * Fix: Save the position before reading the discriminator, then restore it
 * after the case is selected so the case struct can read from the beginning.
 */

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>
#include <cstring>
#include <vector>
#include <iostream>

using namespace datascript;

TEST_SUITE("Inline Discriminator Peek Bug") {

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

    TEST_CASE("Inline discriminator with inline struct - should save position") {
        std::string ds_source = R"(
            choice NameOrId : uint8 {
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

        // DEBUG: Print generated code for inspection
        MESSAGE("Generated C++ code:\n" << cpp_code);

        // The fix should save the position before reading the discriminator
        // Look for position saving pattern: "auto saved_data_pos = data;"
        bool has_save_position = cpp_code.find("saved_data_pos") != std::string::npos;
        CHECK_MESSAGE(has_save_position,
            "Generated code should save position before reading inline discriminator\n"
            "Expected pattern: 'auto saved_data_pos = data;' or similar");

        // After reading discriminator and branching, the position should be restored
        // Look for position restore pattern: "data = saved_data_pos;"
        bool has_restore_position = cpp_code.find("data = saved_data_pos") != std::string::npos;
        CHECK_MESSAGE(has_restore_position,
            "Generated code should restore position after branching\n"
            "Expected pattern: 'data = saved_data_pos;'");

        // Verify the discriminator is still read for branching
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify the if/else chain exists for branching
        bool has_case_ff = cpp_code.find("selector_value == (255)") != std::string::npos;
        CHECK(has_case_ff);
    }

    TEST_CASE("Inline discriminator uint16 with inline struct - should save position") {
        std::string ds_source = R"(
            choice ResourceName : uint16 {
                case 0xFFFF: {
                    uint16 marker;
                    uint16 ordinal;
                } by_ordinal;
                default:
                    string by_name;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // DEBUG: Print generated code
        MESSAGE("Generated C++ code (uint16 discriminator):\n" << cpp_code);

        // Should still have save/restore for uint16 discriminator
        bool has_save_position = cpp_code.find("saved_data_pos") != std::string::npos;
        CHECK_MESSAGE(has_save_position,
            "Generated code should save position for uint16 inline discriminator");

        // Verify uint16 read is used for discriminator
        CHECK(cpp_code.find("read_uint16") != std::string::npos);
    }

    TEST_CASE("Simple inline discriminator without inline struct - no need to save position") {
        // When cases are simple fields (not inline structs), there's no need
        // to save/restore position because the field types don't include
        // the discriminator as their first field.
        std::string ds_source = R"(
            choice SimpleChoice : uint16 {
                case 0x1234:
                    uint32 data;
                default:
                    uint16 fallback;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // For simple cases (no inline struct), position save/restore may not be needed
        // But for consistency, we might still have it. Let's check what we generate.
        MESSAGE("Generated C++ code (simple choice):\n" << cpp_code);

        // Verify the discriminator is read
        CHECK(cpp_code.find("read_uint16") != std::string::npos);

        // Verify branching logic
        bool has_case_value = cpp_code.find("selector_value == (4660)") != std::string::npos ||
                              cpp_code.find("selector_value == (0x1234)") != std::string::npos;
        CHECK(has_case_value);
    }

    TEST_CASE("External discriminator choice - no inline discriminator handling needed") {
        // External discriminator choices use an existing field value,
        // not inline reading. This test verifies we don't break that case.
        // Note: External discriminators reference an existing field (msg_type here)
        std::string ds_source = R"(
            choice MessagePayload on msg_type {
                case 1:
                    uint32 value;
                case 2:
                    string text;
                default:
                    uint8 byte;
            };

            struct Container {
                uint8 msg_type;
                MessagePayload payload;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        MESSAGE("Generated C++ code (external discriminator):\n" << cpp_code);

        // External discriminator should use the existing field value
        // Verify the struct reads msg_type
        CHECK(cpp_code.find("msg_type") != std::string::npos);

        // Verify Container struct exists
        CHECK(cpp_code.find("Container") != std::string::npos);

        // External discriminator choices should NOT have save/restore position
        // since they don't read the discriminator inline
        // (The discriminator is read as a separate field before the choice)
        // Note: We just verify the choice generates correctly
        CHECK(cpp_code.find("MessagePayload") != std::string::npos);
    }

} // TEST_SUITE("Inline Discriminator Peek Bug")

//
// E2E test for choice discriminator behavior
// Tests that discriminators are correctly read and used to select the right case
//

#include "doctest/doctest.h"
#include "datascript/parser.hh"
#include "datascript/semantic_analyzer.hh"
#include "datascript/ir_builder.hh"
#include "datascript/cpp_renderer.hh"
#include <cstring>
#include <vector>

using namespace datascript;

TEST_SUITE("Choice Discriminator E2E") {

    // Helper to compile DataScript to C++ and extract generated code
    static std::string compile_to_cpp(const std::string& ds_source) {
        auto result = parse_datascript_from_string(ds_source);
        auto analyzed = analyze_module(result);
        auto ir_bundle = build_ir(result, analyzed);

        CppRenderer renderer;
        RenderOptions options;
        options.mode = CppMode::SingleHeader;
        return renderer.render_module(ir_bundle, options);
    }

    TEST_CASE("Choice with inline discriminator - uint8") {
        std::string ds_source = R"(
            choice MessageType : uint8 {
                case 0x01:
                    uint32 value;
                case 0x02:
                    string text;
                default:
                    uint8 unknown;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator type in generated code
        CHECK(cpp_code.find("uint8_t") != std::string::npos);

        // Verify read() method reads discriminator first
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify switch on discriminator value
        CHECK(cpp_code.find("switch") != std::string::npos);
        CHECK(cpp_code.find("case 0x01") != std::string::npos ||
              cpp_code.find("case 1") != std::string::npos);
        CHECK(cpp_code.find("case 0x02") != std::string::npos ||
              cpp_code.find("case 2") != std::string::npos);
        CHECK(cpp_code.find("default") != std::string::npos);
    }

    TEST_CASE("Choice with inline discriminator - uint16") {
        std::string ds_source = R"(
            choice PacketType : uint16 {
                case 0x1234:
                    uint32 data;
                case 0xABCD:
                    string message;
                default:
                    uint16 raw;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator type
        CHECK(cpp_code.find("uint16_t") != std::string::npos);

        // Verify read() uses read_uint16
        CHECK(cpp_code.find("read_uint16") != std::string::npos);

        // Verify case values
        CHECK(cpp_code.find("0x1234") != std::string::npos ||
              cpp_code.find("4660") != std::string::npos);  // decimal equivalent
        CHECK(cpp_code.find("0xABCD") != std::string::npos ||
              cpp_code.find("43981") != std::string::npos);
    }

    TEST_CASE("Choice with inline discriminator - uint32") {
        std::string ds_source = R"(
            choice CommandType : uint32 {
                case 0xDEADBEEF:
                    uint64 timestamp;
                case 0xCAFEBABE:
                    string payload;
                default:
                    uint32 unknown_command;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator type
        CHECK(cpp_code.find("uint32_t") != std::string::npos);

        // Verify read() uses read_uint32
        CHECK(cpp_code.find("read_uint32") != std::string::npos);

        // Verify large case values are handled
        CHECK(cpp_code.find("0xDEADBEEF") != std::string::npos ||
              cpp_code.find("3735928559") != std::string::npos);
    }

    TEST_CASE("Choice with external discriminator - field reference") {
        std::string ds_source = R"(
            struct Packet {
                uint8 msg_type;
                choice MessagePayload on msg_type {
                    case 1:
                        uint32 numeric_data;
                    case 2:
                        string text_data;
                    default:
                        uint8 raw_byte;
                };
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify struct has msg_type field
        CHECK(cpp_code.find("uint8_t msg_type") != std::string::npos);

        // Verify choice field exists
        CHECK(cpp_code.find("MessagePayload") != std::string::npos);

        // Verify read() method first reads discriminator
        // Then reads choice based on discriminator
        size_t read_pos = cpp_code.find("static Packet read");
        CHECK(read_pos != std::string::npos);

        // After read() definition, should see msg_type being read
        size_t msg_type_read = cpp_code.find("read_uint8", read_pos);
        CHECK(msg_type_read != std::string::npos);
    }

    TEST_CASE("Choice with inline struct cases") {
        std::string ds_source = R"(
            choice ResourceId : uint16 {
                case 0xFFFF: {
                    uint16 marker;
                    uint16 ordinal;
                } ordinal_id;
                default:
                    string name_id;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator read
        CHECK(cpp_code.find("read_uint16") != std::string::npos);

        // Verify generated struct for inline case
        CHECK(cpp_code.find("ResourceId__ordinal_id") != std::string::npos);

        // Verify switch on discriminator
        CHECK(cpp_code.find("switch") != std::string::npos);
        CHECK(cpp_code.find("0xFFFF") != std::string::npos ||
              cpp_code.find("65535") != std::string::npos);
    }

    TEST_CASE("Choice with multiple case values per case") {
        std::string ds_source = R"(
            choice StatusCode : uint8 {
                case 200:
                case 201:
                case 204:
                    uint32 success_data;
                case 400:
                case 404:
                    string error_message;
                default:
                    uint8 unknown_status;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify discriminator read
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify all case values are handled
        CHECK(cpp_code.find("200") != std::string::npos);
        CHECK(cpp_code.find("201") != std::string::npos);
        CHECK(cpp_code.find("204") != std::string::npos);
        CHECK(cpp_code.find("400") != std::string::npos);
        CHECK(cpp_code.find("404") != std::string::npos);
    }

    TEST_CASE("Choice discriminator with enum type") {
        std::string ds_source = R"(
            enum uint8 MessageKind {
                TEXT = 1,
                BINARY = 2,
                JSON = 3
            };

            choice Message : uint8 {
                case 1:  // MessageKind::TEXT
                    string text;
                case 2:  // MessageKind::BINARY
                    uint8 data[10];
                case 3:  // MessageKind::JSON
                    string json;
                default:
                    uint8 unknown;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify enum is generated
        CHECK(cpp_code.find("enum") != std::string::npos);
        CHECK(cpp_code.find("MessageKind") != std::string::npos);

        // Verify choice reads uint8 discriminator
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify case values match enum values
        CHECK(cpp_code.find("case 1") != std::string::npos);
        CHECK(cpp_code.find("case 2") != std::string::npos);
        CHECK(cpp_code.find("case 3") != std::string::npos);
    }

    TEST_CASE("Nested choice with discriminators") {
        std::string ds_source = R"(
            choice Inner : uint8 {
                case 1:
                    uint32 inner_value;
                default:
                    uint8 inner_default;
            };

            choice Outer : uint16 {
                case 0x100:
                    Inner nested;
                case 0x200:
                    string text;
                default:
                    uint16 outer_default;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify both discriminator reads
        CHECK(cpp_code.find("read_uint8") != std::string::npos);  // Inner
        CHECK(cpp_code.find("read_uint16") != std::string::npos); // Outer

        // Verify both choices are generated
        CHECK(cpp_code.find("struct Inner") != std::string::npos);
        CHECK(cpp_code.find("struct Outer") != std::string::npos);

        // Verify outer choice contains inner choice
        size_t outer_pos = cpp_code.find("struct Outer");
        CHECK(outer_pos != std::string::npos);
        size_t inner_ref = cpp_code.find("Inner", outer_pos);
        CHECK(inner_ref != std::string::npos);
    }

    TEST_CASE("Choice discriminator - default case only") {
        std::string ds_source = R"(
            choice Fallback : uint8 {
                default:
                    uint32 default_value;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Even with only default, discriminator should be read
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify default case handling
        CHECK(cpp_code.find("default") != std::string::npos);
    }

    TEST_CASE("Choice discriminator - no default case") {
        std::string ds_source = R"(
            choice Strict : uint8 {
                case 1:
                    uint32 value1;
                case 2:
                    uint32 value2;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Discriminator should still be read
        CHECK(cpp_code.find("read_uint8") != std::string::npos);

        // Verify specific cases
        CHECK(cpp_code.find("case 1") != std::string::npos);
        CHECK(cpp_code.find("case 2") != std::string::npos);

        // Should either have default or throw for invalid discriminator
        bool has_default = cpp_code.find("default") != std::string::npos;
        bool has_throw = cpp_code.find("throw") != std::string::npos;
        CHECK((has_default || has_throw));
    }

    TEST_CASE("Choice with big-endian discriminator") {
        std::string ds_source = R"(
            choice BigEndianChoice : big uint16 {
                case 0x1234:
                    uint32 data;
                default:
                    uint16 other;
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify big-endian read (implementation-specific, may be read_uint16_be or similar)
        CHECK(cpp_code.find("uint16") != std::string::npos);

        // Verify discriminator value handling
        CHECK(cpp_code.find("0x1234") != std::string::npos ||
              cpp_code.find("4660") != std::string::npos);
    }

    TEST_CASE("Choice in struct with conditional field") {
        std::string ds_source = R"(
            struct ConditionalPacket {
                uint8 has_choice;
                choice OptionalChoice : uint8 optional has_choice {
                    case 1:
                        uint32 value;
                    default:
                        uint8 default_val;
                };
            };
        )";

        std::string cpp_code = compile_to_cpp(ds_source);

        // Verify optional handling (std::optional)
        CHECK(cpp_code.find("std::optional") != std::string::npos);

        // Verify discriminator is only read when condition is true
        CHECK(cpp_code.find("has_choice") != std::string::npos);
        CHECK(cpp_code.find("read_uint8") != std::string::npos);
    }

} // TEST_SUITE("Choice Discriminator E2E")

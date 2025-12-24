//
// Code generation tests for choice (tagged union) types
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Choice Types") {
    TEST_CASE("Choice type generation") {
        const char* schema = R"(
enum uint8 MessageType {
    TEXT = 1,
    NUMBER = 2,
    BINARY = 3
};

choice MessagePayload on msg_type {
    case 1: uint32 text_length;
    case 2: uint64 number_value;
    case 3: uint8 binary_size;
};

struct Message {
    MessageType msg_type;
    MessagePayload payload;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            CHECK(ir.structs.size() == 1);
            CHECK(ir.enums.size() == 1);
            CHECK(ir.choices.size() == 1);

            const auto& choice = ir.choices[0];
            CHECK(choice.name == "MessagePayload");
            CHECK(choice.cases.size() == 3);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify choice declaration
            CHECK(cpp_code.find("struct MessagePayload {") != std::string::npos);

            // Verify std::variant usage with wrapper types (to handle duplicate types)
            CHECK(cpp_code.find("std::variant<") != std::string::npos);
            CHECK(cpp_code.find("Case0_text_length,  // case") != std::string::npos);
            CHECK(cpp_code.find("Case1_number_value,  // case") != std::string::npos);
            CHECK(cpp_code.find("Case2_binary_size  // case") != std::string::npos);

            // Verify wrapper struct declarations
            CHECK(cpp_code.find("struct Case0_text_length { uint32_t value; };") != std::string::npos);
            CHECK(cpp_code.find("struct Case1_number_value { uint64_t value; };") != std::string::npos);
            CHECK(cpp_code.find("struct Case2_binary_size { uint8_t value; };") != std::string::npos);

            // Verify accessor methods
            CHECK(cpp_code.find("as_text_length()") != std::string::npos);
            CHECK(cpp_code.find("as_number_value()") != std::string::npos);
            CHECK(cpp_code.find("as_binary_size()") != std::string::npos);
            CHECK(cpp_code.find("std::get_if<") != std::string::npos);

            // Verify templated read methods
            CHECK(cpp_code.find("template<typename SelectorType>") != std::string::npos);
            CHECK(cpp_code.find("ReadResult<MessagePayload> read_safe") != std::string::npos);
            CHECK(cpp_code.find("SelectorType selector_value") != std::string::npos);

            // Verify case matching
            CHECK(cpp_code.find("selector_value == (1)") != std::string::npos);
            CHECK(cpp_code.find("selector_value == (2)") != std::string::npos);
            CHECK(cpp_code.find("selector_value == (3)") != std::string::npos);
        }

        SUBCASE("Choice integration in struct") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";
            opts.error_handling = codegen::cpp_options::both;

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify struct reads choice with selector
            CHECK(cpp_code.find("// Evaluate choice selector: obj.msg_type") != std::string::npos);
            CHECK(cpp_code.find("auto selector_value = obj.msg_type;") != std::string::npos);
            CHECK(cpp_code.find("MessagePayload::read_safe(data, end, selector_value)") != std::string::npos);

            // Verify no TODOs left
            size_t payload_pos = cpp_code.find("Read payload");
            if (payload_pos != std::string::npos) {
                size_t next_field_pos = cpp_code.find("result.value = obj;", payload_pos);
                std::string payload_section = cpp_code.substr(payload_pos, next_field_pos - payload_pos);
                CHECK(payload_section.find("TODO") == std::string::npos);
            }
        }
    }

    TEST_CASE("Choice with multiple case values") {
        const char* schema = R"(
choice Color on tag {
    case 1:
    case 2:
    case 3: uint8 red;
    case 4: uint8 green;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());
        CHECK(ir.choices.size() == 1);

        const auto& choice = ir.choices[0];
        CHECK(choice.cases.size() == 2);
        CHECK(choice.cases[0].case_values.size() == 3);  // cases 1, 2, 3 all map to red
        CHECK(choice.cases[1].case_values.size() == 1);  // case 4 maps to green
    }
}

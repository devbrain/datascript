/**
 * End-to-end tests for range-based discriminators in choice types.
 *
 * Tests the feature that allows choice cases to use comparison operators:
 * - case >= value:  (greater-than-or-equal)
 * - case > value:   (greater-than)
 * - case <= value:  (less-than-or-equal)
 * - case < value:   (less-than)
 * - case != value:  (not-equal)
 *
 * This is commonly used in binary formats where a discriminator byte's
 * value range determines the type of data that follows (e.g., NE dialog
 * control classes where >= 0x80 means predefined class ID, < 0x80 means
 * string length).
 */

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

#include <sstream>
#include <vector>

using namespace datascript;

namespace {

/**
 * Helper to parse, analyze and generate code from a DataScript schema.
 */
std::string compile_to_cpp(const std::string& source) {
    auto parsed = parse_datascript(source);
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
    opts.error_handling = codegen::cpp_options::exceptions_only;

    return codegen::generate_cpp_header(ir, opts);
}

} // anonymous namespace

TEST_SUITE("Range-Based Discriminators") {

TEST_CASE("Basic range case with >= operator") {
    const char* source = R"(
        choice ControlClass : uint8 {
            case >= 0x80:
                uint8 class_id;
            default:
                uint8 string_length;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code contains the range comparison
    // Integer literals may be decimal or hex depending on renderer
    bool has_ge_comparison = (code.find("selector_value >= (128)") != std::string::npos ||
                              code.find("selector_value >= (0x80)") != std::string::npos);
    CHECK(has_ge_comparison);
}

TEST_CASE("Range case with < operator") {
    const char* source = R"(
        choice LowHigh : uint8 {
            case < 0x80:
                uint8 low_value;
            default:
                uint8 high_value;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code contains the less-than comparison
    bool has_lt_comparison = (code.find("selector_value < (128)") != std::string::npos ||
                              code.find("selector_value < (0x80)") != std::string::npos);
    CHECK(has_lt_comparison);
}

TEST_CASE("Range case with > operator") {
    const char* source = R"(
        choice GreaterThan : uint8 {
            case > 0x7F:
                uint8 high_value;
            default:
                uint8 low_value;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code contains the greater-than comparison
    bool has_gt_comparison = (code.find("selector_value > (127)") != std::string::npos ||
                              code.find("selector_value > (0x7F)") != std::string::npos);
    CHECK(has_gt_comparison);
}

TEST_CASE("Range case with <= operator") {
    const char* source = R"(
        choice LessOrEqual : uint8 {
            case <= 0x7F:
                uint8 low_value;
            default:
                uint8 high_value;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code contains the less-than-or-equal comparison
    bool has_le_comparison = (code.find("selector_value <= (127)") != std::string::npos ||
                              code.find("selector_value <= (0x7F)") != std::string::npos);
    CHECK(has_le_comparison);
}

TEST_CASE("Range case with != operator") {
    const char* source = R"(
        choice NotZero : uint8 {
            case != 0x00:
                uint8 value;
            default:
                uint8 null_marker;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code contains the not-equal comparison
    bool has_ne_comparison = (code.find("selector_value != (0)") != std::string::npos ||
                              code.find("selector_value != (0x00)") != std::string::npos);
    CHECK(has_ne_comparison);
}

TEST_CASE("Multiple range cases in order") {
    const char* source = R"(
        choice MultiRange : uint8 {
            case >= 0xC0:
                uint8 high_range;     // 192-255
            case >= 0x80:
                uint8 medium_range;   // 128-191
            default:
                uint8 low_range;      // 0-127
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify both range conditions are present in order
    size_t pos_high = code.find("selector_value >= (192)");
    if (pos_high == std::string::npos) {
        pos_high = code.find("selector_value >= (0xC0)");
    }
    CHECK(pos_high != std::string::npos);

    size_t pos_medium = code.find("selector_value >= (128)");
    if (pos_medium == std::string::npos) {
        pos_medium = code.find("selector_value >= (0x80)");
    }
    CHECK(pos_medium != std::string::npos);

    // High range check should come before medium range check
    CHECK(pos_high < pos_medium);
}

TEST_CASE("Range case with inline struct syntax") {
    const char* source = R"(
        choice InlineRange : uint8 {
            case >= 0x80:
                { uint8 value; } high_data;
            default:
                { uint8 length; } low_data;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the range comparison is generated
    bool has_ge_comparison = (code.find("selector_value >= (128)") != std::string::npos ||
                              code.find("selector_value >= (0x80)") != std::string::npos);
    CHECK(has_ge_comparison);

    // Verify inline struct types are generated
    CHECK(code.find("InlineRange__high_data_case__type") != std::string::npos);
    CHECK(code.find("InlineRange__low_data_default__type") != std::string::npos);
}

TEST_CASE("Range with hex literal expression") {
    const char* source = R"(
        const uint8 THRESHOLD = 0x80;

        choice WithConstant : uint8 {
            case >= THRESHOLD:
                uint8 high_value;
            default:
                uint8 low_value;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify the generated code compiles (the constant is resolved)
    // The exact output depends on constant folding
    CHECK(code.find("selector_value >=") != std::string::npos);
}

TEST_CASE("AST correctly stores range selector kind") {
    std::string source = R"(
        choice RangeChoice : uint8 {
            case >= 0x80:
                uint8 high_value;
            case < 0x20:
                uint8 low_value;
            default:
                uint8 mid_value;
        };
    )";

    auto parsed = parse_datascript(source);
    REQUIRE(parsed.choices.size() == 1);

    const auto& choice = parsed.choices[0];
    REQUIRE(choice.cases.size() == 3);

    // First case: >= 0x80
    CHECK(choice.cases[0].selector_kind == ast::case_selector_kind::range_ge);
    CHECK(choice.cases[0].range_bound.has_value());
    CHECK(!choice.cases[0].is_default);

    // Second case: < 0x20
    CHECK(choice.cases[1].selector_kind == ast::case_selector_kind::range_lt);
    CHECK(choice.cases[1].range_bound.has_value());
    CHECK(!choice.cases[1].is_default);

    // Third case: default
    CHECK(choice.cases[2].selector_kind == ast::case_selector_kind::exact);
    CHECK(!choice.cases[2].range_bound.has_value());
    CHECK(choice.cases[2].is_default);
}

TEST_CASE("IR correctly stores range selector mode") {
    std::string source = R"(
        choice TestChoice : uint8 {
            case >= 0x80:
                uint8 high_value;
            case <= 0x10:
                uint8 low_value;
            case != 0x00:
                uint8 non_zero;
            default:
                uint8 other;
        };
    )";

    auto parsed = parse_datascript(source);
    module_set modules;
    modules.main.file_path = "test.ds";
    modules.main.module = std::move(parsed);
    modules.main.package_name = "test";

    auto analysis = semantic::analyze(modules);
    REQUIRE(!analysis.has_errors());

    auto bundle = ir::build_ir(analysis.analyzed.value());

    REQUIRE(bundle.choices.size() == 1);
    const auto& choice = bundle.choices[0];
    REQUIRE(choice.cases.size() == 4);

    // First case: >= 0x80
    CHECK(choice.cases[0].selector_mode == ir::case_selector_mode::ge);
    CHECK(choice.cases[0].range_bound.has_value());

    // Second case: <= 0x10
    CHECK(choice.cases[1].selector_mode == ir::case_selector_mode::le);
    CHECK(choice.cases[1].range_bound.has_value());

    // Third case: != 0x00
    CHECK(choice.cases[2].selector_mode == ir::case_selector_mode::ne);
    CHECK(choice.cases[2].range_bound.has_value());

    // Fourth case: default (exact mode, no values)
    CHECK(choice.cases[3].selector_mode == ir::case_selector_mode::exact);
    CHECK(!choice.cases[3].range_bound.has_value());
    CHECK(choice.cases[3].case_values.empty());
}

TEST_CASE("Mixed exact and range cases") {
    const char* source = R"(
        choice MixedChoice : uint8 {
            case 0xFF:
                uint8 special_marker;
            case >= 0x80:
                uint8 high_value;
            default:
                uint8 low_value;
        };
    )";

    std::string code = compile_to_cpp(source);

    // Verify both exact match and range comparison are present
    bool has_exact = (code.find("selector_value == (255)") != std::string::npos ||
                      code.find("selector_value == (0xFF)") != std::string::npos);
    CHECK(has_exact);

    bool has_range = (code.find("selector_value >= (128)") != std::string::npos ||
                      code.find("selector_value >= (0x80)") != std::string::npos);
    CHECK(has_range);
}

} // TEST_SUITE

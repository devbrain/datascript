/**
 * Library Mode E2E Test: Conditional Fields
 * Comprehensive test coverage for library mode conditional field handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/LMSimpleConditional_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_lm_simple_conditional_with_value() {
    return std::vector<uint8_t>{
        0x01,              // flag = 1 (condition true)
        0x78, 0x56, 0x34, 0x12  // value = 0x12345678 (LE)
    };
}

std::vector<uint8_t> create_lm_simple_conditional_without_value() {
    return std::vector<uint8_t>{
        0x00               // flag = 0 (condition false, no value follows)
    };
}

std::vector<uint8_t> create_lm_multiple_conditionals_all() {
    return std::vector<uint8_t>{
        0x07,              // flags = 0x07 (all three bits set)
        0x34, 0x12,        // field_a = 0x1234
        0x78, 0x56,        // field_b = 0x5678
        0xBC, 0x9A         // field_c = 0x9ABC
    };
}

std::vector<uint8_t> create_lm_multiple_conditionals_partial() {
    return std::vector<uint8_t>{
        0x05,              // flags = 0x05 (field_a and field_c)
        0x34, 0x12,        // field_a = 0x1234
        0xBC, 0x9A         // field_c = 0x9ABC (field_b skipped)
    };
}

std::vector<uint8_t> create_lm_multiple_conditionals_none() {
    return std::vector<uint8_t>{
        0x00               // flags = 0x00 (no fields)
    };
}

std::vector<uint8_t> create_lm_optional_syntax_with_data() {
    return std::vector<uint8_t>{
        0x01,              // has_data = 1
        0x78, 0x56, 0x34, 0x12  // data = 0x12345678 (LE)
    };
}

std::vector<uint8_t> create_lm_optional_syntax_without_data() {
    return std::vector<uint8_t>{
        0x00               // has_data = 0
    };
}

std::vector<uint8_t> create_lm_nested_conditional_with_inner() {
    return std::vector<uint8_t>{
        0x01,              // outer_flag = 1 (condition true)
        0x01,              // inner.flag = 1
        0x78, 0x56, 0x34, 0x12  // inner.value = 0x12345678
    };
}

std::vector<uint8_t> create_lm_nested_conditional_without_inner() {
    return std::vector<uint8_t>{
        0x00               // outer_flag = 0 (no inner struct)
    };
}

// ============================================================================
// Test Suite 1: Simple Conditional - Value Present
// ============================================================================

TEST_CASE("LM Simple conditional - value present") {
    auto data = create_lm_simple_conditional_with_value();
    LMSimpleConditional sc = parse_LMSimpleConditional(data);

    CHECK(sc.flag == 1);
    CHECK(sc.value == 0x12345678);
}

// ============================================================================
// Test Suite 2: Simple Conditional - Value Absent
// ============================================================================

TEST_CASE("LM Simple conditional - value absent") {
    auto data = create_lm_simple_conditional_without_value();
    LMSimpleConditional sc = parse_LMSimpleConditional(data);

    CHECK(sc.flag == 0);
    // value is not read when condition is false (has uninitialized/default value)
}

// ============================================================================
// Test Suite 3: Multiple Conditionals - All Present
// ============================================================================

TEST_CASE("LM Multiple conditionals - all fields present") {
    auto data = create_lm_multiple_conditionals_all();
    LMMultipleConditionals mc = parse_LMMultipleConditionals(data);

    CHECK(mc.flags == 0x07);
    CHECK(mc.field_a == 0x1234);
    CHECK(mc.field_b == 0x5678);
    CHECK(mc.field_c == 0x9ABC);
}

// ============================================================================
// Test Suite 4: Multiple Conditionals - Partial
// ============================================================================

TEST_CASE("LM Multiple conditionals - partial fields") {
    auto data = create_lm_multiple_conditionals_partial();
    LMMultipleConditionals mc = parse_LMMultipleConditionals(data);

    CHECK(mc.flags == 0x05);
    CHECK(mc.field_a == 0x1234);
    // field_b not read (condition false)
    CHECK(mc.field_c == 0x9ABC);
}

// ============================================================================
// Test Suite 5: Multiple Conditionals - None Present
// ============================================================================

TEST_CASE("LM Multiple conditionals - no fields") {
    auto data = create_lm_multiple_conditionals_none();
    LMMultipleConditionals mc = parse_LMMultipleConditionals(data);

    CHECK(mc.flags == 0x00);
    // All fields not read (conditions false)
}

// ============================================================================
// Test Suite 6: Optional Syntax Sugar
// ============================================================================

TEST_CASE("LM Optional syntax - data present") {
    auto data = create_lm_optional_syntax_with_data();
    LMOptionalSyntax os = parse_LMOptionalSyntax(data);

    CHECK(os.has_data == 1);
    CHECK(os.data == 0x12345678);
}

TEST_CASE("LM Optional syntax - data absent") {
    auto data = create_lm_optional_syntax_without_data();
    LMOptionalSyntax os = parse_LMOptionalSyntax(data);

    CHECK(os.has_data == 0);
    // data not read (condition false)
}

// ============================================================================
// Test Suite 7: Nested Conditional
// ============================================================================

TEST_CASE("LM Nested conditional - inner present") {
    auto data = create_lm_nested_conditional_with_inner();
    LMNestedConditional nc = parse_LMNestedConditional(data);

    CHECK(nc.outer_flag == 1);
    CHECK(nc.inner.flag == 1);
    CHECK(nc.inner.value == 0x12345678);
}

TEST_CASE("LM Nested conditional - inner absent") {
    auto data = create_lm_nested_conditional_without_inner();
    LMNestedConditional nc = parse_LMNestedConditional(data);

    CHECK(nc.outer_flag == 0);
    // inner not read (condition false)
}

// ============================================================================
// Test Suite 8: Introspection Features
// ============================================================================

TEST_CASE("LM Conditional struct - introspection") {
    auto data = create_lm_simple_conditional_with_value();
    LMSimpleConditional sc = parse_LMSimpleConditional(data);

    StructView<LMSimpleConditional> view(&sc);

    CHECK(view.type_name() == std::string("LMSimpleConditional"));
    CHECK(view.field_count() == 2);

    // Check field metadata
    Field flag_field = view.field(0);
    CHECK(flag_field.name() == std::string("flag"));

    Field value_field = view.field(1);
    CHECK(value_field.name() == std::string("value"));
}

/**
 * Test for inline discriminator default case bug.
 *
 * Bug: When using inline discriminator choices, the default case consumes
 * the discriminator byte, but it should NOT be consumed because it's part
 * of the actual data.
 *
 * This test verifies:
 * 1. Default case: discriminator byte is NOT consumed (string/data starts from it)
 * 2. Explicit case: discriminator byte IS consumed (data starts after it)
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/inline_discrim_default_bug_impl.h"
#include <vector>
#include <cstdint>
#include <cstring>

using namespace generated;

// ============================================================================
// Test Data Helpers
// ============================================================================

// NEControlText: string case (default) - "Hello"
// Discriminator 0x48 ('H') is NOT 0xFF, so default case.
// String should read FROM the discriminator position, giving "Hello".
// Bug behavior: String reads AFTER discriminator, giving "ello".
std::vector<uint8_t> create_ne_string_hello() {
    return std::vector<uint8_t>{
        0x48,                     // 'H' - discriminator, also first byte of string
        0x65, 0x6C, 0x6C, 0x6F,   // "ello"
        0x00                      // null terminator
    };
}

// NEControlText: string case (default) - "Test"
// Discriminator 0x54 ('T') is NOT 0xFF, so default case.
std::vector<uint8_t> create_ne_string_test() {
    return std::vector<uint8_t>{
        0x54,              // 'T' - discriminator, also first byte of string
        0x65, 0x73, 0x74,  // "est"
        0x00               // null terminator
    };
}

// NEControlText: ordinal case - 0xFF + ordinal 100
// Discriminator 0xFF triggers ordinal case.
// Ordinal should be read AFTER the 0xFF.
std::vector<uint8_t> create_ne_ordinal() {
    return std::vector<uint8_t>{
        0xFF,        // discriminator = 0xFF (ordinal case)
        0x64, 0x00   // ordinal = 100 (little-endian)
    };
}

// PEResourceNameOrId: name case (default) - first char is 'A' (0x0041)
// Discriminator 0x0041 is NOT 0xFFFF, so default case.
// first_char should be 0x0041 (the discriminator itself).
std::vector<uint8_t> create_pe_name() {
    return std::vector<uint8_t>{
        0x41, 0x00   // discriminator = 0x0041 ('A'), also first_char value
    };
}

// PEResourceNameOrId: ordinal case - 0xFFFF + ordinal 42
// Discriminator 0xFFFF triggers ordinal case.
// Ordinal should be read AFTER the 0xFFFF.
std::vector<uint8_t> create_pe_ordinal() {
    return std::vector<uint8_t>{
        0xFF, 0xFF,  // discriminator = 0xFFFF (ordinal case)
        0x2A, 0x00   // ordinal = 42 (little-endian)
    };
}

// ExplicitCaseControl: case 0x01 - data
// Discriminator 0x01 triggers case 0x01.
// data should be read AFTER the 0x01 (discriminator consumed).
std::vector<uint8_t> create_explicit_case_data() {
    return std::vector<uint8_t>{
        0x01,                    // discriminator = 0x01
        0x78, 0x56, 0x34, 0x12   // data = 0x12345678 (little-endian)
    };
}

// ExplicitCaseControl: default case - "Hello"
// Discriminator 0x48 ('H') is NOT 0xFF or 0x01, so default case.
// String should read FROM the discriminator position.
std::vector<uint8_t> create_explicit_default_hello() {
    return std::vector<uint8_t>{
        0x48,                     // 'H' - discriminator, also first byte of string
        0x65, 0x6C, 0x6C, 0x6F,   // "ello"
        0x00                      // null terminator
    };
}

// ============================================================================
// Test Suite 1: Default Case - Discriminator NOT Consumed
// ============================================================================

TEST_SUITE("Inline Discriminator Default Case Bug") {

TEST_CASE("NEControlText string - discriminator byte is part of string") {
    auto data = create_ne_string_hello();
    NEControlItem item = parse_NEControlItem(data);

    // Should be default (string) case
    auto* text = item.control_text.as_text();
    REQUIRE(text != nullptr);

    // KEY TEST: String should be "Hello" (6 bytes including null), not "ello"
    // If bug exists: string = "ello" (discriminator consumed)
    // If fixed: string = "Hello" (discriminator NOT consumed, part of string)
    CHECK(text->value == "Hello");
    CHECK(text->value.size() == 5);  // "Hello" without null
    CHECK(text->value[0] == 'H');    // First char should be 'H' (0x48)
}

TEST_CASE("NEControlText string - another string test") {
    auto data = create_ne_string_test();
    NEControlItem item = parse_NEControlItem(data);

    auto* text = item.control_text.as_text();
    REQUIRE(text != nullptr);

    // String should be "Test", not "est"
    CHECK(text->value == "Test");
    CHECK(text->value.size() == 4);
    CHECK(text->value[0] == 'T');  // First char should be 'T' (0x54)
}

TEST_CASE("NEControlText ordinal - discriminator IS consumed") {
    auto data = create_ne_ordinal();
    NEControlItem item = parse_NEControlItem(data);

    // Should be ordinal case (0xFF)
    auto* ordinal = item.control_text.as_ordinal();
    REQUIRE(ordinal != nullptr);

    // Ordinal should be 100, read AFTER the 0xFF discriminator
    CHECK(ordinal->value == 100);
}

TEST_CASE("PEResourceNameOrId name - discriminator byte is part of data") {
    auto data = create_pe_name();
    PEResourceRef ref = parse_PEResourceRef(data);

    auto* name = ref.name_or_id.as_first_char();
    REQUIRE(name != nullptr);

    // KEY TEST: first_char should be 0x0041 (the discriminator value itself)
    // If bug exists: first_char reads AFTER discriminator (undefined/zero)
    // If fixed: first_char = 0x0041 (discriminator NOT consumed)
    CHECK(name->value == 0x0041);
}

TEST_CASE("PEResourceNameOrId ordinal - discriminator IS consumed") {
    auto data = create_pe_ordinal();
    PEResourceRef ref = parse_PEResourceRef(data);

    auto* ordinal = ref.name_or_id.as_ordinal();
    REQUIRE(ordinal != nullptr);

    // Ordinal should be 42, read AFTER the 0xFFFF discriminator
    CHECK(ordinal->value == 42);
}

// ============================================================================
// Test Suite 2: Explicit Case - Discriminator IS Consumed
// ============================================================================

TEST_CASE("ExplicitCaseControl case 0x01 - discriminator IS consumed") {
    auto data = create_explicit_case_data();
    ExplicitCaseContainer container = parse_ExplicitCaseContainer(data);

    auto* data_case = container.value.as_value_data();
    REQUIRE(data_case != nullptr);

    // value_data should be 0x12345678, read AFTER the 0x01 discriminator
    CHECK(data_case->value == 0x12345678);
}

TEST_CASE("ExplicitCaseControl default - discriminator NOT consumed") {
    auto data = create_explicit_default_hello();
    ExplicitCaseContainer container = parse_ExplicitCaseContainer(data);

    auto* text = container.value.as_text();
    REQUIRE(text != nullptr);

    // String should be "Hello", starting from discriminator position
    CHECK(text->value == "Hello");
    CHECK(text->value[0] == 'H');
}

// ============================================================================
// Test Suite 3: Byte Count Verification
// ============================================================================

TEST_CASE("NEControlText string - correct byte count consumed") {
    // String "Hello\0" = 6 bytes total (discriminator + 4 more chars + null)
    // With fix: all 6 bytes consumed (discriminator is part of string)
    auto data = create_ne_string_hello();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    NEControlText result = NEControlText::read(ptr, end);

    // Should have consumed exactly 6 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 6);
}

TEST_CASE("NEControlText ordinal - correct byte count consumed") {
    // Ordinal case: 1 byte discriminator (0xFF) + 2 bytes ordinal = 3 bytes total
    auto data = create_ne_ordinal();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    NEControlText result = NEControlText::read(ptr, end);

    // Should have consumed exactly 3 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 3);
}

TEST_CASE("PEResourceNameOrId name - correct byte count consumed") {
    // Name case: 2 bytes (discriminator is the data)
    auto data = create_pe_name();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    PEResourceNameOrId result = PEResourceNameOrId::read(ptr, end);

    // Should have consumed exactly 2 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 2);
}

} // TEST_SUITE("Inline Discriminator Default Case Bug")

/**
 * Library Mode E2E Test: String Types
 * Comprehensive test coverage for library mode string handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/FixedString_impl.h"
#include <cstring>

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_fixed_string() {
    std::vector<uint8_t> data(16);
    const char* str = "Hello";
    std::memcpy(data.data(), str, 5);
    return data;
}

std::vector<uint8_t> create_prefixed_string() {
    return std::vector<uint8_t>{
        0x05,       // length = 5
        'H', 'e', 'l', 'l', 'o'
    };
}

std::vector<uint8_t> create_string_container() {
    return std::vector<uint8_t>{
        0x42,                              // tag = 66
        0x04,                              // name_len = 4
        'T', 'e', 's', 't',               // name
        0x07, 0x00,                        // description_len = 7 (uint16 LE)
        'E', 'x', 'a', 'm', 'p', 'l', 'e', // description
        // fixed_data[32] - zeros
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0,
        0, 0, 0, 0, 0, 0, 0, 0
    };
}

std::vector<uint8_t> create_outer_string() {
    return std::vector<uint8_t>{
        0x05,                    // inner.length = 5
        'H', 'e', 'l', 'l', 'o', // inner.chars
        0x06,                    // suffix_len = 6
        'W', 'o', 'r', 'l', 'd', '!'
    };
}

// ============================================================================
// Test Suite 1: Fixed-Length Strings
// ============================================================================

TEST_CASE("Fixed string - parse") {
    auto data = create_fixed_string();
    FixedString fs = parse_FixedString(data);

    CHECK(fs.data.size() == 16);
    CHECK(fs.data[0] == 'H');
    CHECK(fs.data[1] == 'e');
    CHECK(fs.data[2] == 'l');
    CHECK(fs.data[3] == 'l');
    CHECK(fs.data[4] == 'o');
}

// ============================================================================
// Test Suite 2: Length-Prefixed Strings
// ============================================================================

TEST_CASE("Prefixed string - parse") {
    auto data = create_prefixed_string();
    PrefixedString ps = parse_PrefixedString(data);

    CHECK(ps.length == 5);
    CHECK(ps.chars.size() == 5);
    CHECK(ps.chars[0] == 'H');
    CHECK(ps.chars[4] == 'o');
}

// ============================================================================
// Test Suite 3: String Container
// ============================================================================

TEST_CASE("String container - parse all fields") {
    auto data = create_string_container();
    StringContainer sc = parse_StringContainer(data);

    CHECK(sc.tag == 66);
    CHECK(sc.name_len == 4);
    CHECK(sc.name_chars.size() == 4);
    CHECK(sc.description_len == 7);
    CHECK(sc.description_chars.size() == 7);
    CHECK(sc.fixed_data.size() == 32);
}

// ============================================================================
// Test Suite 4: Nested Strings
// ============================================================================

TEST_CASE("Outer string - parse nested") {
    auto data = create_outer_string();
    OuterString os = parse_OuterString(data);

    CHECK(os.inner.length == 5);
    CHECK(os.inner.chars.size() == 5);
    CHECK(os.suffix_len == 6);
    CHECK(os.suffix.size() == 6);
}

// ============================================================================
// Test Suite 5: Introspection Features
// ============================================================================

TEST_CASE("String struct - introspection") {
    auto data = create_prefixed_string();
    PrefixedString ps = parse_PrefixedString(data);

    StructView<PrefixedString> view(&ps);

    CHECK(view.field_count() == 2);
    CHECK(std::string(view.type_name()) == "PrefixedString");
}

/**
 * Library Mode E2E Test: Inline Discriminator with Anonymous Block Cases
 *
 * This test verifies the fix for the bug where inline discriminator choices
 * would consume the discriminator byte but not restore position for inline
 * struct cases to re-read it as their first field.
 *
 * Bug report: When using `choice foo : uint8 { case 0xFF: { uint8 marker; ... } }`,
 * the discriminator is consumed, but the inline struct expects to read `marker`
 * as the discriminator value. Without the fix, `marker` would read the wrong byte.
 */

#include <doctest/doctest.h>
#include "IDNameOrOrdinal__ordinal_id_case__type_impl.h"
#include <vector>
#include <cstdint>

using namespace generated;

// ============================================================================
// Test Data Helpers
// ============================================================================

// IDNameOrOrdinal: ordinal case (0xFF marker)
// With fix: marker=0xFF, id_value=100 from bytes [0xFF, 0x64, 0x00]
std::vector<uint8_t> create_id_ordinal() {
    return std::vector<uint8_t>{
        0xFF,        // discriminator = 0xFF (ordinal case), also marker field
        0x64, 0x00   // id_value = 100 (little-endian)
    };
}

// IDNameOrOrdinal: string case (length=4, "Test")
// With fix: length=4, chars="Test" from bytes [0x04, 'T', 'e', 's', 't']
std::vector<uint8_t> create_id_string() {
    return std::vector<uint8_t>{
        0x04,              // discriminator = 4 (default case), also length field
        'T', 'e', 's', 't' // chars[4]
    };
}

// IDMixedChoice: special case (0xFFFF marker) - inline struct
// With fix: marker=0xFFFF, data=0x12345678 from bytes [0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12]
std::vector<uint8_t> create_id_mixed_special() {
    return std::vector<uint8_t>{
        0xFF, 0xFF,              // discriminator = 0xFFFF (special case), also marker field
        0x78, 0x56, 0x34, 0x12   // data = 0x12345678 (little-endian)
    };
}

// IDMixedChoice: regular case (0x0001) - NOT inline struct
// Discriminator is consumed, regular_data reads from next position
std::vector<uint8_t> create_id_mixed_regular() {
    return std::vector<uint8_t>{
        0x01, 0x00,              // discriminator = 1
        0x78, 0x56, 0x34, 0x12   // regular_data = 0x12345678 (little-endian)
    };
}

// IDMixedChoice: default case - position IS restored (discriminator is part of data)
// With the fix: discriminator is NOT consumed for default case
std::vector<uint8_t> create_id_mixed_default() {
    return std::vector<uint8_t>{
        0x99, 0x00   // discriminator = 153 (default case), also fallback value
    };
}

// IDSimpleInline: case 1 - regular field (no position restore)
std::vector<uint8_t> create_id_simple_case1() {
    return std::vector<uint8_t>{
        0x01,                    // discriminator = 1
        0x78, 0x56, 0x34, 0x12   // value_one = 0x12345678 (little-endian)
    };
}

// IDSimpleInline: case 2 - regular field (no position restore)
std::vector<uint8_t> create_id_simple_case2() {
    return std::vector<uint8_t>{
        0x02,        // discriminator = 2
        0x34, 0x12   // value_two = 0x1234 (little-endian)
    };
}

// IDSimpleInline: default case - position IS restored (discriminator is part of data)
// With the fix: discriminator is NOT consumed for default case
std::vector<uint8_t> create_id_simple_default() {
    return std::vector<uint8_t>{
        0x99   // discriminator = 153 (default case), also value_default
    };
}

// ============================================================================
// Test Suite 1: Inline Struct Cases (Position Should Be Restored)
// ============================================================================

TEST_CASE("ID Inline discriminator - ordinal case with inline struct") {
    auto data = create_id_ordinal();
    IDEntity entity = parse_IDEntity(data);

    // Should be ordinal case
    auto* ordinal = entity.identifier.as_ordinal_id();
    REQUIRE(ordinal != nullptr);

    // Key test: marker should be 0xFF (the discriminator value re-read)
    CHECK(ordinal->value.marker == 0xFF);
    CHECK(ordinal->value.id_value == 100);
}

TEST_CASE("ID Inline discriminator - string case with inline struct (default)") {
    auto data = create_id_string();
    IDEntity entity = parse_IDEntity(data);

    // Should be string case (default)
    auto* str = entity.identifier.as_string_name();
    REQUIRE(str != nullptr);

    // Key test: length should be 4 (the discriminator value re-read)
    CHECK(str->value.length == 4);
    REQUIRE(str->value.chars.size() == 4);
    CHECK(str->value.chars[0] == 'T');
    CHECK(str->value.chars[1] == 'e');
    CHECK(str->value.chars[2] == 's');
    CHECK(str->value.chars[3] == 't');
}

TEST_CASE("ID Mixed choice - special case with inline struct") {
    auto data = create_id_mixed_special();
    IDMixedContainer container = parse_IDMixedContainer(data);

    // Should be special case (inline struct)
    auto* special = container.value.as_special_case();
    REQUIRE(special != nullptr);

    // Key test: marker should be 0xFFFF (the discriminator value re-read)
    CHECK(special->value.marker == 0xFFFF);
    CHECK(special->value.data == 0x12345678);
}

// ============================================================================
// Test Suite 2: Regular Field Cases (Position Should NOT Be Restored)
// ============================================================================

TEST_CASE("ID Mixed choice - regular case (not inline struct)") {
    auto data = create_id_mixed_regular();
    IDMixedContainer container = parse_IDMixedContainer(data);

    // Should be case 1 (regular field, not inline struct)
    auto* regular = container.value.as_regular_data();
    REQUIRE(regular != nullptr);

    // Key test: regular_data reads AFTER discriminator (not re-reading it)
    CHECK(regular->value == 0x12345678);
}

TEST_CASE("ID Mixed choice - default case (position restored)") {
    auto data = create_id_mixed_default();
    IDMixedContainer container = parse_IDMixedContainer(data);

    // Should be default case (regular field)
    auto* fallback = container.value.as_fallback();
    REQUIRE(fallback != nullptr);

    // With the fix: fallback reads FROM discriminator position (not after)
    // discriminator = 153 (0x0099) is also fallback value
    CHECK(fallback->value == 153);
}

TEST_CASE("ID Simple inline - case 1 (no inline struct)") {
    auto data = create_id_simple_case1();
    IDSimpleContainer container = parse_IDSimpleContainer(data);

    // Should be case 1
    auto* case1 = container.choice_field.as_value_one();
    REQUIRE(case1 != nullptr);

    // value_one reads AFTER discriminator
    CHECK(case1->value == 0x12345678);
}

TEST_CASE("ID Simple inline - case 2 (no inline struct)") {
    auto data = create_id_simple_case2();
    IDSimpleContainer container = parse_IDSimpleContainer(data);

    // Should be case 2
    auto* case2 = container.choice_field.as_value_two();
    REQUIRE(case2 != nullptr);

    // value_two reads AFTER discriminator
    CHECK(case2->value == 0x1234);
}

TEST_CASE("ID Simple inline - default case (position restored)") {
    auto data = create_id_simple_default();
    IDSimpleContainer container = parse_IDSimpleContainer(data);

    // Should be default case
    auto* def = container.choice_field.as_value_default();
    REQUIRE(def != nullptr);

    // With the fix: value_default reads FROM discriminator position (not after)
    // discriminator = 153 (0x99) is also value_default
    CHECK(def->value == 153);
}

// ============================================================================
// Test Suite 3: Byte Count Verification
// ============================================================================

TEST_CASE("ID Inline struct case - correct byte count consumed") {
    // For ordinal case: 1 byte discriminator re-read as marker + 2 bytes id_value = 3 bytes total
    auto data = create_id_ordinal();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    IDNameOrOrdinal result = IDNameOrOrdinal::read(ptr, end);

    // Should have consumed exactly 3 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 3);
}

TEST_CASE("ID Inline struct case - string with correct byte count") {
    // For string case: 1 byte length (discriminator) + 4 bytes chars = 5 bytes total
    auto data = create_id_string();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    IDNameOrOrdinal result = IDNameOrOrdinal::read(ptr, end);

    // Should have consumed exactly 5 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 5);
}

TEST_CASE("ID Regular field case - correct byte count consumed") {
    // For regular case: 2 bytes discriminator + 4 bytes data = 6 bytes total
    auto data = create_id_mixed_regular();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    IDMixedChoice result = IDMixedChoice::read(ptr, end);

    // Should have consumed exactly 6 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 6);
}

// ============================================================================
// Test Suite 4: Introspection API
// ============================================================================

TEST_CASE("ID StructView for inline struct case") {
    auto data = create_id_ordinal();
    IDEntity entity = parse_IDEntity(data);

    StructView<IDEntity> view(&entity);

    CHECK(view.type_name() == std::string("IDEntity"));
    CHECK(view.field_count() == 1);

    auto field0 = view.field(0);
    CHECK(field0.name() == std::string("identifier"));
}

// ============================================================================
// Test Suite 5: Named Struct Cases (Non-Anonymous)
// These verify discriminator IS consumed (position NOT restored)
// ============================================================================

// NamedStructChoice with OrdinalData (named struct)
// Data: [0xFF, 0xAA, 0x42, 0x00]
// - discriminator = 0xFF (consumed)
// - first_byte = 0xAA (NOT 0xFF - reads AFTER discriminator)
// - id_value = 0x0042 = 66
std::vector<uint8_t> create_named_ordinal() {
    return std::vector<uint8_t>{
        0xFF,        // discriminator = 0xFF (consumed, NOT passed to struct)
        0xAA,        // first_byte = 0xAA (reads AFTER discriminator)
        0x42, 0x00   // id_value = 66 (little-endian)
    };
}

// NamedStructChoice with StringData (named struct)
// With the fix: position IS restored for default case (discriminator is part of data)
// Data: [0x05, 'H', 'e', 'l', 'l', 'o']
// - discriminator = 0x05 (default case), position restored to 0
// - length = 0x05 (reads from position 0 = discriminator value)
// - chars = ['H', 'e', 'l', 'l', 'o'] (5 bytes from position 1)
std::vector<uint8_t> create_named_string() {
    return std::vector<uint8_t>{
        0x05,                        // discriminator = 5 (default), also length
        'H', 'e', 'l', 'l', 'o'      // chars[5] (read from position 1)
    };
}

// MixedAnonAndNamed: anonymous block case (0xFFFF)
// Data: [0xFF, 0xFF, 0x78, 0x56, 0x34, 0x12]
// - discriminator = 0xFFFF (peeked, position restored)
// - marker = 0xFFFF (re-reads discriminator)
// - data = 0x12345678
std::vector<uint8_t> create_mixed_anon() {
    return std::vector<uint8_t>{
        0xFF, 0xFF,              // discriminator = 0xFFFF, also read as marker
        0x78, 0x56, 0x34, 0x12   // data = 0x12345678 (little-endian)
    };
}

// MixedAnonAndNamed: named struct case (0x0001)
// Data: [0x01, 0x00, 0xAA, 0xBB, 0x78, 0x56, 0x34, 0x12]
// - discriminator = 0x0001 (consumed)
// - first_word = 0xBBAA (reads AFTER discriminator, NOT 0x0001)
// - payload = 0x12345678
std::vector<uint8_t> create_mixed_named() {
    return std::vector<uint8_t>{
        0x01, 0x00,              // discriminator = 1 (consumed)
        0xAA, 0xBB,              // first_word = 0xBBAA (reads AFTER discriminator)
        0x78, 0x56, 0x34, 0x12   // payload = 0x12345678 (little-endian)
    };
}

TEST_CASE("Named struct case - ordinal (discriminator consumed)") {
    auto data = create_named_ordinal();
    NamedStructContainer container = parse_NamedStructContainer(data);

    // Should be ordinal case
    auto* ordinal = container.value.as_ordinal();
    REQUIRE(ordinal != nullptr);

    // Key test: first_byte should be 0xAA, NOT 0xFF
    // If position was incorrectly restored, first_byte would be 0xFF
    CHECK(ordinal->value.first_byte == 0xAA);
    CHECK(ordinal->value.id_value == 66);
}

TEST_CASE("Named struct case - string (position restored for default)") {
    auto data = create_named_string();
    NamedStructContainer container = parse_NamedStructContainer(data);

    // Should be default/string case
    auto* str = container.value.as_str();
    REQUIRE(str != nullptr);

    // With the fix: length = 5 (the discriminator value, since position is restored)
    // chars reads 5 bytes from position 1 = ['H', 'e', 'l', 'l', 'o']
    CHECK(str->value.length == 5);
    REQUIRE(str->value.chars.size() == 5);
    CHECK(str->value.chars[0] == 'H');
    CHECK(str->value.chars[1] == 'e');
    CHECK(str->value.chars[2] == 'l');
    CHECK(str->value.chars[3] == 'l');
    CHECK(str->value.chars[4] == 'o');
}

TEST_CASE("Mixed choice - anonymous block case (position restored)") {
    auto data = create_mixed_anon();
    MixedContainer container = parse_MixedContainer(data);

    // Should be anonymous block case
    auto* anon = container.value.as_anon_case();
    REQUIRE(anon != nullptr);

    // Key test: marker should be 0xFFFF (re-read from discriminator position)
    CHECK(anon->value.marker == 0xFFFF);
    CHECK(anon->value.data == 0x12345678);
}

TEST_CASE("Mixed choice - named struct case (discriminator consumed)") {
    auto data = create_mixed_named();
    MixedContainer container = parse_MixedContainer(data);

    // Should be named struct case
    auto* named = container.value.as_named_case();
    REQUIRE(named != nullptr);

    // Key test: first_word should be 0xBBAA, NOT 0x0001
    // If position was incorrectly restored, first_word would be 0x0001
    CHECK(named->value.first_word == 0xBBAA);
    CHECK(named->value.payload == 0x12345678);
}

TEST_CASE("Named struct case - correct byte count consumed") {
    // For named struct case: 1 byte discriminator + 1 byte first_byte + 2 bytes id_value = 4 bytes
    auto data = create_named_ordinal();
    const uint8_t* ptr = data.data();
    const uint8_t* end = ptr + data.size();

    NamedStructChoice result = NamedStructChoice::read(ptr, end);

    // Should have consumed exactly 4 bytes
    CHECK(ptr == end);
    CHECK(ptr - data.data() == 4);
}

TEST_CASE("Mixed choice - verify different byte counts for anon vs named") {
    // Anonymous block case: position restored, so 2 bytes discriminator (re-read as marker) + 4 bytes data = 6 bytes
    {
        auto data = create_mixed_anon();
        const uint8_t* ptr = data.data();
        const uint8_t* end = ptr + data.size();

        MixedAnonAndNamed result = MixedAnonAndNamed::read(ptr, end);

        CHECK(ptr == end);
        CHECK(ptr - data.data() == 6);
    }

    // Named struct case: discriminator consumed, then 2 bytes first_word + 4 bytes payload = 8 bytes total
    {
        auto data = create_mixed_named();
        const uint8_t* ptr = data.data();
        const uint8_t* end = ptr + data.size();

        MixedAnonAndNamed result = MixedAnonAndNamed::read(ptr, end);

        CHECK(ptr == end);
        CHECK(ptr - data.data() == 8);
    }
}

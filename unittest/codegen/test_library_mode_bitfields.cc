/**
 * Library Mode E2E Test: Bitfields
 * Comprehensive test coverage for library mode bitfield handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/library_mode_bitfields_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_simple_bitfield() {
    return std::vector<uint8_t>{
        0x03  // flags: flag_a=1, flag_b=1, reserved=0
    };
}

std::vector<uint8_t> create_multi_bitfield() {
    // Bitfields are packed LSB first:
    // Byte 0-1 (16 bits): mode(3) | priority(2) | enabled(1) | reserved(10)
    // mode=7 (bits 0-2: 111), priority=2 (bits 3-4: 10), enabled=1 (bit 5: 1), reserved=0
    // Binary: 0000000000 1 10 111 = 0b0000000000110111 = 0x0037
    // Byte 2 (8 bits): ready(1) | error(1) | padding(6)
    // ready=1, error=1, padding=0
    // Binary: 00000011 = 0x03
    return std::vector<uint8_t>{
        0x37, 0x00,  // Little endian: 0x0037
        0x03         // ready=1, error=1, padding=0
    };
}

std::vector<uint8_t> create_mixed_fields() {
    return std::vector<uint8_t>{
        0x42,              // header = 66
        0xA5, 0x00,        // flags: type=5, version=10 (0x00A5 = 0b0000000010100101)
        0x2A, 0x00, 0x00, 0x00  // data = 42
    };
}

// ============================================================================
// Test Suite 1: Simple Bitfield
// ============================================================================

TEST_CASE("Simple bitfield - parse") {
    auto data = create_simple_bitfield();
    SimpleBitfield sb = parse_SimpleBitfield(data);

    CHECK(sb.flag_a == 1);
    CHECK(sb.flag_b == 1);
    CHECK(sb.reserved == 0);
}

// ============================================================================
// Test Suite 2: Multiple Bitfields
// ============================================================================

TEST_CASE("Multi bitfield - parse all fields") {
    auto data = create_multi_bitfield();
    MultiBitfield mb = parse_MultiBitfield(data);

    CHECK(mb.mode == 7);
    CHECK(mb.priority == 2);
    CHECK(mb.enabled == 1);
    CHECK(mb.reserved == 0);
    CHECK(mb.ready == 1);
    CHECK(mb.error == 1);
    CHECK(mb.padding == 0);
}

// ============================================================================
// Test Suite 3: Mixed Fields
// ============================================================================

TEST_CASE("Mixed fields - parse all") {
    auto data = create_mixed_fields();
    MixedFields mf = parse_MixedFields(data);

    CHECK(mf.header == 66);
    CHECK(mf.type == 5);
    CHECK(mf.version == 10);
    CHECK(mf.options == 0);
    CHECK(mf.data == 42);
}

// ============================================================================
// Test Suite 4: Introspection Features
// ============================================================================

TEST_CASE("Bitfield struct - introspection") {
    auto data = create_simple_bitfield();
    SimpleBitfield sb = parse_SimpleBitfield(data);

    StructView<SimpleBitfield> view(&sb);

    CHECK(view.field_count() == 3);  // Three bitfield fields
    CHECK(std::string(view.type_name()) == "SimpleBitfield");
}

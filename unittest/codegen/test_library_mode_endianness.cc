/**
 * Library Mode E2E Test: Endianness
 * Comprehensive test coverage for library mode endianness handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/library_mode_endianness_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_little_endian() {
    return std::vector<uint8_t>{
        0x34, 0x12,              // word = 0x1234 (LE)
        0x78, 0x56, 0x34, 0x12,  // dword = 0x12345678 (LE)
        0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12  // qword = 0x1234567890ABCDEF (LE)
    };
}

std::vector<uint8_t> create_big_endian() {
    return std::vector<uint8_t>{
        0x12, 0x34,              // word = 0x1234 (BE)
        0x12, 0x34, 0x56, 0x78,  // dword = 0x12345678 (BE)
        0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF  // qword = 0x1234567890ABCDEF (BE)
    };
}

std::vector<uint8_t> create_mixed_endian() {
    return std::vector<uint8_t>{
        0x34, 0x12,              // le_word = 0x1234 (LE)
        0x56, 0x78,              // be_word = 0x5678 (BE)
        0xBC, 0x9A, 0x78, 0x56,  // le_dword = 0x56789ABC (LE)
        0xDE, 0xF0, 0x12, 0x34   // be_dword = 0xDEF01234 (BE)
    };
}

std::vector<uint8_t> create_network_header() {
    return std::vector<uint8_t>{
        0x01, 0xF4,              // packet_length = 500 (BE)
        0xAB, 0xCD,              // packet_id = 0xABCD (BE)
        0x5F, 0x5E, 0x0F, 0xF0,  // timestamp = 0x5F5E0FF0 (BE)
        0x42                     // flags = 0x42
    };
}

// ============================================================================
// Test Suite 1: Little-Endian
// ============================================================================

TEST_CASE("Little-endian - parse all fields") {
    auto data = create_little_endian();
    LittleEndianData led = parse_LittleEndianData(data);

    CHECK(led.word == 0x1234);
    CHECK(led.dword == 0x12345678);
    CHECK(led.qword == 0x1234567890ABCDEF);
}

// ============================================================================
// Test Suite 2: Big-Endian
// ============================================================================

TEST_CASE("Big-endian - parse all fields") {
    auto data = create_big_endian();
    BigEndianData bed = parse_BigEndianData(data);

    CHECK(bed.word == 0x1234);
    CHECK(bed.dword == 0x12345678);
    CHECK(bed.qword == 0x1234567890ABCDEF);
}

// ============================================================================
// Test Suite 3: Mixed Endianness
// ============================================================================

TEST_CASE("Mixed endianness - parse all fields") {
    auto data = create_mixed_endian();
    MixedEndianData med = parse_MixedEndianData(data);

    CHECK(med.le_word == 0x1234);
    CHECK(med.be_word == 0x5678);
    CHECK(med.le_dword == 0x56789ABC);
    CHECK(med.be_dword == 0xDEF01234);
}

// ============================================================================
// Test Suite 4: Network Header
// ============================================================================

TEST_CASE("Network header - parse") {
    auto data = create_network_header();
    NetworkHeader nh = parse_NetworkHeader(data);

    CHECK(nh.packet_length == 500);
    CHECK(nh.packet_id == 0xABCD);
    CHECK(nh.timestamp == 0x5F5E0FF0);
    CHECK(nh.flags == 0x42);
}

// ============================================================================
// Test Suite 5: Introspection Features
// ============================================================================

TEST_CASE("Endian struct - introspection") {
    auto data = create_mixed_endian();
    MixedEndianData med = parse_MixedEndianData(data);

    StructView<MixedEndianData> view(&med);

    CHECK(view.field_count() == 4);
    CHECK(std::string(view.type_name()) == "MixedEndianData");
}

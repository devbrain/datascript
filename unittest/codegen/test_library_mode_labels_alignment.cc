/**
 * Library Mode E2E Test: Labels and Alignment
 * Comprehensive test coverage for library mode label and alignment handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/SimpleLabel_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_simple_label() {
    std::vector<uint8_t> data;

    // data_offset = 10 (uint32 LE)
    data.push_back(0x0A);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // Padding bytes 4-9 (will be skipped by label seek)
    for (int i = 0; i < 6; i++) {
        data.push_back(0xFF);
    }

    // data_value at offset 10
    data.push_back(0x42);

    return data;
}

std::vector<uint8_t> create_simple_alignment() {
    std::vector<uint8_t> data;

    // header (1 byte)
    data.push_back(0x12);

    // Padding to 4-byte boundary (3 bytes)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // aligned_value at offset 4 (uint32 LE)
    data.push_back(0x78);
    data.push_back(0x56);
    data.push_back(0x34);
    data.push_back(0x12);

    return data;
}

std::vector<uint8_t> create_label_and_alignment() {
    std::vector<uint8_t> data;

    // data_offset = 12 (uint32 LE)
    data.push_back(0x0C);
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // Padding bytes 4-11
    for (int i = 0; i < 8; i++) {
        data.push_back(0xFF);
    }

    // unaligned_byte at offset 12
    data.push_back(0x42);

    // Padding to 8-byte boundary (3 bytes)
    data.push_back(0x00);
    data.push_back(0x00);
    data.push_back(0x00);

    // aligned_value at offset 16 (uint64 LE)
    for (int i = 0; i < 8; i++) {
        data.push_back(i + 1);
    }

    return data;
}

std::vector<uint8_t> create_multiple_alignments() {
    std::vector<uint8_t> data;

    // byte1 at offset 0
    data.push_back(0x42);

    // Padding to offset 2 (1 byte)
    data.push_back(0x00);

    // word at offset 2 (uint16 LE)
    data.push_back(0x34);
    data.push_back(0x12);

    // No padding needed (already at offset 4)

    // dword at offset 4 (uint32 LE)
    data.push_back(0x78);
    data.push_back(0x56);
    data.push_back(0x34);
    data.push_back(0x12);

    // No padding needed (already at offset 8)

    // qword at offset 8 (uint64 LE)
    data.push_back(0xEF);
    data.push_back(0xCD);
    data.push_back(0xAB);
    data.push_back(0x90);
    data.push_back(0x78);
    data.push_back(0x56);
    data.push_back(0x34);
    data.push_back(0x12);

    return data;
}

// ============================================================================
// Test Suite 1: Simple Label
// ============================================================================

TEST_CASE("Simple label - seek and read") {
    auto data = create_simple_label();
    SimpleLabel sl = parse_SimpleLabel(data);

    CHECK(sl.data_offset == 10);
    CHECK(sl.data_value == 0x42);
}

// ============================================================================
// Test Suite 2: Simple Alignment
// ============================================================================

TEST_CASE("Simple alignment - pad to 4-byte boundary") {
    auto data = create_simple_alignment();
    SimpleAlignment sa = parse_SimpleAlignment(data);

    CHECK(sa.header == 0x12);
    CHECK(sa.aligned_value == 0x12345678);
}

// ============================================================================
// Test Suite 3: Label and Alignment Combined
// ============================================================================

TEST_CASE("Label and alignment - combined") {
    auto data = create_label_and_alignment();
    LabelAndAlignment laa = parse_LabelAndAlignment(data);

    CHECK(laa.data_offset == 12);
    CHECK(laa.unaligned_byte == 0x42);
    CHECK(laa.aligned_value == 0x0807060504030201);
}

// ============================================================================
// Test Suite 4: Multiple Alignments
// ============================================================================

TEST_CASE("Multiple alignments - different boundaries") {
    auto data = create_multiple_alignments();
    MultipleAlignments ma = parse_MultipleAlignments(data);

    CHECK(ma.byte1 == 0x42);
    CHECK(ma.word == 0x1234);
    CHECK(ma.dword == 0x12345678);
    CHECK(ma.qword == 0x1234567890ABCDEF);
}

// ============================================================================
// Test Suite 5: Introspection Features
// ============================================================================

TEST_CASE("Label/alignment struct - introspection") {
    auto data = create_multiple_alignments();
    MultipleAlignments ma = parse_MultipleAlignments(data);

    StructView<MultipleAlignments> view(&ma);

    CHECK(view.field_count() == 4);
    CHECK(std::string(view.type_name()) == "MultipleAlignments");
}

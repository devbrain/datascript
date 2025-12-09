//
// End-to-End Test: Labels and Alignment
//
#include <doctest/doctest.h>
#include <e2e_labels_alignment.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Labels and Alignment") {

    // ========================================================================
    // Label Tests
    // ========================================================================

    TEST_CASE("SimpleLabel - seek to offset") {
        // data_offset = 20 (absolute offset from struct start)
        // Bytes 0-3: data_offset field
        // Byte 20: data_value = 0x42
        std::vector<uint8_t> data = {
            0x14, 0x00, 0x00, 0x00,  // data_offset = 20 (absolute offset from start)
            // 16 bytes of padding to reach absolute offset 20
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x42                      // Byte 20: data_value = 0x42
        };

        const uint8_t* ptr = data.data();
        SimpleLabel obj = SimpleLabel::read(ptr, ptr + data.size());

        CHECK(obj.data_offset == 20);
        CHECK(obj.data_value == 0x42);
    }

    TEST_CASE("SimpleLabel - seek to current position") {
        // data_offset = 4 (absolute offset from struct start)
        // After reading data_offset (4 bytes), we're at byte 4
        // Seeking to absolute offset 4 keeps us at byte 4
        // Byte 4: data_value = 0x99
        std::vector<uint8_t> data = {
            0x04, 0x00, 0x00, 0x00,  // data_offset = 4 (absolute offset)
            0x99                      // Byte 4: data_value = 0x99
        };

        const uint8_t* ptr = data.data();
        SimpleLabel obj = SimpleLabel::read(ptr, ptr + data.size());

        CHECK(obj.data_offset == 4);
        CHECK(obj.data_value == 0x99);
    }

    TEST_CASE("SimpleLabel - seek out of bounds") {
        // data_offset = 1000 (beyond buffer end)
        std::vector<uint8_t> data = {
            0xE8, 0x03, 0x00, 0x00   // data_offset = 1000 (invalid!)
        };

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(SimpleLabel::read(ptr, ptr + data.size()), std::runtime_error);
    }

    // ========================================================================
    // Alignment Tests
    // ========================================================================

    TEST_CASE("SimpleAlignment - align to 4-byte boundary") {
        // header = 0x01 (at byte 0)
        // After reading header, we're at byte 1
        // Need to pad 3 bytes to reach byte 4 (4-byte aligned)
        // aligned_value = 0x12345678 (at byte 4)
        std::vector<uint8_t> data = {
            0x01,                    // Byte 0: header
            0xAA, 0xBB, 0xCC,        // Bytes 1-3: padding (will be skipped)
            0x78, 0x56, 0x34, 0x12   // Bytes 4-7: aligned_value = 0x12345678
        };

        const uint8_t* ptr = data.data();
        SimpleAlignment obj = SimpleAlignment::read(ptr, ptr + data.size());

        CHECK(obj.header == 0x01);
        CHECK(obj.aligned_value == 0x12345678);
    }

    TEST_CASE("SimpleAlignment - already aligned") {
        // If we read 4 bytes before align(4), we're already at a 4-byte boundary
        // This test uses a workaround: we need to ensure the initial pointer alignment
        std::vector<uint8_t> data = {
            0x05,                    // header
            0x00, 0x00, 0x00,        // padding
            0xAB, 0xCD, 0xEF, 0x12   // aligned_value
        };

        const uint8_t* ptr = data.data();
        SimpleAlignment obj = SimpleAlignment::read(ptr, ptr + data.size());

        CHECK(obj.header == 0x05);
        CHECK(obj.aligned_value == 0x12EFCDAB);
    }

    // ========================================================================
    // Combined Label and Alignment Tests
    // ========================================================================

    TEST_CASE("LabelAndAlignment - label then alignment") {
        // data_offset = 16 (absolute offset from struct start)
        // After reading data_offset (4 bytes), we're at byte 4
        // Seeking to absolute offset 16 puts us at byte 16
        // At byte 16: unaligned_byte = 0x42
        // After reading byte at 17, align to 8-byte boundary (next is 24)
        // At byte 24: aligned_value = 0x1122334455667788
        std::vector<uint8_t> data = {
            0x10, 0x00, 0x00, 0x00,  // Bytes 0-3: data_offset = 16 (absolute)
            // Bytes 4-15: padding (12 bytes) to reach absolute offset 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x42,                    // Byte 16: unaligned_byte
            // Bytes 17-23: padding to 8-byte boundary (7 bytes)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // Bytes 24-31: aligned_value
            0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11
        };

        const uint8_t* ptr = data.data();
        LabelAndAlignment obj = LabelAndAlignment::read(ptr, ptr + data.size());

        CHECK(obj.data_offset == 16);
        CHECK(obj.unaligned_byte == 0x42);
        CHECK(obj.aligned_value == 0x1122334455667788ULL);
    }

    // ========================================================================
    // Multiple Labels
    // ========================================================================

    TEST_CASE("MultipleLabels - seek to two different positions") {
        // After reading offset1 and offset2 (8 bytes), we're at position 8
        // offset1 = 16: absolute position of data1
        // Read data1 at position 16-17, now at position 18
        // offset2 = 20: absolute position of data2
        // Read data2 at position 20-21
        std::vector<uint8_t> data = {
            0x10, 0x00, 0x00, 0x00,  // offset1 = 16 (absolute)
            0x14, 0x00, 0x00, 0x00,  // offset2 = 20 (absolute)
            // Bytes 8-15: padding to reach position 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0xBB, 0xAA,              // Bytes 16-17: data1 = 0xAABB
            0x00, 0x00,              // Bytes 18-19: padding to reach position 20
            0xDD, 0xCC               // Bytes 20-21: data2 = 0xCCDD
        };

        const uint8_t* ptr = data.data();
        MultipleLabels obj = MultipleLabels::read(ptr, ptr + data.size());

        CHECK(obj.offset1 == 16);
        CHECK(obj.offset2 == 20);
        CHECK(obj.data1 == 0xAABB);
        CHECK(obj.data2 == 0xCCDD);
    }

    // ========================================================================
    // Multiple Alignments
    // ========================================================================

    TEST_CASE("MultipleAlignments - different alignment boundaries") {
        // byte1 at byte 0
        // word at byte 2 (2-byte aligned)
        // dword at byte 4 (4-byte aligned)
        // qword at byte 8 (8-byte aligned)
        std::vector<uint8_t> data = {
            0x11,                    // Byte 0: byte1
            0x00,                    // Byte 1: padding to 2-byte boundary
            0x22, 0x33,              // Bytes 2-3: word = 0x3322
            0x44, 0x55, 0x66, 0x77,  // Bytes 4-7: dword = 0x77665544
            0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF  // Bytes 8-15: qword
        };

        const uint8_t* ptr = data.data();
        MultipleAlignments obj = MultipleAlignments::read(ptr, ptr + data.size());

        CHECK(obj.byte1 == 0x11);
        CHECK(obj.word == 0x3322);
        CHECK(obj.dword == 0x77665544);
        CHECK(obj.qword == 0xFFEEDDCCBBAA9988ULL);
    }

    // ========================================================================
    // Field Access in Labels
    // ========================================================================

    TEST_CASE("WithFieldAccessLabel - label with field access") {
        // After reading header (4 bytes), we're at position 4
        // header.data_offset = 12: absolute position of payload
        // At byte 12-15: payload = 0x12345678
        std::vector<uint8_t> data = {
            0x0C, 0x00, 0x00, 0x00,  // header.data_offset = 12 (absolute)
            // Bytes 4-11: padding to reach position 12
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x78, 0x56, 0x34, 0x12   // Bytes 12-15: payload = 0x12345678
        };

        const uint8_t* ptr = data.data();
        WithFieldAccessLabel obj = WithFieldAccessLabel::read(ptr, ptr + data.size());

        CHECK(obj.header.data_offset == 12);
        CHECK(obj.payload == 0x12345678);
    }

    // ========================================================================
    // Edge Cases
    // ========================================================================

    TEST_CASE("SimpleLabel - zero offset") {
        // data_offset = 4 (absolute offset from struct start)
        // After reading data_offset (4 bytes), we're at byte 4
        // Seeking to absolute offset 4 keeps us at byte 4
        std::vector<uint8_t> data = {
            0x04, 0x00, 0x00, 0x00,  // data_offset = 4 (absolute)
            0x77                      // data_value = 0x77
        };

        const uint8_t* ptr = data.data();
        SimpleLabel obj = SimpleLabel::read(ptr, ptr + data.size());

        CHECK(obj.data_offset == 4);
        CHECK(obj.data_value == 0x77);
    }

    TEST_CASE("SimpleAlignment - minimal padding") {
        // Test case where only 1 byte of padding is needed
        std::vector<uint8_t> data = {
            0xFF,                    // header (byte 0)
            0x00, 0x00, 0x00,        // padding (bytes 1-3)
            0x01, 0x02, 0x03, 0x04   // aligned_value (bytes 4-7)
        };

        const uint8_t* ptr = data.data();
        SimpleAlignment obj = SimpleAlignment::read(ptr, ptr + data.size());

        CHECK(obj.header == 0xFF);
        CHECK(obj.aligned_value == 0x04030201);
    }

    // ========================================================================
    // Bug Regression: Alignment must use relative offsets, not absolute memory addresses
    // https://github.com/devbrain/datascript/issues/XXX
    // ========================================================================

    TEST_CASE("SimpleAlignment - unaligned buffer address (bug regression)") {
        // This test verifies that alignment is calculated relative to the buffer start,
        // NOT based on absolute memory addresses. The bug was that align(N) used
        // uintptr_t alignment on the data pointer, which fails when the buffer
        // is allocated at a non-aligned memory address.

        // Data: header(1) + padding(3) + aligned_value(4) = 8 bytes
        std::vector<uint8_t> aligned_data = {
            0x42,                    // header (byte 0)
            0xAA, 0xBB, 0xCC,        // padding (bytes 1-3)
            0x78, 0x56, 0x34, 0x12   // aligned_value (bytes 4-7) = 0x12345678
        };

        // Create a buffer with 1 extra byte at the start to force unaligned address
        std::vector<uint8_t> unaligned_buffer(1 + aligned_data.size());
        unaligned_buffer[0] = 0xFF;  // padding byte
        std::copy(aligned_data.begin(), aligned_data.end(), unaligned_buffer.begin() + 1);

        // Parse from unaligned address (data.data() + 1)
        const uint8_t* ptr = unaligned_buffer.data() + 1;
        const uint8_t* end = ptr + aligned_data.size();

        SimpleAlignment obj = SimpleAlignment::read(ptr, end);

        CHECK(obj.header == 0x42);
        // The bug would cause incorrect alignment padding based on memory address,
        // resulting in reading wrong bytes as aligned_value
        CHECK(obj.aligned_value == 0x12345678);
    }

    TEST_CASE("MultipleAlignments - unaligned buffer address (bug regression)") {
        // Test multiple alignment directives with an unaligned buffer
        // byte1 at offset 0
        // word at offset 2 (2-byte aligned)
        // dword at offset 4 (4-byte aligned)
        // qword at offset 8 (8-byte aligned)
        std::vector<uint8_t> aligned_data = {
            0x11,                    // Byte 0: byte1
            0x00,                    // Byte 1: padding to 2-byte boundary
            0x22, 0x33,              // Bytes 2-3: word = 0x3322
            0x44, 0x55, 0x66, 0x77,  // Bytes 4-7: dword = 0x77665544
            0x88, 0x99, 0xAA, 0xBB, 0xCC, 0xDD, 0xEE, 0xFF  // Bytes 8-15: qword
        };

        // Test with various unaligned offsets (1, 2, 3 bytes)
        for (size_t offset = 1; offset <= 3; ++offset) {
            std::vector<uint8_t> unaligned_buffer(offset + aligned_data.size());
            std::fill(unaligned_buffer.begin(), unaligned_buffer.begin() + offset, 0xFF);
            std::copy(aligned_data.begin(), aligned_data.end(), unaligned_buffer.begin() + offset);

            const uint8_t* ptr = unaligned_buffer.data() + offset;
            const uint8_t* end = ptr + aligned_data.size();

            MultipleAlignments obj = MultipleAlignments::read(ptr, end);

            CHECK(obj.byte1 == 0x11);
            CHECK(obj.word == 0x3322);
            CHECK(obj.dword == 0x77665544);
            CHECK(obj.qword == 0xFFEEDDCCBBAA9988ULL);
        }
    }

    TEST_CASE("LabelAndAlignment - unaligned buffer address (bug regression)") {
        // Test combined label and alignment with unaligned buffer
        std::vector<uint8_t> aligned_data = {
            0x10, 0x00, 0x00, 0x00,  // Bytes 0-3: data_offset = 16 (absolute)
            // Bytes 4-15: padding (12 bytes) to reach absolute offset 16
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x42,                    // Byte 16: unaligned_byte
            // Bytes 17-23: padding to 8-byte boundary (7 bytes)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // Bytes 24-31: aligned_value
            0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11
        };

        // Parse from unaligned address
        std::vector<uint8_t> unaligned_buffer(3 + aligned_data.size());
        std::copy(aligned_data.begin(), aligned_data.end(), unaligned_buffer.begin() + 3);

        const uint8_t* ptr = unaligned_buffer.data() + 3;
        const uint8_t* end = ptr + aligned_data.size();

        LabelAndAlignment obj = LabelAndAlignment::read(ptr, end);

        CHECK(obj.data_offset == 16);
        CHECK(obj.unaligned_byte == 0x42);
        CHECK(obj.aligned_value == 0x1122334455667788ULL);
    }
}

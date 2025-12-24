//
// End-to-End Test: Bit Fields
// Tests fixed-width bit field extraction
//
#include <doctest/doctest.h>
#include <e2e_bitfields.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Bit Fields") {

    TEST_CASE("BitFieldBasic - 4-bit nibbles") {
        std::vector<uint8_t> data = { 0xAB };  // nibble1=0xA, nibble2=0xB

        const uint8_t* ptr = data.data();
        BitFieldBasic obj = BitFieldBasic::read(ptr, ptr + data.size());

        CHECK(obj.nibble1 == 0xB);  // Lower nibble first
        CHECK(obj.nibble2 == 0xA);  // Upper nibble second
    }

    TEST_CASE("BitFlags - individual bit flags") {
        std::vector<uint8_t> data = { 0x0D };  // 0000 1101

        const uint8_t* ptr = data.data();
        BitFlags obj = BitFlags::read(ptr, ptr + data.size());

        CHECK(obj.flag_a == 1);
        CHECK(obj.flag_b == 0);
        CHECK(obj.flag_c == 1);
        CHECK(obj.flag_d == 1);
        CHECK(obj.reserved == 0);
    }

    TEST_CASE("VersionBits - version encoding") {
        std::vector<uint8_t> data = { 0x2B };  // major=3 (bits 0-2), minor=5 (bits 3-7): 0b00101011 = 0x2B

        const uint8_t* ptr = data.data();
        VersionBits obj = VersionBits::read(ptr, ptr + data.size());

        CHECK(obj.major == 3);  // 0b011 = 3
        CHECK(obj.minor == 5);  // 0b00101 = 5
        CHECK(obj.get_version() == 0x65);  // (3 << 5) | 5 = 0b01100101 = 0x65
    }

    TEST_CASE("PackedData - complex bit packing") {
        std::vector<uint8_t> data = {
            0x2F,  // Byte 1: type=3 (bits 0-1), length=11 (bits 2-7)
            0x87   // Byte 2: compressed=1 (bit 0), priority=67 (bits 1-7)
        };

        const uint8_t* ptr = data.data();
        PackedData obj = PackedData::read(ptr, ptr + data.size());

        CHECK(obj.type == 3);      // 2 bits: 0b11 = 3
        CHECK(obj.length == 11);   // 6 bits: 0b001011 = 11
        CHECK(obj.compressed == 1); // 1 bit: 0b1 = 1
        CHECK(obj.priority == 67);  // 7 bits: 0b1000011 = 67

        CHECK(obj.get_type() == 3);
        CHECK(obj.is_compressed() == true);
    }
}

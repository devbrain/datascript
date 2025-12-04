//
// End-to-End Test: Primitive Types
// Tests all primitive integer types, strings, and booleans
//
#include <doctest/doctest.h>
#include <e2e_primitives.h>
#include <vector>
#include <cstring>

using namespace generated;

TEST_SUITE("E2E - Primitive Types") {

    TEST_CASE("AllPrimitives - all integer types") {
        std::vector<uint8_t> data = {
            // int8
            0xFF,  // -1
            // int16 (little-endian)
            0xFE, 0xFF,  // -2
            // int32 (little-endian)
            0xFD, 0xFF, 0xFF, 0xFF,  // -3
            // int64 (little-endian)
            0xFC, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,  // -4

            // uint8
            0x01,
            // uint16
            0x02, 0x00,
            // uint32
            0x03, 0x00, 0x00, 0x00,
            // uint64
            0x04, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

            // bool
            0x01  // true
        };

        const uint8_t* ptr = data.data();
        AllPrimitives obj = AllPrimitives::read(ptr, ptr + data.size());

        CHECK(obj.i8 == -1);
        CHECK(obj.i16 == -2);
        CHECK(obj.i32 == -3);
        CHECK(obj.i64 == -4);

        CHECK(obj.u8 == 1);
        CHECK(obj.u16 == 2);
        CHECK(obj.u32 == 3);
        CHECK(obj.u64 == 4);

        CHECK(obj.flag == true);
    }

    TEST_CASE("MinimalPrimitives - basic unsigned integers") {
        std::vector<uint8_t> data = {
            0x42,                    // byte = 0x42
            0x34, 0x12,              // word = 0x1234
            0x78, 0x56, 0x34, 0x12   // dword = 0x12345678
        };

        const uint8_t* ptr = data.data();
        MinimalPrimitives obj = MinimalPrimitives::read(ptr, ptr + data.size());

        CHECK(obj.byte_value == 0x42);
        CHECK(obj.word_value == 0x1234);
        CHECK(obj.dword_value == 0x12345678);
    }

    TEST_CASE("SignedIntegers - negative values") {
        std::vector<uint8_t> data = {
            0x80,                    // -128 (int8)
            0x00, 0x80,              // -32768 (int16)
            0x00, 0x00, 0x00, 0x80,  // -2147483648 (int32)
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x80  // int64 min
        };

        const uint8_t* ptr = data.data();
        SignedIntegers obj = SignedIntegers::read(ptr, ptr + data.size());

        CHECK(obj.negative_byte == -128);
        CHECK(obj.negative_word == -32768);
        CHECK(obj.negative_dword == -2147483648);
    }

    TEST_CASE("BooleanFlags - true and false") {
        std::vector<uint8_t> data = {
            0x01,  // true
            0x00,  // false
            0x01,  // true
            0x00   // false
        };

        const uint8_t* ptr = data.data();
        BooleanFlags obj = BooleanFlags::read(ptr, ptr + data.size());

        CHECK(obj.flag1 == true);
        CHECK(obj.flag2 == false);
        CHECK(obj.flag3 == true);
        CHECK(obj.flag4 == false);
    }
}

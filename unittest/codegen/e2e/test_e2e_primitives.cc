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

    TEST_CASE("Utf8Strings - UTF-8 encoded strings") {
        std::vector<uint8_t> data = {
            // ascii_text: "Hello"
            'H', 'e', 'l', 'l', 'o', 0x00,
            // unicode_text: "Test" (with null terminator)
            'T', 'e', 's', 't', 0x00,
            // empty_text: ""
            0x00
        };

        const uint8_t* ptr = data.data();
        Utf8Strings obj = Utf8Strings::read(ptr, ptr + data.size());

        CHECK(obj.ascii_text == "Hello");
        CHECK(obj.unicode_text == "Test");
        CHECK(obj.empty_text == "");
    }

    TEST_CASE("Utf16LeStrings - UTF-16 little-endian strings") {
        std::vector<uint8_t> data = {
            // ascii_text: "Hi" (UTF-16 LE)
            'H', 0x00, 'i', 0x00, 0x00, 0x00,  // null terminator: 0x0000
            // unicode_text: "AB" (UTF-16 LE)
            'A', 0x00, 'B', 0x00, 0x00, 0x00,
            // empty_text: ""
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Utf16LeStrings obj = Utf16LeStrings::read(ptr, ptr + data.size());

        CHECK(obj.ascii_text == u"Hi");
        CHECK(obj.unicode_text == u"AB");
        CHECK(obj.empty_text == u"");
    }

    TEST_CASE("Utf16BeStrings - UTF-16 big-endian strings") {
        std::vector<uint8_t> data = {
            // ascii_text: "Hi" (UTF-16 BE)
            0x00, 'H', 0x00, 'i', 0x00, 0x00,  // null terminator: 0x0000
            // unicode_text: "AB" (UTF-16 BE)
            0x00, 'A', 0x00, 'B', 0x00, 0x00,
            // empty_text: ""
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Utf16BeStrings obj = Utf16BeStrings::read(ptr, ptr + data.size());

        CHECK(obj.ascii_text == u"Hi");
        CHECK(obj.unicode_text == u"AB");
        CHECK(obj.empty_text == u"");
    }

    TEST_CASE("Utf32LeStrings - UTF-32 little-endian strings") {
        std::vector<uint8_t> data = {
            // ascii_text: "Hi" (UTF-32 LE)
            'H', 0x00, 0x00, 0x00, 'i', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,  // null terminator
            // unicode_text: "AB" (UTF-32 LE)
            'A', 0x00, 0x00, 0x00, 'B', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            // empty_text: ""
            0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Utf32LeStrings obj = Utf32LeStrings::read(ptr, ptr + data.size());

        CHECK(obj.ascii_text == U"Hi");
        CHECK(obj.unicode_text == U"AB");
        CHECK(obj.empty_text == U"");
    }

    TEST_CASE("Utf32BeStrings - UTF-32 big-endian strings") {
        std::vector<uint8_t> data = {
            // ascii_text: "Hi" (UTF-32 BE)
            0x00, 0x00, 0x00, 'H', 0x00, 0x00, 0x00, 'i', 0x00, 0x00, 0x00, 0x00,  // null terminator
            // unicode_text: "AB" (UTF-32 BE)
            0x00, 0x00, 0x00, 'A', 0x00, 0x00, 0x00, 'B', 0x00, 0x00, 0x00, 0x00,
            // empty_text: ""
            0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Utf32BeStrings obj = Utf32BeStrings::read(ptr, ptr + data.size());

        CHECK(obj.ascii_text == U"Hi");
        CHECK(obj.unicode_text == U"AB");
        CHECK(obj.empty_text == U"");
    }

    TEST_CASE("AllUnicodeStrings - comprehensive test with all string types") {
        std::vector<uint8_t> data = {
            // utf8_name: "Test"
            'T', 'e', 's', 't', 0x00,

            // utf16_le_text: "AB" (UTF-16 LE)
            'A', 0x00, 'B', 0x00, 0x00, 0x00,

            // utf16_be_text: "CD" (UTF-16 BE)
            0x00, 'C', 0x00, 'D', 0x00, 0x00,

            // utf16_default: "EF" (UTF-16 LE - default)
            'E', 0x00, 'F', 0x00, 0x00, 0x00,

            // utf32_le_text: "GH" (UTF-32 LE)
            'G', 0x00, 0x00, 0x00, 'H', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,

            // utf32_be_text: "IJ" (UTF-32 BE)
            0x00, 0x00, 0x00, 'I', 0x00, 0x00, 0x00, 'J', 0x00, 0x00, 0x00, 0x00,

            // utf32_default: "KL" (UTF-32 LE - default)
            'K', 0x00, 0x00, 0x00, 'L', 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        AllUnicodeStrings obj = AllUnicodeStrings::read(ptr, ptr + data.size());

        CHECK(obj.utf8_name == "Test");
        CHECK(obj.utf16_le_text == u"AB");
        CHECK(obj.utf16_be_text == u"CD");
        CHECK(obj.utf16_default == u"EF");
        CHECK(obj.utf32_le_text == U"GH");
        CHECK(obj.utf32_be_text == U"IJ");
        CHECK(obj.utf32_default == U"KL");
    }
}

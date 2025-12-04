//
// End-to-End Test: Endianness
//
#include <doctest/doctest.h>
#include <e2e_endianness.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Endianness") {

    TEST_CASE("LittleEndianData - default little endian") {
        std::vector<uint8_t> data = {
            0x34, 0x12,                          // word = 0x1234
            0x78, 0x56, 0x34, 0x12,              // dword = 0x12345678
            0xEF, 0xCD, 0xAB, 0x90, 0x78, 0x56, 0x34, 0x12  // qword
        };

        const uint8_t* ptr = data.data();
        LittleEndianData obj = LittleEndianData::read(ptr, ptr + data.size());

        CHECK(obj.word == 0x1234);
        CHECK(obj.dword == 0x12345678);
        CHECK(obj.qword == 0x1234567890ABCDEF);
    }

    TEST_CASE("BigEndianData - explicit big endian") {
        std::vector<uint8_t> data = {
            0x12, 0x34,                          // word = 0x1234 (BE)
            0x12, 0x34, 0x56, 0x78,              // dword = 0x12345678 (BE)
            0x12, 0x34, 0x56, 0x78, 0x90, 0xAB, 0xCD, 0xEF  // qword (BE)
        };

        const uint8_t* ptr = data.data();
        BigEndianData obj = BigEndianData::read(ptr, ptr + data.size());

        CHECK(obj.word == 0x1234);
        CHECK(obj.dword == 0x12345678);
        CHECK(obj.qword == 0x1234567890ABCDEF);
    }

    TEST_CASE("MixedEndianData - mixed endianness") {
        std::vector<uint8_t> data = {
            0x34, 0x12,       // le_word (LE) = 0x1234
            0x12, 0x34,       // be_word (BE) = 0x1234
            0x78, 0x56, 0x34, 0x12,       // le_dword (LE)
            0x12, 0x34, 0x56, 0x78        // be_dword (BE)
        };

        const uint8_t* ptr = data.data();
        MixedEndianData obj = MixedEndianData::read(ptr, ptr + data.size());

        CHECK(obj.le_word == 0x1234);
        CHECK(obj.be_word == 0x1234);
        CHECK(obj.le_dword == 0x12345678);
        CHECK(obj.be_dword == 0x12345678);
    }
}

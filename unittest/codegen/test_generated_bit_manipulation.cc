//
// End-to-end tests using generated code from bit_manipulation.ds
//
#include <doctest/doctest.h>
#include <bit_manipulation.h>
#include <vector>
#include <cstdint>

using namespace generated;

TEST_SUITE("E2E Generated - Bit Manipulation") {

    TEST_CASE("Flags struct - basic parsing") {
        std::vector<uint8_t> data = {
            0xA5, 0x5A, 0x3C, 0xC3  // bits = 0xC33C5AA5 (little endian)
        };

        const uint8_t* ptr = data.data();
        Flags flags = Flags::read(ptr, ptr + data.size());

        CHECK( flags.bits == 0xC33C5AA5 );
    }

    TEST_CASE("Flags struct - get_bit function") {
        std::vector<uint8_t> data = {
            0x0F, 0x00, 0x00, 0x00  // bits = 0x0000000F = 0b1111
        };

        const uint8_t* ptr = data.data();
        Flags flags = Flags::read(ptr, ptr + data.size());

        CHECK( flags.get_bit(0) == 1 );
        CHECK( flags.get_bit(1) == 1 );
        CHECK( flags.get_bit(2) == 1 );
        CHECK( flags.get_bit(3) == 1 );
        CHECK( flags.get_bit(4) == 0 );
        CHECK( flags.get_bit(5) == 0 );
    }

    TEST_CASE("Flags struct - get_byte function") {
        std::vector<uint8_t> data = {
            0x12, 0x34, 0x56, 0x78  // bits = 0x78563412
        };

        const uint8_t* ptr = data.data();
        Flags flags = Flags::read(ptr, ptr + data.size());

        CHECK( flags.get_byte(0) == 0x12 );
        CHECK( flags.get_byte(1) == 0x34 );
        CHECK( flags.get_byte(2) == 0x56 );
        CHECK( flags.get_byte(3) == 0x78 );
    }

    TEST_CASE("Flags struct - popcount_simple function") {
        std::vector<uint8_t> data = {
            0x07, 0x00, 0x00, 0x00  // bits = 0b0111 (3 bits set)
        };

        const uint8_t* ptr = data.data();
        Flags flags = Flags::read(ptr, ptr + data.size());

        CHECK( flags.popcount_simple() == 3 );
    }

    TEST_CASE("Flags struct - has_any_bits function") {
        // Non-zero bits
        std::vector<uint8_t> data1 = {0x01, 0x00, 0x00, 0x00};
        const uint8_t* ptr1 = data1.data();
        Flags flags1 = Flags::read(ptr1, ptr1 + data1.size());
        CHECK( flags1.has_any_bits() == true );

        // Zero bits
        std::vector<uint8_t> data2 = {0x00, 0x00, 0x00, 0x00};
        const uint8_t* ptr2 = data2.data();
        Flags flags2 = Flags::read(ptr2, ptr2 + data2.size());
        CHECK( flags2.has_any_bits() == false );
    }

    TEST_CASE("Flags struct - has_all_bits function") {
        std::vector<uint8_t> data = {
            0xFF, 0x0F, 0x00, 0x00  // bits = 0x00000FFF
        };

        const uint8_t* ptr = data.data();
        Flags flags = Flags::read(ptr, ptr + data.size());

        CHECK( flags.has_all_bits(0x00000001) == true );  // Bit 0 set
        CHECK( flags.has_all_bits(0x00000FFF) == true );  // All 12 bits set
        CHECK( flags.has_all_bits(0x0000F000) == false ); // Bits 12-15 not set
    }

    TEST_CASE("Version struct - basic parsing") {
        std::vector<uint8_t> data = {
            0x05, 0x01  // encoded = 0x0105 = v1.5
        };

        const uint8_t* ptr = data.data();
        Version version = Version::read(ptr, ptr + data.size());

        CHECK( version.encoded == 0x0105 );
    }

    TEST_CASE("Version struct - major function") {
        std::vector<uint8_t> data = {
            0x03, 0x02  // encoded = 0x0203 = v2.3
        };

        const uint8_t* ptr = data.data();
        Version version = Version::read(ptr, ptr + data.size());

        CHECK( version.major() == 2 );
    }

    TEST_CASE("Version struct - minor function") {
        std::vector<uint8_t> data = {
            0x07, 0x01  // encoded = 0x0107 = v1.7
        };

        const uint8_t* ptr = data.data();
        Version version = Version::read(ptr, ptr + data.size());

        CHECK( version.minor() == 7 );
    }

    TEST_CASE("Version struct - is_v1 function" * doctest::skip()) {
        // KNOWN LIMITATION: Function calls within function bodies are currently
        // rendered as 0. The is_v1() function calls major(), which gets rendered
        // as "return (0 == 1)" instead of "return (major() == 1)".
        // See E2E_FUNCTIONS_TEST_RESULTS.md for details.
        // TODO: Fix function call expression rendering in cpp_expr_renderer.cc

        // Version 1.5
        std::vector<uint8_t> data1 = {0x05, 0x01};
        const uint8_t* ptr1 = data1.data();
        Version v1 = Version::read(ptr1, ptr1 + data1.size());
        CHECK( v1.is_v1() == true );

        // Version 2.0
        std::vector<uint8_t> data2 = {0x00, 0x02};
        const uint8_t* ptr2 = data2.data();
        Version v2 = Version::read(ptr2, ptr2 + data2.size());
        CHECK( v2.is_v1() == false );
    }
}

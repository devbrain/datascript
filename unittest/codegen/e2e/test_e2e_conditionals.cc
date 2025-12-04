//
// End-to-End Test: Conditional Fields
//
#include <doctest/doctest.h>
#include <e2e_conditionals.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Conditionals") {

    TEST_CASE("SimpleConditional - field present") {
        // flags = 0x01 (bit 0 set), optional_value = 0x1234
        std::vector<uint8_t> data = {
            0x01,        // flags with bit 0 set
            0x34, 0x12   // optional_value = 0x1234
        };

        const uint8_t* ptr = data.data();
        SimpleConditional obj = SimpleConditional::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x01);
        CHECK(obj.optional_value == 0x1234);
    }

    TEST_CASE("SimpleConditional - field absent") {
        // flags = 0x00 (bit 0 clear), no optional_value
        std::vector<uint8_t> data = {
            0x00         // flags with bit 0 clear
        };

        const uint8_t* ptr = data.data();
        SimpleConditional obj = SimpleConditional::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x00);
        // optional_value is not read, will have uninitialized value
    }

    TEST_CASE("MultipleConditionals - all flags set") {
        // flags = 0x07 (all 3 bits set)
        std::vector<uint8_t> data = {
            0x07,                       // flags = 0b00000111
            0x11, 0x00,                 // field1 (bit 0 set)
            0x22, 0x00, 0x00, 0x00,     // field2 (bit 1 set)
            0x33, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00  // field3 (bit 2 set)
        };

        const uint8_t* ptr = data.data();
        MultipleConditionals obj = MultipleConditionals::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x07);
        CHECK(obj.field1 == 0x0011);
        CHECK(obj.field2 == 0x00000022);
        CHECK(obj.field3 == 0x0000000000000033);
    }

    TEST_CASE("MultipleConditionals - only field2 present") {
        // flags = 0x02 (only bit 1 set)
        std::vector<uint8_t> data = {
            0x02,                       // flags = 0b00000010
            0x99, 0x88, 0x77, 0x66      // field2
        };

        const uint8_t* ptr = data.data();
        MultipleConditionals obj = MultipleConditionals::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x02);
        CHECK(obj.field2 == 0x66778899);
    }

    TEST_CASE("ValueConditional - legacy version") {
        // version = 1, legacy_field present
        std::vector<uint8_t> data = {
            0x01,        // version = 1
            0xAA, 0xBB   // legacy_field
        };

        const uint8_t* ptr = data.data();
        ValueConditional obj = ValueConditional::read(ptr, ptr + data.size());

        CHECK(obj.version == 1);
        CHECK(obj.legacy_field == 0xBBAA);
    }

    TEST_CASE("ValueConditional - modern version") {
        // version = 2, modern_field present
        std::vector<uint8_t> data = {
            0x02,                       // version = 2
            0x11, 0x22, 0x33, 0x44      // modern_field
        };

        const uint8_t* ptr = data.data();
        ValueConditional obj = ValueConditional::read(ptr, ptr + data.size());

        CHECK(obj.version == 2);
        CHECK(obj.modern_field == 0x44332211);
    }

    TEST_CASE("ComplexConditional - AND condition satisfied") {
        // flags = 0x01, mode = 1 → data1 present
        std::vector<uint8_t> data = {
            0x01,        // flags (bit 0 set)
            0x01,        // mode = 1
            0xAA, 0xBB   // data1
        };

        const uint8_t* ptr = data.data();
        ComplexConditional obj = ComplexConditional::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x01);
        CHECK(obj.mode == 0x01);
        CHECK(obj.data1 == 0xBBAA);
    }

    TEST_CASE("ComplexConditional - OR condition satisfied") {
        // flags = 0x02, mode = 0 → data2 present (flags & 0x02 != 0)
        std::vector<uint8_t> data = {
            0x02,                       // flags (bit 1 set)
            0x00,                       // mode = 0
            0x11, 0x22, 0x33, 0x44      // data2
        };

        const uint8_t* ptr = data.data();
        ComplexConditional obj = ComplexConditional::read(ptr, ptr + data.size());

        CHECK(obj.flags == 0x02);
        CHECK(obj.mode == 0x00);
        CHECK(obj.data2 == 0x44332211);
    }

    TEST_CASE("ConditionalString - string present") {
        // has_name = 1, name = "test"
        std::vector<uint8_t> data = {
            0x01,                           // has_name = 1
            't', 'e', 's', 't', '\0'        // name = "test"
        };

        const uint8_t* ptr = data.data();
        ConditionalString obj = ConditionalString::read(ptr, ptr + data.size());

        CHECK(obj.has_name == 1);
        CHECK(obj.name == "test");
    }

    TEST_CASE("ConditionalString - string absent") {
        // has_name = 0, no name field
        std::vector<uint8_t> data = {
            0x00         // has_name = 0
        };

        const uint8_t* ptr = data.data();
        ConditionalString obj = ConditionalString::read(ptr, ptr + data.size());

        CHECK(obj.has_name == 0);
        CHECK(obj.name.empty());  // Empty string (default constructed)
    }
}

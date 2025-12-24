//
// End-to-End Test: String Types
// Tests null-terminated UTF-8 string reading
//
#include <doctest/doctest.h>
#include <e2e_strings.h>
#include <vector>
#include <cstring>

using namespace generated;

TEST_SUITE("E2E - String Types") {

    TEST_CASE("SimpleString - basic string reading") {
        // "Hello" in UTF-8 with null terminator
        std::vector<uint8_t> data = {
            'H', 'e', 'l', 'l', 'o', 0x00
        };

        const uint8_t* ptr = data.data();
        SimpleString obj = SimpleString::read(ptr, ptr + data.size());

        CHECK(obj.text == "Hello");
        CHECK(ptr == data.data() + data.size());  // Pointer should be at end
    }

    TEST_CASE("MultipleStrings - sequential strings") {
        // "foo", "bar", "baz" with null terminators
        std::vector<uint8_t> data = {
            'f', 'o', 'o', 0x00,
            'b', 'a', 'r', 0x00,
            'b', 'a', 'z', 0x00
        };

        const uint8_t* ptr = data.data();
        MultipleStrings obj = MultipleStrings::read(ptr, ptr + data.size());

        CHECK(obj.first == "foo");
        CHECK(obj.second == "bar");
        CHECK(obj.third == "baz");
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("MixedData - strings mixed with primitives") {
        // count=42, name="test", value=1000, description="desc"
        std::vector<uint8_t> data = {
            0x2A, 0x00,  // uint16: 42
            't', 'e', 's', 't', 0x00,  // string: "test"
            0xE8, 0x03, 0x00, 0x00,  // uint32: 1000
            'd', 'e', 's', 'c', 0x00  // string: "desc"
        };

        const uint8_t* ptr = data.data();
        MixedData obj = MixedData::read(ptr, ptr + data.size());

        CHECK(obj.count == 42);
        CHECK(obj.name == "test");
        CHECK(obj.value == 1000);
        CHECK(obj.description == "desc");
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("EmptyString - zero-length string") {
        // Empty string (just null terminator) followed by marker byte
        std::vector<uint8_t> data = {
            0x00,  // Empty string (just null terminator)
            0xFF   // marker byte
        };

        const uint8_t* ptr = data.data();
        EmptyString obj = EmptyString::read(ptr, ptr + data.size());

        CHECK(obj.empty == "");
        CHECK(obj.marker == 0xFF);
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("StringArray - variable array of strings") {
        // count=3, items=["one", "two", "three"]
        std::vector<uint8_t> data = {
            0x03,  // count
            'o', 'n', 'e', 0x00,
            't', 'w', 'o', 0x00,
            't', 'h', 'r', 'e', 'e', 0x00
        };

        const uint8_t* ptr = data.data();
        StringArray obj = StringArray::read(ptr, ptr + data.size());

        CHECK(obj.count == 3);
        REQUIRE(obj.items.size() == 3);
        CHECK(obj.items[0] == "one");
        CHECK(obj.items[1] == "two");
        CHECK(obj.items[2] == "three");
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("SimpleString - UTF-8 multibyte characters") {
        // "café" in UTF-8 (c=0x63, a=0x61, f=0x66, é=0xC3 0xA9)
        std::vector<uint8_t> data = {
            0x63, 0x61, 0x66, 0xC3, 0xA9, 0x00
        };

        const uint8_t* ptr = data.data();
        SimpleString obj = SimpleString::read(ptr, ptr + data.size());

        CHECK(obj.text == "café");
        CHECK(ptr == data.data() + data.size());
    }

    TEST_CASE("SimpleString - missing null terminator throws exception") {
        // String without null terminator
        std::vector<uint8_t> data = {
            'H', 'e', 'l', 'l', 'o'
        };

        const uint8_t* ptr = data.data();

        CHECK_THROWS_WITH_AS(
            SimpleString::read(ptr, ptr + data.size()),
            "String not null-terminated before end of buffer",
            std::runtime_error
        );
    }

    TEST_CASE("MultipleStrings - special characters") {
        // Strings with spaces and special chars
        std::vector<uint8_t> data = {
            'H', 'e', 'l', 'l', 'o', ' ', 'W', 'o', 'r', 'l', 'd', '!', 0x00,
            '1', '2', '3', '.', '4', '5', 0x00,
            '@', '#', '$', '%', 0x00
        };

        const uint8_t* ptr = data.data();
        MultipleStrings obj = MultipleStrings::read(ptr, ptr + data.size());

        CHECK(obj.first == "Hello World!");
        CHECK(obj.second == "123.45");
        CHECK(obj.third == "@#$%");
    }
}

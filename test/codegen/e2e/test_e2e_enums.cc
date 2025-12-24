//
// End-to-End Test: Enumerations
// Tests enum definitions and usage
//
#include <doctest/doctest.h>
#include <e2e_enums.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Enumerations") {

    TEST_CASE("EnumTest - enum field access") {
        std::vector<uint8_t> data = {
            0x01,        // color = GREEN
            0x06,        // protocol = TCP
            0x2C, 0x01   // file_type = IMAGE (300)
        };

        const uint8_t* ptr = data.data();
        EnumTest obj = EnumTest::read(ptr, ptr + data.size());

        CHECK(obj.color == static_cast<Color>(1));  // GREEN
        CHECK(obj.protocol == static_cast<ProtocolType>(6));  // TCP
        CHECK(obj.file_type == static_cast<FileType>(300));  // IMAGE

        CHECK(obj.is_primary_color() == true);
        CHECK(obj.is_tcp() == true);
    }

    TEST_CASE("EnumTest - RED color") {
        std::vector<uint8_t> data = {
            0x00,        // color = RED
            0x11,        // protocol = UDP
            0x64, 0x00   // file_type = TEXT (100)
        };

        const uint8_t* ptr = data.data();
        EnumTest obj = EnumTest::read(ptr, ptr + data.size());

        CHECK(obj.color == static_cast<Color>(0));  // RED
        CHECK(obj.is_primary_color() == true);
        CHECK(obj.is_tcp() == false);
    }

    TEST_CASE("FilePermissions - flag enum") {
        std::vector<uint8_t> data1 = { 0x03 };  // READ | WRITE
        const uint8_t* ptr1 = data1.data();
        FilePermissions obj1 = FilePermissions::read(ptr1, ptr1 + data1.size());

        CHECK(obj1.mode == static_cast<FileMode>(3));
        CHECK(obj1.can_read() == true);
        CHECK(obj1.can_write() == true);

        std::vector<uint8_t> data2 = { 0x04 };  // EXECUTE only
        const uint8_t* ptr2 = data2.data();
        FilePermissions obj2 = FilePermissions::read(ptr2, ptr2 + data2.size());

        CHECK(obj2.mode == static_cast<FileMode>(4));
        CHECK(obj2.can_read() == false);
        CHECK(obj2.can_write() == false);
    }
}

//
// End-to-end tests using generated code from simple_types.ds
//
#include <doctest/doctest.h>
#include <simple_types.h>
#include <vector>
#include <cstdint>

using namespace generated;

TEST_SUITE("E2E Generated - Simple Types") {

    TEST_CASE("Config struct - basic parsing") {
        std::vector<uint8_t> data = {
            0x03,        // flags = 3 (debug | verbose)
            0x64, 0x00   // value = 100 (little endian)
        };

        const uint8_t* ptr = data.data();
        const uint8_t* end = ptr + data.size();

        Config config = Config::read(ptr, end);

        CHECK( config.flags == 0x03 );
        CHECK( config.value == 100 );
    }

    TEST_CASE("Config struct - is_debug function") {
        std::vector<uint8_t> data = {
            0x01,        // flags = 1 (debug only)
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Config config = Config::read(ptr, ptr + data.size());

        CHECK( config.is_debug() == true );
        CHECK( config.is_verbose() == false );
    }

    TEST_CASE("Config struct - is_verbose function") {
        std::vector<uint8_t> data = {
            0x02,        // flags = 2 (verbose only)
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Config config = Config::read(ptr, ptr + data.size());

        CHECK( config.is_debug() == false );
        CHECK( config.is_verbose() == true );
    }

    TEST_CASE("Config struct - both flags set") {
        std::vector<uint8_t> data = {
            0x03,        // flags = 3 (debug | verbose)
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Config config = Config::read(ptr, ptr + data.size());

        CHECK( config.is_debug() == true );
        CHECK( config.is_verbose() == true );
    }

    TEST_CASE("Config struct - flag_count function") {
        std::vector<uint8_t> data = {
            0x05,        // flags = 5
            0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        Config config = Config::read(ptr, ptr + data.size());

        CHECK( config.flag_count() == 5 );
    }

    TEST_CASE("Config struct - exceeds function with parameter") {
        std::vector<uint8_t> data = {
            0x00,        // flags
            0xC8, 0x00   // value = 200
        };

        const uint8_t* ptr = data.data();
        Config config = Config::read(ptr, ptr + data.size());

        CHECK( config.exceeds(100) == true );
        CHECK( config.exceeds(200) == false );
        CHECK( config.exceeds(250) == false );
    }

    TEST_CASE("Point struct - basic parsing") {
        std::vector<uint8_t> data = {
            0x0A, 0x00,  // x = 10
            0x14, 0x00   // y = 20
        };

        const uint8_t* ptr = data.data();
        Point point = Point::read(ptr, ptr + data.size());

        CHECK( point.x == 10 );
        CHECK( point.y == 20 );
    }

    TEST_CASE("Point struct - getter functions") {
        std::vector<uint8_t> data = {
            0x0F, 0x00,  // x = 15
            0x1E, 0x00   // y = 30
        };

        const uint8_t* ptr = data.data();
        Point point = Point::read(ptr, ptr + data.size());

        CHECK( point.get_x() == 15 );
        CHECK( point.get_y() == 30 );
    }

    TEST_CASE("Point struct - is_origin function") {
        // Origin point
        std::vector<uint8_t> origin = {0x00, 0x00, 0x00, 0x00};
        const uint8_t* ptr1 = origin.data();
        Point p1 = Point::read(ptr1, ptr1 + origin.size());
        CHECK( p1.is_origin() == true );

        // Non-origin point
        std::vector<uint8_t> non_origin = {0x01, 0x00, 0x00, 0x00};
        const uint8_t* ptr2 = non_origin.data();
        Point p2 = Point::read(ptr2, ptr2 + non_origin.size());
        CHECK( p2.is_origin() == false );
    }

    TEST_CASE("Point struct - x_greater_than_y function") {
        // x > y
        std::vector<uint8_t> data1 = {0x0A, 0x00, 0x05, 0x00};  // 10, 5
        const uint8_t* ptr1 = data1.data();
        Point p1 = Point::read(ptr1, ptr1 + data1.size());
        CHECK( p1.x_greater_than_y() == true );

        // x < y
        std::vector<uint8_t> data2 = {0x05, 0x00, 0x0A, 0x00};  // 5, 10
        const uint8_t* ptr2 = data2.data();
        Point p2 = Point::read(ptr2, ptr2 + data2.size());
        CHECK( p2.x_greater_than_y() == false );

        // x == y
        std::vector<uint8_t> data3 = {0x0A, 0x00, 0x0A, 0x00};  // 10, 10
        const uint8_t* ptr3 = data3.data();
        Point p3 = Point::read(ptr3, ptr3 + data3.size());
        CHECK( p3.x_greater_than_y() == false );
    }
}

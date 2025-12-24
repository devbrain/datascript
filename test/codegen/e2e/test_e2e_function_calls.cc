//
// End-to-End Test: Function Calls in Expressions
// Tests recursive function calls within function bodies
//
#include <doctest/doctest.h>
#include <e2e_function_calls.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Function Calls") {

    TEST_CASE("Header - function calling another function") {
        // total_length=100, header_size=20
        std::vector<uint8_t> data = {
            0x64, 0x00,  // total_length = 100
            0x14, 0x00   // header_size = 20
        };

        const uint8_t* ptr = data.data();
        Header obj = Header::read(ptr, ptr + data.size());

        CHECK(obj.total_length == 100);
        CHECK(obj.header_size == 20);

        // get_header_length() should return header_size
        CHECK(obj.get_header_length() == 20);

        // get_payload_length() calls get_header_length()
        // Should return: total_length - get_header_length() = 100 - 20 = 80
        CHECK(obj.get_payload_length() == 80);

        // double_payload() calls get_payload_length() * 2
        // Should return: 80 * 2 = 160
        CHECK(obj.double_payload() == 160);

        // add_value(10) calls get_payload_length() + 10
        // Should return: 80 + 10 = 90
        CHECK(obj.add_value(10) == 90);
    }

    TEST_CASE("Calculator - function calls with multiple parameters") {
        // a=5, b=10, c=15
        std::vector<uint8_t> data = {
            0x05, 0x00, 0x00, 0x00,  // a = 5
            0x0A, 0x00, 0x00, 0x00,  // b = 10
            0x0F, 0x00, 0x00, 0x00   // c = 15
        };

        const uint8_t* ptr = data.data();
        Calculator obj = Calculator::read(ptr, ptr + data.size());

        CHECK(obj.a == 5);
        CHECK(obj.b == 10);
        CHECK(obj.c == 15);

        // sum() = a + b + c = 5 + 10 + 15 = 30
        CHECK(obj.sum() == 30);

        // multiply(5, 10) = 50
        CHECK(obj.multiply(5, 10) == 50);

        // product() calls multiply(a, b) = multiply(5, 10) = 50
        CHECK(obj.product() == 50);

        // chain_calls() calls multiply(sum(), 2) = multiply(30, 2) = 60
        CHECK(obj.chain_calls() == 60);
    }

    TEST_CASE("Header - zero values") {
        std::vector<uint8_t> data = {
            0x00, 0x00,  // total_length = 0
            0x00, 0x00   // header_size = 0
        };

        const uint8_t* ptr = data.data();
        Header obj = Header::read(ptr, ptr + data.size());

        CHECK(obj.get_header_length() == 0);
        CHECK(obj.get_payload_length() == 0);
        CHECK(obj.double_payload() == 0);
        CHECK(obj.add_value(100) == 100);
    }
}

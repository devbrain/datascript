//
// End-to-End Test: Subtypes in Structs
//
#include <doctest/doctest.h>
#include <e2e_subtypes.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Subtypes") {

    TEST_CASE("User - valid UserID") {
        // user_id = 1234 (valid: > 0)
        std::vector<uint8_t> data = {
            0xD2, 0x04,              // id = 1234
            0x01, 0x00, 0x00, 0x00   // flags = 1
        };

        const uint8_t* ptr = data.data();
        User obj = User::read(ptr, ptr + data.size());

        CHECK(obj.id == 1234);
        CHECK(obj.flags == 1);
    }

    TEST_CASE("User - invalid UserID (zero)") {
        // user_id = 0 (invalid: must be > 0)
        std::vector<uint8_t> data = {
            0x00, 0x00,              // id = 0 (invalid!)
            0x01, 0x00, 0x00, 0x00   // flags = 1
        };

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(User::read(ptr, ptr + data.size()), std::runtime_error);
    }

    TEST_CASE("Connection - all valid subtypes") {
        // user = 5000, port = 8080, reliability = 99
        std::vector<uint8_t> data = {
            0x88, 0x13,  // user = 5000 (valid: > 0)
            0x90, 0x1F,  // port = 8080 (valid: > 1024)
            0x63         // reliability = 99 (valid: <= 100)
        };

        const uint8_t* ptr = data.data();
        Connection obj = Connection::read(ptr, ptr + data.size());

        CHECK(obj.user == 5000);
        CHECK(obj.port == 8080);
        CHECK(obj.reliability == 99);
    }

    TEST_CASE("Connection - invalid port (too low)") {
        // port = 80 (invalid: must be > 1024)
        std::vector<uint8_t> data = {
            0x01, 0x00,  // user = 1 (valid)
            0x50, 0x00,  // port = 80 (invalid!)
            0x32         // reliability = 50 (valid)
        };

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(Connection::read(ptr, ptr + data.size()), std::runtime_error);
    }

    TEST_CASE("Connection - invalid percentage (too high)") {
        // reliability = 150 (invalid: must be <= 100)
        std::vector<uint8_t> data = {
            0x01, 0x00,  // user = 1 (valid)
            0xFF, 0x04,  // port = 1279 (valid)
            0x96         // reliability = 150 (invalid!)
        };

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(Connection::read(ptr, ptr + data.size()), std::runtime_error);
    }

    TEST_CASE("Server - nested subtypes") {
        // listen_port = 3000, cpu_usage = 75, admin.id = 1, admin.flags = 42
        std::vector<uint8_t> data = {
            0xB8, 0x0B,              // listen_port = 3000 (valid)
            0x4B,                    // cpu_usage = 75 (valid)
            0x01, 0x00,              // admin.id = 1 (valid)
            0x2A, 0x00, 0x00, 0x00   // admin.flags = 42
        };

        const uint8_t* ptr = data.data();
        Server obj = Server::read(ptr, ptr + data.size());

        CHECK(obj.listen_port == 3000);
        CHECK(obj.cpu_usage == 75);
        CHECK(obj.admin.id == 1);
        CHECK(obj.admin.flags == 42);
    }

    TEST_CASE("Server - nested invalid UserID") {
        // admin.id = 0 (invalid: must be > 0)
        std::vector<uint8_t> data = {
            0xB8, 0x0B,              // listen_port = 3000 (valid)
            0x4B,                    // cpu_usage = 75 (valid)
            0x00, 0x00,              // admin.id = 0 (invalid!)
            0x2A, 0x00, 0x00, 0x00   // admin.flags = 42
        };

        const uint8_t* ptr = data.data();
        CHECK_THROWS_AS(Server::read(ptr, ptr + data.size()), std::runtime_error);
    }

    TEST_CASE("Validation function - UserID") {
        CHECK(validate_UserID(1) == true);
        CHECK(validate_UserID(5000) == true);
        CHECK(validate_UserID(65535) == true);
        CHECK(validate_UserID(0) == false);
    }

    TEST_CASE("Validation function - Port") {
        CHECK(validate_Port(1025) == true);
        CHECK(validate_Port(8080) == true);
        CHECK(validate_Port(65535) == true);
        CHECK(validate_Port(1024) == false);
        CHECK(validate_Port(80) == false);
        CHECK(validate_Port(0) == false);
    }

    TEST_CASE("Validation function - Percentage") {
        CHECK(validate_Percentage(0) == true);
        CHECK(validate_Percentage(50) == true);
        CHECK(validate_Percentage(100) == true);
        CHECK(validate_Percentage(101) == false);
        CHECK(validate_Percentage(255) == false);
    }
}

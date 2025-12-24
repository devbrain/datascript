//
// End-to-End Test: Choice Types (Tagged Unions)
//
#include <doctest/doctest.h>
#include <e2e_choices.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Choices") {

    TEST_CASE("Message - text message") {
        std::vector<uint8_t> data = {
            MSG_TEXT,    // msg_type = TEXT
            0x05,        // length = 5
            'H', 'e', 'l', 'l', 'o'
        };

        const uint8_t* ptr = data.data();
        Message obj = Message::read(ptr, ptr + data.size());

        CHECK(obj.msg_type == MSG_TEXT);
    }

    TEST_CASE("Message - number message") {
        std::vector<uint8_t> data = {
            MSG_NUMBER,  // msg_type = NUMBER
            0x39, 0x30, 0x00, 0x00  // value = 12345
        };

        const uint8_t* ptr = data.data();
        Message obj = Message::read(ptr, ptr + data.size());

        CHECK(obj.msg_type == MSG_NUMBER);
    }

    TEST_CASE("NetworkPacket - request packet") {
        std::vector<uint8_t> data = {
            PACKET_REQUEST,  // packet_type
            0x01, 0x00,      // request_id = 1
            0x02             // method = 2
        };

        const uint8_t* ptr = data.data();
        NetworkPacket obj = NetworkPacket::read(ptr, ptr + data.size());

        CHECK(obj.packet_type == PACKET_REQUEST);
    }
}

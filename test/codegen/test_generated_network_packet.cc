//
// End-to-end tests using generated code from network_packet.ds
//
#include <doctest/doctest.h>
#include <network_packet.h>
#include <vector>
#include <cstdint>

using namespace generated;

TEST_SUITE("E2E Generated - Network Packet") {

    TEST_CASE("IPv4Header - basic parsing") {
        std::vector<uint8_t> data = {
            0x45,        // version_ihl: v4, 5 words (20 bytes)
            0x00,        // tos
            0x3C, 0x00,  // total_length = 60
            64,          // ttl
            6,           // protocol = TCP
            0x00, 0x00,  // checksum
            192, 168, 1, 100,  // source_ip
            10, 0, 0, 1        // dest_ip
        };

        const uint8_t* ptr = data.data();
        IPv4Header ip = IPv4Header::read(ptr, ptr + data.size());

        CHECK( ip.version_ihl == 0x45 );
        CHECK( ip.protocol == 6 );
        CHECK( ip.total_length == 60 );
        CHECK( ip.ttl == 64 );
    }

    TEST_CASE("IPv4Header - version function") {
        std::vector<uint8_t> data = {
            0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        IPv4Header ip = IPv4Header::read(ptr, ptr + data.size());

        CHECK( ip.version() == 4 );
    }

    TEST_CASE("IPv4Header - header_length function") {
        std::vector<uint8_t> data = {
            0x45, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        IPv4Header ip = IPv4Header::read(ptr, ptr + data.size());

        // IHL = 5, so 5 * 4 = 20 bytes
        CHECK( ip.header_length() == 20 );
    }

    TEST_CASE("IPv4Header - is_tcp function") {
        // TCP packet
        std::vector<uint8_t> tcp_data = {
            0x45, 0x00, 0x00, 0x00, 0x00,
            6,    // protocol = TCP
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const uint8_t* ptr1 = tcp_data.data();
        IPv4Header ip_tcp = IPv4Header::read(ptr1, ptr1 + tcp_data.size());
        CHECK( ip_tcp.is_tcp() == true );
        CHECK( ip_tcp.is_udp() == false );

        // UDP packet
        std::vector<uint8_t> udp_data = {
            0x45, 0x00, 0x00, 0x00, 0x00,
            17,   // protocol = UDP
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const uint8_t* ptr2 = udp_data.data();
        IPv4Header ip_udp = IPv4Header::read(ptr2, ptr2 + udp_data.size());
        CHECK( ip_udp.is_tcp() == false );
        CHECK( ip_udp.is_udp() == true );
    }

    TEST_CASE("TCPHeader - basic parsing") {
        std::vector<uint8_t> data = {
            0xBB, 0x01,  // source_port = 443
            0x39, 0x30,  // dest_port = 12345
            0x01, 0x00, 0x00, 0x00,  // sequence
            0x00, 0x00, 0x00, 0x00,  // ack_number
            0x50,        // data_offset_flags (5 words = 20 bytes)
            0x02,        // flags = SYN
            0x00, 0x10,  // window
            0x00, 0x00,  // checksum
            0x00, 0x00   // urgent
        };

        const uint8_t* ptr = data.data();
        TCPHeader tcp = TCPHeader::read(ptr, ptr + data.size());

        CHECK( tcp.source_port == 443 );
        CHECK( tcp.dest_port == 12345 );
        CHECK( tcp.flags == 0x02 );
    }

    TEST_CASE("TCPHeader - header_length function") {
        std::vector<uint8_t> data = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00,
            0x50,  // data_offset = 5, so 5 * 4 = 20 bytes
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        TCPHeader tcp = TCPHeader::read(ptr, ptr + data.size());

        CHECK( tcp.header_length() == 20 );
    }

    TEST_CASE("TCPHeader - flag checking functions") {
        // SYN flag
        std::vector<uint8_t> syn_data = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x02,  // flags = SYN
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const uint8_t* ptr1 = syn_data.data();
        TCPHeader tcp_syn = TCPHeader::read(ptr1, ptr1 + syn_data.size());
        CHECK( tcp_syn.is_syn() == true );
        CHECK( tcp_syn.is_ack() == false );
        CHECK( tcp_syn.is_fin() == false );

        // ACK flag
        std::vector<uint8_t> ack_data = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x10,  // flags = ACK
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const uint8_t* ptr2 = ack_data.data();
        TCPHeader tcp_ack = TCPHeader::read(ptr2, ptr2 + ack_data.size());
        CHECK( tcp_ack.is_syn() == false );
        CHECK( tcp_ack.is_ack() == true );
        CHECK( tcp_ack.is_fin() == false );

        // FIN flag
        std::vector<uint8_t> fin_data = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x01,  // flags = FIN
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };
        const uint8_t* ptr3 = fin_data.data();
        TCPHeader tcp_fin = TCPHeader::read(ptr3, ptr3 + fin_data.size());
        CHECK( tcp_fin.is_syn() == false );
        CHECK( tcp_fin.is_ack() == false );
        CHECK( tcp_fin.is_fin() == true );
    }

    TEST_CASE("TCPHeader - combined flags") {
        // SYN + ACK
        std::vector<uint8_t> data = {
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
            0x00, 0x00, 0x00, 0x00, 0x00,
            0x12,  // flags = SYN | ACK
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        TCPHeader tcp = TCPHeader::read(ptr, ptr + data.size());

        CHECK( tcp.is_syn() == true );
        CHECK( tcp.is_ack() == true );
        CHECK( tcp.is_fin() == false );
    }
}

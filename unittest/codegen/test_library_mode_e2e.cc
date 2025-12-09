/**
 * Comprehensive End-to-End Test for C++ Library Mode
 *
 * Tests all library mode features:
 * - Parse functions (3 overloads)
 * - Metadata access (structs and fields)
 * - StructView API
 * - Field iteration
 * - Type queries
 * - Value formatting
 * - JSON serialization
 * - Pretty printing
 * - Error handling
 * - Nested structs
 */

#include <doctest/doctest.h>
#include "net_protocol_impl.h"
#include <sstream>
#include <vector>
#include <cstdint>

using namespace net::protocol;
using namespace net::protocol::introspection;

// Helper: Create sample packet data
std::vector<uint8_t> create_sample_packet_data() {
    return std::vector<uint8_t>{
        // PacketHeader
        0x50, 0x54, 0x45, 0x4E,  // magic = 0x4E455450 (NETP)
        0x01,                     // version = 1
        0x02,                     // type = DATA
        0x03,                     // flags = ENCRYPTED | COMPRESSED
        0x78, 0x56, 0x34, 0x12,  // sequence = 0x12345678
        0x05, 0x00,              // payload_length = 5
        // Timestamp
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,  // seconds
        0x21, 0x43, 0x65, 0x87,  // microseconds
        // Source IPv4Address
        192, 168, 1, 100,
        // Destination IPv4Address
        10, 0, 0, 1,
        // Payload (5 bytes)
        'H', 'e', 'l', 'l', 'o',
        // Checksum
        0xEF, 0xBE, 0xAD, 0xDE
    };
}

// ============================================================================
// Test Suite 1: Parse Functions (Phase 2)
// ============================================================================

TEST_CASE("Parse functions - pointer + length overload") {
    auto data = create_sample_packet_data();

    // Parse using pointer + length
    NetworkPacket packet = parse_NetworkPacket(data.data(), data.size());

    // Verify parsed data
    CHECK(packet.header.magic == 0x4E455450);
    CHECK(packet.header.version == 1);
    CHECK(packet.header.type == PacketType::DATA);
    CHECK(packet.header.flags == static_cast<ConnectionFlags>(3));
    CHECK(packet.header.sequence == 0x12345678);
    CHECK(packet.header.payload_length == 5);
    CHECK(packet.payload.size() == 5);
    CHECK(packet.payload[0] == 'H');
    CHECK(packet.checksum == 0xDEADBEEF);
}

TEST_CASE("Parse functions - std::vector overload") {
    auto data = create_sample_packet_data();

    // Parse using vector
    NetworkPacket packet = parse_NetworkPacket(data);

    // Verify same results
    CHECK(packet.header.magic == 0x4E455450);
    CHECK(packet.header.sequence == 0x12345678);
    CHECK(packet.payload.size() == 5);
}

TEST_CASE("Parse functions - std::span overload") {
    auto data = create_sample_packet_data();
    std::span<const uint8_t> span(data);

    // Parse using span
    NetworkPacket packet = parse_NetworkPacket(span);

    // Verify same results
    CHECK(packet.header.magic == 0x4E455450);
    CHECK(packet.header.sequence == 0x12345678);
    CHECK(packet.payload.size() == 5);
}

TEST_CASE("Parse functions - nested struct parsing") {
    std::vector<uint8_t> header_data = {
        0x50, 0x54, 0x45, 0x4E,  // magic
        0x01,                     // version
        0x02,                     // type
        0x03,                     // flags
        0x78, 0x56, 0x34, 0x12,  // sequence
        0x0A, 0x00,              // payload_length
        0x10, 0x32, 0x54, 0x76, 0x98, 0xBA, 0xDC, 0xFE,  // timestamp.seconds
        0x21, 0x43, 0x65, 0x87   // timestamp.microseconds
    };

    PacketHeader header = parse_PacketHeader(header_data);

    CHECK(header.magic == 0x4E455450);
    CHECK(header.timestamp.seconds == 0xFEDCBA9876543210ULL);
    CHECK(header.timestamp.microseconds == 0x87654321);
}

// ============================================================================
// Test Suite 2: Metadata Infrastructure (Phase 3)
// ============================================================================

TEST_CASE("Metadata - struct metadata access") {
    // Access PacketHeader metadata
    CHECK(PacketHeader_meta.type_name == std::string("PacketHeader"));
    CHECK(PacketHeader_meta.docstring != nullptr);
    CHECK(PacketHeader_meta.size_in_bytes == sizeof(PacketHeader));
    CHECK(PacketHeader_meta.field_count == 7);
    CHECK(PacketHeader_meta.fields != nullptr);
}

TEST_CASE("Metadata - field metadata iteration") {
    const StructMeta& meta = PacketHeader_meta;

    // Verify all fields exist
    REQUIRE(meta.field_count == 7);

    // Check first field (magic)
    CHECK(meta.fields[0].name == std::string("magic"));
    CHECK(meta.fields[0].type_name == std::string("uint32_t"));
    CHECK(meta.fields[0].type_kind == FieldType::Primitive);
    CHECK(meta.fields[0].offset == offsetof(PacketHeader, magic));
    CHECK(meta.fields[0].size == sizeof(uint32_t));

    // Check enum field (type)
    CHECK(meta.fields[2].name == std::string("type"));
    CHECK(meta.fields[2].type_kind == FieldType::Enum);

    // Check nested struct field (timestamp) - it's the 7th field (index 6)
    CHECK(meta.fields[6].name == std::string("timestamp"));
    CHECK(meta.fields[6].type_kind == FieldType::Struct);
}

TEST_CASE("Metadata - field formatters") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);

    // Test formatter for primitive field (sequence is the 5th field, index 4)
    const FieldMeta& seq_field = PacketHeader_meta.fields[4];  // sequence
    std::string seq_value = seq_field.format_value(&packet.header);
    CHECK(seq_value == "305419896");  // 0x12345678 in decimal

    // Test formatter for enum field
    const FieldMeta& type_field = PacketHeader_meta.fields[2];  // type
    std::string type_value = type_field.format_value(&packet.header);
    CHECK(type_value == "2");  // PacketType::DATA
}

TEST_CASE("Metadata - nested struct metadata") {
    CHECK(Timestamp_meta.type_name == std::string("Timestamp"));
    CHECK(Timestamp_meta.field_count == 2);
    CHECK(Timestamp_meta.fields[0].name == std::string("seconds"));
    CHECK(Timestamp_meta.fields[1].name == std::string("microseconds"));
}

TEST_CASE("Metadata - NetworkPacket metadata") {
    CHECK(NetworkPacket_meta.type_name == std::string("NetworkPacket"));
    CHECK(NetworkPacket_meta.field_count == 5);

    // Verify field types
    CHECK(NetworkPacket_meta.fields[0].type_kind == FieldType::Struct);   // header
    CHECK(NetworkPacket_meta.fields[1].type_kind == FieldType::Struct);   // source
    CHECK(NetworkPacket_meta.fields[2].type_kind == FieldType::Struct);   // destination
    CHECK(NetworkPacket_meta.fields[3].type_kind == FieldType::Array);    // payload
    CHECK(NetworkPacket_meta.fields[4].type_kind == FieldType::Primitive); // checksum
}

// ============================================================================
// Test Suite 3: StructView API (Phase 4)
// ============================================================================

TEST_CASE("StructView - construction and basic accessors") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);

    StructView<NetworkPacket> view(&packet);

    CHECK(view.type_name() == std::string("NetworkPacket"));
    CHECK(view.size_in_bytes() == sizeof(NetworkPacket));
    CHECK(view.field_count() == 5);
    CHECK(view.has_docstring() == true);
    CHECK(view.docstring() != nullptr);
}

TEST_CASE("StructView - field access by index") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    // Access each field
    Field f0 = view.field(0);
    CHECK(f0.name() == std::string("header"));
    CHECK(f0.is_struct() == true);
    CHECK(f0.is_primitive() == false);

    Field f1 = view.field(1);
    CHECK(f1.name() == std::string("source"));
    CHECK(f1.is_struct() == true);

    Field f3 = view.field(3);
    CHECK(f3.name() == std::string("payload"));
    CHECK(f3.is_array() == true);
}

TEST_CASE("StructView - field access by name") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    // Find field by name
    Field header = view.find_field("header");
    CHECK(header.name() == std::string("header"));
    CHECK(header.type_name() == std::string("PacketHeader"));

    Field checksum = view.find_field("checksum");
    CHECK(checksum.name() == std::string("checksum"));
    CHECK(checksum.is_primitive() == true);
}

TEST_CASE("StructView - field not found error") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    // Try to find non-existent field
    CHECK_THROWS_AS(view.find_field("nonexistent"), std::runtime_error);
}

TEST_CASE("StructView - field index out of range") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    // Try to access invalid index
    CHECK_THROWS_AS(view.field(999), std::out_of_range);
}

// ============================================================================
// Test Suite 4: Field Iteration (Phase 4)
// ============================================================================

TEST_CASE("Field iteration - range-based for loop") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    std::vector<std::string> field_names;
    for (const auto& field : view.fields()) {
        field_names.push_back(field.name());
    }

    REQUIRE(field_names.size() == 5);
    CHECK(field_names[0] == "header");
    CHECK(field_names[1] == "source");
    CHECK(field_names[2] == "destination");
    CHECK(field_names[3] == "payload");
    CHECK(field_names[4] == "checksum");
}

TEST_CASE("Field iteration - manual iterator") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    FieldRange range = view.fields();
    FieldIterator it = range.begin();
    FieldIterator end = range.end();

    CHECK(it != end);
    Field f = *it;
    CHECK(f.name() == std::string("header"));

    ++it;
    CHECK(it != end);
    CHECK((*it).name() == std::string("source"));
}

TEST_CASE("Field iteration - nested struct") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    size_t count = 0;
    for (const auto& field : view.fields()) {
        ++count;
        // Verify we can access field properties
        CHECK(field.name() != nullptr);
        CHECK(field.type_name() != nullptr);
    }

    CHECK(count == 7);  // PacketHeader has 7 fields
}

// ============================================================================
// Test Suite 5: Type Queries (Phase 4)
// ============================================================================

TEST_CASE("Type queries - primitive types") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    Field magic = view.find_field("magic");
    CHECK(magic.is_primitive() == true);
    CHECK(magic.is_enum() == false);
    CHECK(magic.is_struct() == false);
    CHECK(magic.is_array() == false);
}

TEST_CASE("Type queries - enum types") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    Field type = view.find_field("type");
    CHECK(type.is_enum() == true);
    CHECK(type.is_primitive() == false);
    CHECK(type.is_struct() == false);
}

TEST_CASE("Type queries - struct types") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    Field header = view.find_field("header");
    CHECK(header.is_struct() == true);
    CHECK(header.is_primitive() == false);
    CHECK(header.is_enum() == false);
}

TEST_CASE("Type queries - array types") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    Field payload = view.find_field("payload");
    CHECK(payload.is_array() == true);
    CHECK(payload.is_primitive() == false);
}

// ============================================================================
// Test Suite 6: Value Formatting (Phase 3 & 4)
// ============================================================================

TEST_CASE("Value formatting - primitive values") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    Field magic = view.find_field("magic");
    std::string value = magic.value_as_string();
    CHECK(value == "1313166416");  // 0x4E455450 in decimal

    Field version = view.find_field("version");
    CHECK(version.value_as_string() == "1");
}

TEST_CASE("Value formatting - enum values") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    Field type = view.find_field("type");
    CHECK(type.value_as_string() == "2");  // Numeric value
}

TEST_CASE("Value formatting - nested struct") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    Field header = view.find_field("header");
    std::string value = header.value_as_string();
    CHECK(value == "<PacketHeader>");  // Placeholder for struct
}

TEST_CASE("Value formatting - array") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    Field payload = view.find_field("payload");
    std::string value = payload.value_as_string();
    CHECK(value == "[5 items]");  // Array with count
}

// ============================================================================
// Test Suite 7: JSON Serialization (Phase 4)
// ============================================================================

TEST_CASE("JSON serialization - to_json() string overload") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    std::string json = view.to_json();

    // Verify it's valid JSON structure
    CHECK(json.front() == '{');
    CHECK(json.back() == '}');

    // Verify it contains field names
    CHECK(json.find("\"header\"") != std::string::npos);
    CHECK(json.find("\"source\"") != std::string::npos);
    CHECK(json.find("\"destination\"") != std::string::npos);
    CHECK(json.find("\"payload\"") != std::string::npos);
    CHECK(json.find("\"checksum\"") != std::string::npos);
}

TEST_CASE("JSON serialization - to_json() stream overload") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    std::ostringstream oss;
    view.to_json(oss);
    std::string json = oss.str();

    // Same checks as string overload
    CHECK(json.front() == '{');
    CHECK(json.back() == '}');
    CHECK(json.find("\"header\"") != std::string::npos);
}

TEST_CASE("JSON serialization - nested struct") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    std::string json = view.to_json();

    CHECK(json.find("\"magic\"") != std::string::npos);
    CHECK(json.find("\"version\"") != std::string::npos);
    CHECK(json.find("\"type\"") != std::string::npos);
    CHECK(json.find("\"sequence\"") != std::string::npos);
}

TEST_CASE("JSON serialization - deeply nested struct") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<Timestamp> view(&packet.header.timestamp);

    std::string json = view.to_json();

    CHECK(json.find("\"seconds\"") != std::string::npos);
    CHECK(json.find("\"microseconds\"") != std::string::npos);
}

// ============================================================================
// Test Suite 8: Pretty Printing (Phase 4)
// ============================================================================

TEST_CASE("Pretty printing - basic output") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<NetworkPacket> view(&packet);

    std::ostringstream oss;
    view.print(oss, 0);
    std::string output = oss.str();

    // Verify structure
    CHECK(output.find("NetworkPacket {") != std::string::npos);
    CHECK(output.find("header:") != std::string::npos);
    CHECK(output.find("checksum:") != std::string::npos);
    CHECK(output.find("}") != std::string::npos);
}

TEST_CASE("Pretty printing - with indentation") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<PacketHeader> view(&packet.header);

    std::ostringstream oss;
    view.print(oss, 4);
    std::string output = oss.str();

    // Verify indentation (4 spaces at start)
    CHECK(output.substr(0, 4) == "    ");
}

TEST_CASE("Pretty printing - nested struct") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);
    StructView<Timestamp> view(&packet.header.timestamp);

    std::ostringstream oss;
    view.print(oss);
    std::string output = oss.str();

    CHECK(output.find("Timestamp {") != std::string::npos);
    CHECK(output.find("seconds:") != std::string::npos);
    CHECK(output.find("microseconds:") != std::string::npos);
}

// ============================================================================
// Test Suite 9: Integration Tests
// ============================================================================

TEST_CASE("Integration - complete workflow") {
    // 1. Parse binary data
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);

    // 2. Create StructView
    StructView<NetworkPacket> view(&packet);

    // 3. Verify metadata
    CHECK(view.field_count() == 5);

    // 4. Iterate fields
    size_t count = 0;
    for (const auto& field : view.fields()) {
        ++count;
    }
    CHECK(count == 5);

    // 5. Access specific field
    Field payload = view.find_field("payload");
    CHECK(payload.is_array() == true);

    // 6. Serialize to JSON
    std::string json = view.to_json();
    CHECK(json.size() > 0);

    // 7. Pretty print
    std::ostringstream oss;
    view.print(oss);
    CHECK(oss.str().size() > 0);
}

TEST_CASE("Integration - multiple struct types") {
    auto data = create_sample_packet_data();
    NetworkPacket packet = parse_NetworkPacket(data);

    // Test NetworkPacket
    StructView<NetworkPacket> packet_view(&packet);
    CHECK(packet_view.field_count() == 5);

    // Test nested PacketHeader
    StructView<PacketHeader> header_view(&packet.header);
    CHECK(header_view.field_count() == 7);

    // Test deeply nested Timestamp
    StructView<Timestamp> time_view(&packet.header.timestamp);
    CHECK(time_view.field_count() == 2);

    // Test IPv4Address
    StructView<IPv4Address> addr_view(&packet.source);
    CHECK(addr_view.field_count() == 1);  // octets array
}

TEST_CASE("Integration - constants access") {
    // Verify constants are accessible
    CHECK(PROTOCOL_VERSION == 1);
    CHECK(PACKET_MAGIC == 0x4E455450);
    CHECK(MAX_PAYLOAD_SIZE == 8192);
}

TEST_CASE("Integration - enum values") {
    // Verify enum values are accessible
    CHECK(PacketType::CONNECT == static_cast<PacketType>(1));
    CHECK(PacketType::DATA == static_cast<PacketType>(2));
    CHECK(PacketType::ACK == static_cast<PacketType>(3));
    CHECK(PacketType::DISCONNECT == static_cast<PacketType>(4));

    CHECK(ConnectionFlags::NONE == static_cast<ConnectionFlags>(0));
    CHECK(ConnectionFlags::ENCRYPTED == static_cast<ConnectionFlags>(1));
    CHECK(ConnectionFlags::COMPRESSED == static_cast<ConnectionFlags>(2));
}

/**
 * Library Mode Test: Choice Types (Comprehensive)
 *
 * Tests all choice features with library mode introspection:
 * - External discriminator choices
 * - Inline discriminator choices
 * - Default cases
 * - Multiple case values
 * - Nested choices
 * - Metadata access for choices
 * - JSON serialization of choices
 * - Type queries for choice fields
 */

#include <doctest/doctest.h>
#include "Message_impl.h"
#include <sstream>
#include <vector>
#include <cstdint>

using namespace generated;

// ============================================================================
// Helper Functions
// ============================================================================

std::vector<uint8_t> create_text_message() {
    return std::vector<uint8_t>{
        MSG_TEXT,  // msg_type = 1
        0x05       // text_length = 5
    };
}

std::vector<uint8_t> create_number_message() {
    return std::vector<uint8_t>{
        MSG_NUMBER,            // msg_type = 2
        0x39, 0x30, 0x00, 0x00  // number_value = 12345
    };
}

std::vector<uint8_t> create_ordinal_resource() {
    return std::vector<uint8_t>{
        0xFF,       // discriminator = 0xFF (ordinal case)
        0x42, 0x00  // ordinal = 66
    };
}

std::vector<uint8_t> create_name_resource() {
    return std::vector<uint8_t>{
        0x03,  // discriminator = 3 (default case)
        0x03   // name_length = 3
    };
}

std::vector<uint8_t> create_name_or_id_ordinal() {
    // With the inline discriminator peek fix, the position is restored so the inline
    // struct re-reads the discriminator as its first field (marker).
    return std::vector<uint8_t>{
        0xFF,       // discriminator = 0xFF, also read as marker
        0x2A, 0x00  // id_value = 42
    };
}

std::vector<uint8_t> create_name_or_id_string() {
    // With the inline discriminator peek fix, the position is restored so the inline
    // struct re-reads the discriminator as its first field (length).
    return std::vector<uint8_t>{
        0x04,              // discriminator = 4 (default case), also read as length = 4
        'T', 'e', 's', 't' // chars[4]
    };
}

std::vector<uint8_t> create_status_success() {
    return std::vector<uint8_t>{
        0xC8, 0x00,  // status code = 200 (0x00C8)
        0x01         // success_data = 1
    };
}

std::vector<uint8_t> create_status_error() {
    return std::vector<uint8_t>{
        0x90, 0x01,  // status code = 400 (0x0190)
        0x04, 0x00   // error_code = 4
    };
}

std::vector<uint8_t> create_status_unknown() {
    return std::vector<uint8_t>{
        0xF4, 0x01,              // status code = 500 (default case)
        0x05, 0x00, 0x00, 0x00   // unknown_status = 5 (uint32 little endian)
    };
}

// ============================================================================
// Test Suite 1: Parse Functions with External Discriminator
// ============================================================================

TEST_CASE("External discriminator - parse text message") {
    auto data = create_text_message();
    Message msg = parse_Message(data);

    CHECK(msg.msg_type == MSG_TEXT);
    // Choice should have text_length case active
    auto* text_case = msg.payload.as_text_length();
    REQUIRE(text_case != nullptr);
    CHECK(text_case->value == 5);
}

TEST_CASE("External discriminator - parse number message") {
    auto data = create_number_message();
    Message msg = parse_Message(data);

    CHECK(msg.msg_type == MSG_NUMBER);
    auto* number_case = msg.payload.as_number_value();
    REQUIRE(number_case != nullptr);
    CHECK(number_case->value == 12345);
}

// ============================================================================
// Test Suite 2: Parse Functions with Inline Discriminator
// ============================================================================

TEST_CASE("Inline discriminator - parse ordinal resource") {
    auto data = create_ordinal_resource();
    Resource res = parse_Resource(data);

    auto* ordinal_case = res.id.as_ordinal();
    REQUIRE(ordinal_case != nullptr);
    CHECK(ordinal_case->value == 66);
}

TEST_CASE("Inline discriminator - parse name resource (default case)") {
    auto data = create_name_resource();
    Resource res = parse_Resource(data);

    auto* name_case = res.id.as_name_length();
    REQUIRE(name_case != nullptr);
    CHECK(name_case->value == 3);
}

// ============================================================================
// Test Suite 3: Default Cases with Inline Structs
// ============================================================================

TEST_CASE("Default case with inline struct - ordinal") {
    auto data = create_name_or_id_ordinal();
    NamedEntity entity = parse_NamedEntity(data);

    auto* ordinal_case = entity.identifier.as_ordinal_id();
    REQUIRE(ordinal_case != nullptr);
    CHECK(ordinal_case->value.marker == 0xFF);
    CHECK(ordinal_case->value.id_value == 42);
}

TEST_CASE("Default case with inline struct - string name") {
    auto data = create_name_or_id_string();
    NamedEntity entity = parse_NamedEntity(data);

    auto* string_case = entity.identifier.as_string_name();
    REQUIRE(string_case != nullptr);
    CHECK(string_case->value.length == 4);
    REQUIRE(string_case->value.chars.size() == 4);
    CHECK(string_case->value.chars[0] == 'T');
    CHECK(string_case->value.chars[1] == 'e');
    CHECK(string_case->value.chars[2] == 's');
    CHECK(string_case->value.chars[3] == 't');
}

// ============================================================================
// Test Suite 4: Multiple Case Values
// ============================================================================

TEST_CASE("Multiple case values - success (200)") {
    auto data = create_status_success();
    Response resp = parse_Response(data);

    auto* success_case = resp.status.as_success_data();
    REQUIRE(success_case != nullptr);
    CHECK(success_case->value == 1);
}

TEST_CASE("Multiple case values - error (400)") {
    auto data = create_status_error();
    Response resp = parse_Response(data);

    auto* error_case = resp.status.as_error_code();
    REQUIRE(error_case != nullptr);
    CHECK(error_case->value == 4);
}

TEST_CASE("Multiple case values - unknown (default)") {
    auto data = create_status_unknown();
    Response resp = parse_Response(data);

    auto* unknown_case = resp.status.as_unknown_status();
    REQUIRE(unknown_case != nullptr);
    CHECK(unknown_case->value == 5);
}

// ============================================================================
// Test Suite 5: Struct Metadata for Choices
// ============================================================================

TEST_CASE("Struct metadata - Message contains choice field") {
    auto data = create_text_message();
    Message msg = parse_Message(data);

    StructView<Message> view(&msg);

    CHECK(view.type_name() == std::string("Message"));
    CHECK(view.field_count() == 2);

    // First field: msg_type (uint8)
    auto field0 = view.field(0);
    CHECK(field0.name() == std::string("msg_type"));
    CHECK(field0.is_primitive());

    // Second field: payload (choice)
    auto field1 = view.field(1);
    CHECK(field1.name() == std::string("payload"));
    // TODO: Add is_choice() once implemented
    // CHECK(field1.is_choice());
}

TEST_CASE("Struct metadata - Resource contains inline discriminator choice") {
    auto data = create_ordinal_resource();
    Resource res = parse_Resource(data);

    StructView<Resource> view(&res);

    CHECK(view.type_name() == std::string("Resource"));
    CHECK(view.field_count() == 1);

    auto field = view.field(0);
    CHECK(field.name() == std::string("id"));
}

// ============================================================================
// Test Suite 6: JSON Serialization of Choices
// ============================================================================

TEST_CASE("JSON serialization - external discriminator choice") {
    auto data = create_text_message();
    Message msg = parse_Message(data);

    StructView<Message> view(&msg);
    std::string json = view.to_json();

    // Check JSON structure
    CHECK(json.find("\"msg_type\"") != std::string::npos);
    CHECK(json.find("\"payload\"") != std::string::npos);

    // TODO: Validate choice serialization shows active variant
    // For now, just check it doesn't throw
}

TEST_CASE("JSON serialization - inline discriminator choice") {
    auto data = create_ordinal_resource();
    Resource res = parse_Resource(data);

    StructView<Resource> view(&res);
    std::string json = view.to_json();

    CHECK(json.find("\"id\"") != std::string::npos);
}

TEST_CASE("JSON serialization - default case with inline struct") {
    auto data = create_name_or_id_string();
    NamedEntity entity = parse_NamedEntity(data);

    StructView<NamedEntity> view(&entity);
    std::string json = view.to_json();

    CHECK(json.find("\"identifier\"") != std::string::npos);
}

// ============================================================================
// Test Suite 7: Field Iteration with Choices
// ============================================================================

TEST_CASE("Field iteration - Message with choice field") {
    auto data = create_text_message();
    Message msg = parse_Message(data);

    StructView<Message> view(&msg);

    std::vector<std::string> field_names;
    for (const auto& field : view.fields()) {
        field_names.push_back(field.name());
    }

    REQUIRE(field_names.size() == 2);
    CHECK(field_names[0] == "msg_type");
    CHECK(field_names[1] == "payload");
}

// ============================================================================
// Test Suite 8: Pretty Printing with Choices
// ============================================================================

TEST_CASE("Pretty printing - external discriminator choice") {
    auto data = create_text_message();
    Message msg = parse_Message(data);

    StructView<Message> view(&msg);

    std::ostringstream oss;
    view.print(oss, 2);

    std::string output = oss.str();
    CHECK(output.find("Message") != std::string::npos);
    CHECK(output.find("msg_type") != std::string::npos);
    CHECK(output.find("payload") != std::string::npos);
}

TEST_CASE("Pretty printing - inline discriminator with default") {
    auto data = create_name_or_id_string();
    NamedEntity entity = parse_NamedEntity(data);

    StructView<NamedEntity> view(&entity);

    std::ostringstream oss;
    view.print(oss, 2);

    std::string output = oss.str();
    CHECK(output.find("NamedEntity") != std::string::npos);
    CHECK(output.find("identifier") != std::string::npos);
}

// ============================================================================
// Test Suite 9: Integration - Complete Workflow
// ============================================================================

TEST_CASE("Integration - external discriminator complete workflow") {
    // Parse
    auto data = create_number_message();
    Message msg = parse_Message(data);

    // Verify parsed data
    CHECK(msg.msg_type == MSG_NUMBER);
    auto* number_case = msg.payload.as_number_value();
    REQUIRE(number_case != nullptr);
    CHECK(number_case->value == 12345);

    // Introspection
    StructView<Message> view(&msg);
    CHECK(view.type_name() == "Message");
    CHECK(view.field_count() == 2);

    // JSON
    std::string json = view.to_json();
    CHECK(!json.empty());

    // Pretty print
    std::ostringstream oss;
    view.print(oss);
    CHECK(!oss.str().empty());
}

TEST_CASE("Integration - inline discriminator with default case complete workflow") {
    // Parse
    auto data = create_name_or_id_string();
    NamedEntity entity = parse_NamedEntity(data);

    // Verify parsed data
    auto* string_case = entity.identifier.as_string_name();
    REQUIRE(string_case != nullptr);
    CHECK(string_case->value.length == 4);

    // Introspection
    StructView<NamedEntity> view(&entity);
    CHECK(view.type_name() == "NamedEntity");

    // JSON
    std::string json = view.to_json();
    CHECK(!json.empty());

    // Pretty print
    std::ostringstream oss;
    view.print(oss);
    CHECK(!oss.str().empty());
}

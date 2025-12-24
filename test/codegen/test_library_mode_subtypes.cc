/**
 * Library Mode E2E Test: Subtypes
 * Comprehensive test coverage for library mode subtype handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/library_mode_subtypes_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_network_config() {
    return std::vector<uint8_t>{
        0x50, 0x04,  // server_port = 1104 (uint16 LE)
        0xBB, 0x1F,  // client_port = 8123 (uint16 LE)
        0x4B,        // cpu_limit = 75
        0xE5, 0x07   // year = 2021 (uint16 LE)
    };
}

// ============================================================================
// Test Suite 1: Simple Subtype Parsing
// ============================================================================

TEST_CASE("Network config - parse all fields") {
    auto data = create_network_config();
    NetworkConfig nc = parse_NetworkConfig(data);

    CHECK(nc.server_port == 1104);
    CHECK(nc.client_port == 8123);
    CHECK(nc.cpu_limit == 75);
    CHECK(nc.year == 2021);
}

// ============================================================================
// Test Suite 2: Subtype Properties
// ============================================================================

TEST_CASE("Subtypes - are underlying types") {
    auto data = create_network_config();
    NetworkConfig nc = parse_NetworkConfig(data);

    // Subtypes should be usable as their base types
    uint16_t port = nc.server_port;
    uint8_t percent = nc.cpu_limit;
    uint16_t year_val = nc.year;

    CHECK(port == 1104);
    CHECK(percent == 75);
    CHECK(year_val == 2021);
}

// ============================================================================
// Test Suite 3: Introspection Features
// ============================================================================

TEST_CASE("Subtype struct - introspection") {
    auto data = create_network_config();
    NetworkConfig nc = parse_NetworkConfig(data);

    StructView<NetworkConfig> view(&nc);

    CHECK(view.field_count() == 4);
    CHECK(std::string(view.type_name()) == "NetworkConfig");
}

/**
 * Library Mode E2E Test: Parameterized Types
 * Comprehensive test coverage for library mode parameterized type handling
 */

#include <doctest/doctest.h>
#include "../library_mode_generated/Buffer_16_impl.h"

using namespace generated;

// ============================================================================
// Test Data Creators
// ============================================================================

std::vector<uint8_t> create_container() {
    std::vector<uint8_t> data;

    // small_buffer: Buffer(16) - 16 bytes
    for (int i = 0; i < 16; i++) {
        data.push_back(static_cast<uint8_t>(i));
    }

    // large_buffer: Buffer(256) - 256 bytes
    for (int i = 0; i < 256; i++) {
        data.push_back(static_cast<uint8_t>(i));
    }

    // another_small: Buffer(16) - 16 bytes (reuses Buffer_16)
    for (int i = 0; i < 16; i++) {
        data.push_back(static_cast<uint8_t>(i + 100));
    }

    // transform: Matrix(4, 4) - 16 uint32 values
    for (int i = 0; i < 16; i++) {
        uint32_t val = 1000 + i;
        data.push_back(val & 0xFF);
        data.push_back((val >> 8) & 0xFF);
        data.push_back((val >> 16) & 0xFF);
        data.push_back((val >> 24) & 0xFF);
    }

    // rotation: Matrix(3, 3) - 9 uint32 values
    for (int i = 0; i < 9; i++) {
        uint32_t val = 2000 + i;
        data.push_back(val & 0xFF);
        data.push_back((val >> 8) & 0xFF);
        data.push_back((val >> 16) & 0xFF);
        data.push_back((val >> 24) & 0xFF);
    }

    return data;
}

// ============================================================================
// Test Suite 1: Simple Parameterized Type
// ============================================================================

TEST_CASE("Container - parse all parameterized fields") {
    auto data = create_container();
    Container c = parse_Container(data);

    CHECK(c.small_buffer.data.size() == 16);
    CHECK(c.large_buffer.data.size() == 256);
    CHECK(c.another_small.data.size() == 16);
    CHECK(c.transform.values.size() == 16);
    CHECK(c.rotation.values.size() == 9);
}

TEST_CASE("Parameterized buffer - verify data") {
    auto data = create_container();
    Container c = parse_Container(data);

    // Check small_buffer contents
    CHECK(c.small_buffer.data[0] == 0);
    CHECK(c.small_buffer.data[15] == 15);

    // Check large_buffer contents
    CHECK(c.large_buffer.data[0] == 0);
    CHECK(c.large_buffer.data[255] == 255);

    // Check another_small contents (different data)
    CHECK(c.another_small.data[0] == 100);
    CHECK(c.another_small.data[15] == 115);
}

// ============================================================================
// Test Suite 2: Multiple Parameters
// ============================================================================

TEST_CASE("Matrix - verify data") {
    auto data = create_container();
    Container c = parse_Container(data);

    // Check transform matrix (4x4)
    CHECK(c.transform.values[0] == 1000);
    CHECK(c.transform.values[15] == 1015);

    // Check rotation matrix (3x3)
    CHECK(c.rotation.values[0] == 2000);
    CHECK(c.rotation.values[8] == 2008);
}

// ============================================================================
// Test Suite 3: Introspection Features
// ============================================================================

TEST_CASE("Parameterized struct - introspection") {
    auto data = create_container();
    Container c = parse_Container(data);

    StructView<Container> view(&c);

    CHECK(view.field_count() == 5);
    CHECK(std::string(view.type_name()) == "Container");
}

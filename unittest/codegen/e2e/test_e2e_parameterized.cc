//
// End-to-End Test: Parameterized Types (Generics)
//
#include <doctest/doctest.h>
#include <e2e_parameterized.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Parameterized Types") {

    TEST_CASE("DataContainer - multiple instantiations") {
        std::vector<uint8_t> data;

        // Buffer_16 small_buffer (16 bytes)
        for (int i = 0; i < 16; i++) data.push_back(i);

        // Buffer_256 large_buffer (256 bytes)
        for (int i = 0; i < 256; i++) data.push_back(i);

        // Buffer_16 another_small (16 bytes) - reuses same type
        for (int i = 0; i < 16; i++) data.push_back(0xFF - i);

        // FixedArray_4 (4 uint32 elements)
        for (int i = 0; i < 4; i++) {
            uint32_t val = (i + 1) * 100;
            data.push_back(val & 0xFF);
            data.push_back((val >> 8) & 0xFF);
            data.push_back((val >> 16) & 0xFF);
            data.push_back((val >> 24) & 0xFF);
        }

        // FixedArray_8 (8 uint32 elements)
        for (int i = 0; i < 8; i++) {
            uint32_t val = (i + 1) * 10;
            data.push_back(val & 0xFF);
            data.push_back((val >> 8) & 0xFF);
            data.push_back((val >> 16) & 0xFF);
            data.push_back((val >> 24) & 0xFF);
        }

        // Matrix_3_3 (9 uint32 elements)
        for (int i = 0; i < 9; i++) {
            data.push_back(i & 0xFF);
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        // Matrix_4_4 (16 uint32 elements)
        for (int i = 0; i < 16; i++) {
            data.push_back(i & 0xFF);
            data.push_back(0);
            data.push_back(0);
            data.push_back(0);
        }

        const uint8_t* ptr = data.data();
        DataContainer obj = DataContainer::read(ptr, ptr + data.size());

        // Verify small buffer
        CHECK(obj.small_buffer.data.size() == 16);
        CHECK(obj.small_buffer.data[0] == 0);
        CHECK(obj.small_buffer.data[15] == 15);
        CHECK(obj.small_buffer.get_capacity() == 16);

        // Verify large buffer
        CHECK(obj.large_buffer.data.size() == 256);
        CHECK(obj.large_buffer.get_capacity() == 256);

        // Verify another small buffer (reused type)
        CHECK(obj.another_small.data.size() == 16);
        CHECK(obj.another_small.data[0] == 0xFF);
        CHECK(obj.another_small.get_capacity() == 16);

        // Verify fixed arrays
        CHECK(obj.quad_array.elements.size() == 4);
        CHECK(obj.quad_array.get_size() == 4);
        CHECK(obj.quad_array.get(0) == 100);

        CHECK(obj.octet_array.elements.size() == 8);
        CHECK(obj.octet_array.get_size() == 8);

        // Verify matrices
        CHECK(obj.transform.get_rows() == 3);
        CHECK(obj.transform.get_cols() == 3);
        CHECK(obj.transform.get_total_size() == 9);

        CHECK(obj.projection.get_rows() == 4);
        CHECK(obj.projection.get_cols() == 4);
        CHECK(obj.projection.get_total_size() == 16);
    }
}

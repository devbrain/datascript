//
// End-to-End Test: Arrays
// Tests fixed, variable, and ranged arrays
//
#include <doctest/doctest.h>
#include <e2e_arrays.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Arrays") {

    TEST_CASE("FixedArrays - fixed-size arrays") {
        std::vector<uint8_t> data = {
            // magic[4]
            0x7F, 0x45, 0x4C, 0x46,  // ELF magic
            // values[3] - 3 uint16 values
            0x01, 0x00,
            0x02, 0x00,
            0x03, 0x00,
            // padding[8]
            0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
        };

        const uint8_t* ptr = data.data();
        FixedArrays obj = FixedArrays::read(ptr, ptr + data.size());

        CHECK(obj.magic.size() == 4);
        CHECK(obj.magic[0] == 0x7F);
        CHECK(obj.magic[1] == 'E');
        CHECK(obj.magic[2] == 'L');
        CHECK(obj.magic[3] == 'F');

        CHECK(obj.values.size() == 3);
        CHECK(obj.values[0] == 1);
        CHECK(obj.values[1] == 2);
        CHECK(obj.values[2] == 3);

        CHECK(obj.padding.size() == 8);
    }

    TEST_CASE("VariableArray - length from field") {
        std::vector<uint8_t> data = {
            0x04,        // count = 4
            // 4 uint16 values
            0x0A, 0x00,
            0x14, 0x00,
            0x1E, 0x00,
            0x28, 0x00
        };

        const uint8_t* ptr = data.data();
        VariableArray obj = VariableArray::read(ptr, ptr + data.size());

        CHECK(obj.count == 4);
        CHECK(obj.values.size() == 4);
        CHECK(obj.values[0] == 10);
        CHECK(obj.values[1] == 20);
        CHECK(obj.values[2] == 30);
        CHECK(obj.values[3] == 40);
    }

    TEST_CASE("MultipleVariableArrays - multiple arrays") {
        std::vector<uint8_t> data = {
            0x03,        // count1 = 3
            0xAA, 0xBB, 0xCC,  // data1
            0x02,        // count2 = 2
            0x11, 0x00,  // data2[0]
            0x22, 0x00   // data2[1]
        };

        const uint8_t* ptr = data.data();
        MultipleVariableArrays obj = MultipleVariableArrays::read(ptr, ptr + data.size());

        CHECK(obj.count1 == 3);
        CHECK(obj.data1.size() == 3);
        CHECK(obj.data1[0] == 0xAA);
        CHECK(obj.data1[1] == 0xBB);
        CHECK(obj.data1[2] == 0xCC);

        CHECK(obj.count2 == 2);
        CHECK(obj.data2.size() == 2);
        CHECK(obj.data2[0] == 0x11);
        CHECK(obj.data2[1] == 0x22);
    }

    TEST_CASE("PointArray - array of structs") {
        std::vector<uint8_t> data = {
            0x02,        // num_points = 2
            // Point 1
            0x0A, 0x00,  // x = 10
            0x14, 0x00,  // y = 20
            // Point 2
            0x1E, 0x00,  // x = 30
            0x28, 0x00   // y = 40
        };

        const uint8_t* ptr = data.data();
        PointArray obj = PointArray::read(ptr, ptr + data.size());

        CHECK(obj.num_points == 2);
        CHECK(obj.points.size() == 2);
        CHECK(obj.points[0].x == 10);
        CHECK(obj.points[0].y == 20);
        CHECK(obj.points[1].x == 30);
        CHECK(obj.points[1].y == 40);
    }

    TEST_CASE("Matrix3x3 - flat array with accessor function") {
        std::vector<uint8_t> data;
        // 3x3 matrix stored as flat array (9 uint32 values)
        for (uint32_t i = 1; i <= 9; i++) {
            data.push_back(i & 0xFF);
            data.push_back((i >> 8) & 0xFF);
            data.push_back((i >> 16) & 0xFF);
            data.push_back((i >> 24) & 0xFF);
        }

        const uint8_t* ptr = data.data();
        Matrix3x3 obj = Matrix3x3::read(ptr, ptr + data.size());

        CHECK(obj.rows.size() == 9);
        CHECK(obj.rows[0] == 1);
        CHECK(obj.rows[4] == 5);
        CHECK(obj.rows[8] == 9);

        // Test get function
        CHECK(obj.get(0, 0) == 1);
        CHECK(obj.get(1, 1) == 5);
        CHECK(obj.get(2, 2) == 9);
    }
}

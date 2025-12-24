//
// End-to-End Test: Complex Expressions
//
#include <doctest/doctest.h>
#include <e2e_expressions.h>
#include <vector>

using namespace generated;

TEST_SUITE("E2E - Expressions") {

    TEST_CASE("ArithmeticExpressions - basic arithmetic") {
        std::vector<uint8_t> data = {
            20,           // a = 20
            4,            // b = 4
            100, 0        // result = 100 (not used, just padding)
        };

        const uint8_t* ptr = data.data();
        ArithmeticExpressions obj = ArithmeticExpressions::read(ptr, ptr + data.size());

        CHECK(obj.a == 20);
        CHECK(obj.b == 4);

        CHECK(obj.add() == 24);
        CHECK(obj.sub() == 16);
        CHECK(obj.mul() == 80);
        CHECK(obj.div() == 5);
        CHECK(obj.mod() == 0);
        CHECK(obj.complex() == (20 + 4) * 2 - (20 / 4));  // 48 - 5 = 43
    }

    TEST_CASE("BitwiseExpressions - bit operations") {
        std::vector<uint8_t> data = {
            0xF0,  // value1 = 11110000
            0x0F   // value2 = 00001111
        };

        const uint8_t* ptr = data.data();
        BitwiseExpressions obj = BitwiseExpressions::read(ptr, ptr + data.size());

        CHECK(obj.bit_and() == 0x00);
        CHECK(obj.bit_or() == 0xFF);
        CHECK(obj.bit_xor() == 0xFF);
        // shift_left() returns uint16: (0xF0 << 0x0F) = 7864320, truncated to uint16 = 0
        CHECK(obj.shift_left() == static_cast<uint16_t>(0xF0 << 0x0F));
        CHECK(obj.shift_right() == (0xF0 >> 0x0F));
        CHECK(obj.bit_not() == static_cast<uint8_t>(~0xF0));
    }

    TEST_CASE("ComparisonExpressions - comparisons") {
        std::vector<uint8_t> data1 = {
            10, 0,  // a = 10
            20, 0   // b = 20
        };

        const uint8_t* ptr1 = data1.data();
        ComparisonExpressions obj1 = ComparisonExpressions::read(ptr1, ptr1 + data1.size());

        CHECK(obj1.eq() == false);
        CHECK(obj1.neq() == true);
        CHECK(obj1.lt() == true);
        CHECK(obj1.lte() == true);
        CHECK(obj1.gt() == false);
        CHECK(obj1.gte() == false);

        std::vector<uint8_t> data2 = {
            15, 0,  // a = 15
            15, 0   // b = 15
        };

        const uint8_t* ptr2 = data2.data();
        ComparisonExpressions obj2 = ComparisonExpressions::read(ptr2, ptr2 + data2.size());

        CHECK(obj2.eq() == true);
        CHECK(obj2.neq() == false);
        CHECK(obj2.lte() == true);
        CHECK(obj2.gte() == true);
    }

    TEST_CASE("LogicalExpressions - boolean logic") {
        std::vector<uint8_t> data = {
            0x01,  // flag1 = true
            0x00   // flag2 = false
        };

        const uint8_t* ptr = data.data();
        LogicalExpressions obj = LogicalExpressions::read(ptr, ptr + data.size());

        CHECK(obj.logic_and() == false);  // true && false
        CHECK(obj.logic_or() == true);    // true || false
        CHECK(obj.logic_not() == false);  // !true
    }

    TEST_CASE("TernaryExpressions - ternary operator") {
        std::vector<uint8_t> data = {
            150,  // value
            100   // threshold
        };

        const uint8_t* ptr = data.data();
        TernaryExpressions obj = TernaryExpressions::read(ptr, ptr + data.size());

        CHECK(obj.clamp() == 100);      // 150 > 100 ? 100 : 150
        CHECK(obj.max_value() == 150);  // 150 > 100 ? 150 : 100
        CHECK(obj.is_valid() == 0);     // 150 < 100 ? 1 : 0
    }

    TEST_CASE("PrecedenceTest - operator precedence") {
        std::vector<uint8_t> data = {
            2,   // a
            3,   // b
            4    // c
        };

        const uint8_t* ptr = data.data();
        PrecedenceTest obj = PrecedenceTest::read(ptr, ptr + data.size());

        CHECK(obj.test1() == 2 + 3 * 4);          // = 14
        CHECK(obj.test2() == (2 + 3) * 4);        // = 20
        CHECK(obj.test3() == (2 << (3 + 4)));     // = 256
        CHECK(obj.test4() == ((2 & 3) | 4));      // = 6
    }

    TEST_CASE("UnaryExpressions - unary operators") {
        std::vector<uint8_t> data = {
            0xF0, 0xFF  // value = -16
        };

        const uint8_t* ptr = data.data();
        UnaryExpressions obj = UnaryExpressions::read(ptr, ptr + data.size());

        CHECK(obj.negate() == 16);
        CHECK(obj.positive() == -16);
        CHECK(obj.abs() == 16);
    }

    TEST_CASE("FieldAccessExpressions - nested field access") {
        std::vector<uint8_t> data = {
            0x03, 0x00,  // p.x = 3
            0x04, 0x00   // p.y = 4
        };

        const uint8_t* ptr = data.data();
        FieldAccessExpressions obj = FieldAccessExpressions::read(ptr, ptr + data.size());

        CHECK(obj.get_x() == 3);
        CHECK(obj.distance_squared() == 3 * 3 + 4 * 4);  // = 25
    }

    TEST_CASE("ArrayIndexing - array element access") {
        std::vector<uint8_t> data = {
            10, 20, 30, 40
        };

        const uint8_t* ptr = data.data();
        ArrayIndexing obj = ArrayIndexing::read(ptr, ptr + data.size());

        CHECK(obj.get_first() == 10);
        CHECK(obj.get_last() == 40);
        CHECK(obj.sum() == 10 + 20 + 30 + 40);  // = 100
    }
}

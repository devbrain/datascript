#include <datascript/parser.hh>
#include <doctest/doctest.h>

/*
 * NOTE: These tests use array types in const declarations, which is semantically
 * meaningless. Arrays are meant for struct fields, function parameters, etc.
 *
 * These tests verify SYNTAX PARSING ONLY. Once structs are implemented, arrays
 * will have proper semantic context (e.g., "struct Header { uint8[4] magic; }").
 */

TEST_SUITE("Parser - Array Types") {

    // ========================================
    // Unsized Arrays (T[])
    // ========================================

    TEST_CASE("Unsized array of uint8") {
        auto mod = datascript::parse_datascript(std::string("const uint8[] X = 123;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is array_type_unsized
        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check element type is primitive_type (uint8)
        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->is_signed == false);
        CHECK(elem->bits == 8);
    }

    TEST_CASE("Unsized array of string") {
        auto mod = datascript::parse_datascript(std::string("const string[] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::string_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
    }

    TEST_CASE("Unsized array of bool") {
        auto mod = datascript::parse_datascript(std::string("const bool[] Z = 789;"));

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::bool_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
    }

    // ========================================
    // Fixed Size Arrays (T[size])
    // ========================================

    TEST_CASE("Fixed size array with integer literal") {
        auto mod = datascript::parse_datascript(std::string("const uint16[10] X = 123;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is array_type_fixed
        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check element type
        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->is_signed == false);
        CHECK(elem->bits == 16);

        // Check size expression
        auto* size_lit = std::get_if<datascript::ast::literal_int>(&arr->size.node);
        REQUIRE(size_lit != nullptr);
        CHECK(size_lit->value == 10);
    }

    TEST_CASE("Fixed size array with expression") {
        auto mod = datascript::parse_datascript(std::string("const int32[5 + 3] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check element type
        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->is_signed == true);
        CHECK(elem->bits == 32);

        // Check size expression is binary (addition)
        auto* size_bin = std::get_if<datascript::ast::binary_expr>(&arr->size.node);
        REQUIRE(size_bin != nullptr);
        CHECK(size_bin->op == datascript::ast::binary_op::add);
    }

    TEST_CASE("Fixed size array with hex literal") {
        auto mod = datascript::parse_datascript(std::string("const uint64[0x20] Z = 789;"));

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* size_lit = std::get_if<datascript::ast::literal_int>(&arr->size.node);
        REQUIRE(size_lit != nullptr);
        CHECK(size_lit->value == 0x20);
    }

    // ========================================
    // Range Arrays with Min and Max (T[min..max])
    // ========================================

    TEST_CASE("Range array with min and max") {
        auto mod = datascript::parse_datascript(std::string("const uint8[10..20] X = 123;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is array_type_range
        auto* arr = std::get_if<datascript::ast::array_type_range>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check element type
        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->is_signed == false);
        CHECK(elem->bits == 8);

        // Check min_size
        REQUIRE(arr->min_size.has_value());
        auto* min_lit = std::get_if<datascript::ast::literal_int>(&arr->min_size->node);
        REQUIRE(min_lit != nullptr);
        CHECK(min_lit->value == 10);

        // Check max_size
        auto* max_lit = std::get_if<datascript::ast::literal_int>(&arr->max_size.node);
        REQUIRE(max_lit != nullptr);
        CHECK(max_lit->value == 20);
    }

    TEST_CASE("Range array with complex expressions") {
        auto mod = datascript::parse_datascript(std::string("const uint16[5*2..100/2] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_range>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check min is multiplication
        REQUIRE(arr->min_size.has_value());
        auto* min_bin = std::get_if<datascript::ast::binary_expr>(&arr->min_size->node);
        REQUIRE(min_bin != nullptr);
        CHECK(min_bin->op == datascript::ast::binary_op::mul);

        // Check max is division
        auto* max_bin = std::get_if<datascript::ast::binary_expr>(&arr->max_size.node);
        REQUIRE(max_bin != nullptr);
        CHECK(max_bin->op == datascript::ast::binary_op::div);
    }

    // ========================================
    // Range Arrays with Only Max (T[..max])
    // ========================================

    TEST_CASE("Range array with only max") {
        auto mod = datascript::parse_datascript(std::string("const uint32[..100] X = 123;"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that type is array_type_range
        auto* arr = std::get_if<datascript::ast::array_type_range>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check element type
        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->is_signed == false);
        CHECK(elem->bits == 32);

        // Check min_size is not present
        CHECK(!arr->min_size.has_value());

        // Check max_size
        auto* max_lit = std::get_if<datascript::ast::literal_int>(&arr->max_size.node);
        REQUIRE(max_lit != nullptr);
        CHECK(max_lit->value == 100);
    }

    TEST_CASE("Range array with only max using expression") {
        auto mod = datascript::parse_datascript(std::string("const int64[..0x100] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_range>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        // Check min_size is not present
        CHECK(!arr->min_size.has_value());

        // Check max_size
        auto* max_lit = std::get_if<datascript::ast::literal_int>(&arr->max_size.node);
        REQUIRE(max_lit != nullptr);
        CHECK(max_lit->value == 0x100);
    }

    // ========================================
    // Arrays with Endianness
    // ========================================

    TEST_CASE("Array of little endian integers") {
        auto mod = datascript::parse_datascript(std::string("const little uint32[5] X = 123;"));

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->byte_order == datascript::ast::endian::little);
    }

    TEST_CASE("Array of big endian integers") {
        auto mod = datascript::parse_datascript(std::string("const big uint64[] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->byte_order == datascript::ast::endian::big);
    }

    // ========================================
    // Arrays of Bit Fields
    // ========================================

    TEST_CASE("Array of fixed bit fields") {
        auto mod = datascript::parse_datascript(std::string("const bit:5[10] X = 123;"));

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::bit_field_type_fixed>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->width == 5);
    }

    TEST_CASE("Array of dynamic bit fields") {
        auto mod = datascript::parse_datascript(std::string("const bit<8>[] Y = 456;"));

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::bit_field_type_expr>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
    }

    // ========================================
    // Multiple Array Definitions
    // ========================================

    TEST_CASE("Multiple array constants") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8[] A = 1;
            const uint16[10] B = 2;
            const uint32[5..10] C = 3;
            const uint64[..100] D = 4;
        )"));

        REQUIRE(mod.constants.size() == 4);

        // Check A - unsized
        CHECK(mod.constants[0].name == "A");
        auto* arr_a = std::get_if<datascript::ast::array_type_unsized>(&mod.constants[0].ctype.node);
        REQUIRE(arr_a != nullptr);

        // Check B - fixed
        CHECK(mod.constants[1].name == "B");
        auto* arr_b = std::get_if<datascript::ast::array_type_fixed>(&mod.constants[1].ctype.node);
        REQUIRE(arr_b != nullptr);

        // Check C - range with min and max
        CHECK(mod.constants[2].name == "C");
        auto* arr_c = std::get_if<datascript::ast::array_type_range>(&mod.constants[2].ctype.node);
        REQUIRE(arr_c != nullptr);
        CHECK(arr_c->min_size.has_value());

        // Check D - range with only max
        CHECK(mod.constants[3].name == "D");
        auto* arr_d = std::get_if<datascript::ast::array_type_range>(&mod.constants[3].ctype.node);
        REQUIRE(arr_d != nullptr);
        CHECK(!arr_d->min_size.has_value());
    }
}

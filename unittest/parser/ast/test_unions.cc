#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Union Definitions") {

    // ========================================
    // Empty Unions
    // ========================================

    TEST_CASE("Empty union") {
        auto mod = datascript::parse_datascript(std::string("union Empty {};"));

        REQUIRE(mod.unions.size() == 1);
        CHECK(mod.unions[0].name == "Empty");
        CHECK(mod.unions[0].cases.empty());
    }

    // ========================================
    // Unions with Simple Fields
    // ========================================

    TEST_CASE("Union with single field") {
        auto mod = datascript::parse_datascript(std::string("union Value { uint8 byte_val; };"));

        REQUIRE(mod.unions.size() == 1);
        CHECK(mod.unions[0].name == "Value");
        REQUIRE(mod.unions[0].cases.size() == 1);

        // Simple field becomes a single-field case
        auto& case0 = mod.unions[0].cases[0];
        CHECK(case0.case_name == "byte_val");
        CHECK(!case0.is_anonymous_block);
        REQUIRE(case0.items.size() == 1);

        auto& field = std::get<datascript::ast::field_def>(case0.items[0]);
        CHECK(field.name == "byte_val");
        CHECK(!field.condition.has_value());

        auto* prim = std::get_if<datascript::ast::primitive_type>(&field.field_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 8);
    }

    TEST_CASE("Union with multiple fields") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union Number {
                uint8 as_byte;
                uint16 as_word;
                uint32 as_dword;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        CHECK(mod.unions[0].name == "Number");
        REQUIRE(mod.unions[0].cases.size() == 3);

        // Each simple field becomes a single-field case
        CHECK(mod.unions[0].cases[0].case_name == "as_byte");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).name == "as_byte");
        CHECK(mod.unions[0].cases[1].case_name == "as_word");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).name == "as_word");
        CHECK(mod.unions[0].cases[2].case_name == "as_dword");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[2].items[0]).name == "as_dword");
    }

    TEST_CASE("Union with different types") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union Data {
                uint32 int_val;
                string str_val;
                bool bool_val;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 3);

        // Check uint32 field
        auto* prim0 = std::get_if<datascript::ast::primitive_type>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).field_type.node);
        REQUIRE(prim0 != nullptr);
        CHECK(prim0->bits == 32);

        // Check string field
        auto* str = std::get_if<datascript::ast::string_type>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).field_type.node);
        REQUIRE(str != nullptr);

        // Check bool field
        auto* b = std::get_if<datascript::ast::bool_type>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[2].items[0]).field_type.node);
        REQUIRE(b != nullptr);
    }

    // ========================================
    // Unions with Array Fields
    // ========================================

    TEST_CASE("Union with fixed array field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union Buffer {
                uint8[4] bytes;
                uint32 value;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 2);

        // Check array field
        auto& field0 = std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]);
        CHECK(field0.name == "bytes");

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&field0.field_type.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->bits == 8);
    }

    TEST_CASE("Union with unsized array field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union Payload {
                uint8[] data;
                string text;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 2);

        auto& field0 = std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]);
        CHECK(field0.name == "data");

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&field0.field_type.node);
        REQUIRE(arr != nullptr);
    }

    // ========================================
    // Unions with Conditional Fields
    // ========================================

    TEST_CASE("Union with conditional field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union OptionalData {
                uint8 flags;
                uint32 extended if flags > 0;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 2);

        // First field has no condition
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).name == "flags");
        CHECK(!std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).condition.has_value());

        // Second field has condition
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).name == "extended");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).condition.has_value());

        // Verify condition is a binary expression (>)
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).condition->node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::gt);
    }

    // ========================================
    // Unions with Bit Fields
    // ========================================

    TEST_CASE("Union with bit field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union Flags {
                bit:8 bits;
                uint8 byte;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 2);

        auto* bf = std::get_if<datascript::ast::bit_field_type_fixed>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).field_type.node);
        REQUIRE(bf != nullptr);
        CHECK(bf->width == 8);
    }

    // ========================================
    // Unions with Endianness
    // ========================================

    TEST_CASE("Union with endianness modifiers") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union MultiEndian {
                big uint32 big_val;
                little uint32 little_val;
                uint32 native_val;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        REQUIRE(mod.unions[0].cases.size() == 3);

        auto* prim0 = std::get_if<datascript::ast::primitive_type>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).field_type.node);
        REQUIRE(prim0 != nullptr);
        CHECK(prim0->byte_order == datascript::ast::endian::big);

        auto* prim1 = std::get_if<datascript::ast::primitive_type>(&std::get<datascript::ast::field_def>(mod.unions[0].cases[1].items[0]).field_type.node);
        REQUIRE(prim1 != nullptr);
        CHECK(prim1->byte_order == datascript::ast::endian::little);
    }

    // ========================================
    // Multiple Unions
    // ========================================

    TEST_CASE("Multiple union definitions") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union SmallValue {
                uint8 byte;
                int8 sbyte;
            };

            union LargeValue {
                uint64 qword;
                string text;
            };

            union Empty {};
        )"));

        REQUIRE(mod.unions.size() == 3);

        CHECK(mod.unions[0].name == "SmallValue");
        CHECK(mod.unions[0].cases.size() == 2);

        CHECK(mod.unions[1].name == "LargeValue");
        CHECK(mod.unions[1].cases.size() == 2);

        CHECK(mod.unions[2].name == "Empty");
        CHECK(mod.unions[2].cases.size() == 0);
    }

    // ========================================
    // Unions Mixed with Other Definitions
    // ========================================

    TEST_CASE("Union with constants and structs") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint32 MAGIC = 0xDEADBEEF;

            struct Header {
                uint32 magic;
                uint8 type;
            };

            union Value {
                uint32 int_val;
                string str_val;
            };
        )"));

        CHECK(mod.constants.size() == 1);
        CHECK(mod.structs.size() == 1);
        REQUIRE(mod.unions.size() == 1);

        CHECK(mod.unions[0].name == "Value");
        CHECK(mod.unions[0].cases.size() == 2);
    }

    // ========================================
    // Complex Union Example
    // ========================================

    TEST_CASE("Complex union with all features") {
        auto mod = datascript::parse_datascript(std::string(R"(
            union ComplexData {
                uint8[4] bytes;
                big uint32 big_int;
                uint8 flags;
                string text if flags & 1;
                bit:16 bits;
                little uint64 timestamp;
                uint8[] payload;
            };
        )"));

        REQUIRE(mod.unions.size() == 1);
        CHECK(mod.unions[0].name == "ComplexData");
        REQUIRE(mod.unions[0].cases.size() == 7);

        // Verify various field types and conditions
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[0].items[0]).name == "bytes");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[2].items[0]).name == "flags");
        CHECK(std::get<datascript::ast::field_def>(mod.unions[0].cases[3].items[0]).condition.has_value());
    }
}

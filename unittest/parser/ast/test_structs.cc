#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Struct Definitions") {

    // ========================================
    // Empty Structs
    // ========================================

    TEST_CASE("Empty struct") {
        auto mod = datascript::parse_datascript(std::string("struct Empty {};"));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "Empty");
        CHECK(mod.structs[0].body.empty());
    }

    // ========================================
    // Structs with Simple Fields
    // ========================================

    TEST_CASE("Struct with single field") {
        auto mod = datascript::parse_datascript(std::string("struct Header { uint8 magic; };"));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "Header");
        REQUIRE(mod.structs[0].body.size() == 1);

        auto* field = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field != nullptr);
        CHECK(field->name == "magic");
        CHECK(!field->condition.has_value());

        auto* prim = std::get_if<datascript::ast::primitive_type>(&field->field_type.node);
        REQUIRE(prim != nullptr);
        CHECK(prim->is_signed == false);
        CHECK(prim->bits == 8);
    }

    TEST_CASE("Struct with multiple fields") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Point {
                int32 x;
                int32 y;
                int32 z;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "Point");
        REQUIRE(mod.structs[0].body.size() == 3);

        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(field0->name == "x");

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "y");

        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[2]);
        REQUIRE(field2 != nullptr);
        CHECK(field2->name == "z");
    }

    TEST_CASE("Struct with different types") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Record {
                uint8 type;
                uint32 length;
                string name;
                bool active;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 4);

        // Check uint8 field
        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        auto* prim0 = std::get_if<datascript::ast::primitive_type>(&field0->field_type.node);
        REQUIRE(prim0 != nullptr);
        CHECK(prim0->bits == 8);

        // Check uint32 field
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        auto* prim1 = std::get_if<datascript::ast::primitive_type>(&field1->field_type.node);
        REQUIRE(prim1 != nullptr);
        CHECK(prim1->bits == 32);

        // Check string field
        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[2]);
        REQUIRE(field2 != nullptr);
        auto* str = std::get_if<datascript::ast::string_type>(&field2->field_type.node);
        REQUIRE(str != nullptr);

        // Check bool field
        auto* field3 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[3]);
        REQUIRE(field3 != nullptr);
        auto* b = std::get_if<datascript::ast::bool_type>(&field3->field_type.node);
        REQUIRE(b != nullptr);
    }

    // ========================================
    // Structs with Array Fields
    // ========================================

    TEST_CASE("Struct with fixed array field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Header {
                uint8[4] magic;
                uint16 version;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // Check array field
        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(field0->name == "magic");

        auto* arr = std::get_if<datascript::ast::array_type_fixed>(&field0->field_type.node);
        REQUIRE(arr != nullptr);

        auto* elem = std::get_if<datascript::ast::primitive_type>(&arr->element_type->node);
        REQUIRE(elem != nullptr);
        CHECK(elem->bits == 8);
    }

    TEST_CASE("Struct with unsized array field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Packet {
                uint32 length;
                uint8[] data;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "data");

        auto* arr = std::get_if<datascript::ast::array_type_unsized>(&field1->field_type.node);
        REQUIRE(arr != nullptr);
    }

    TEST_CASE("Struct with range array field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Message {
                uint16[10..100] items;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 1);

        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        auto* arr = std::get_if<datascript::ast::array_type_range>(&field0->field_type.node);
        REQUIRE(arr != nullptr);
        CHECK(arr->min_size.has_value());
    }

    // ========================================
    // Structs with Conditional Fields
    // ========================================

    TEST_CASE("Struct with conditional field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct OptionalHeader {
                uint8 flags;
                uint32 extended if flags > 0;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // First field has no condition
        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(field0->name == "flags");
        CHECK(!field0->condition.has_value());

        // Second field has condition
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "extended");
        CHECK(field1->condition.has_value());

        // Verify condition is a binary expression (>)
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&field1->condition->node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::gt);
    }

    TEST_CASE("Struct with multiple conditional fields") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Complex {
                uint8 type;
                uint32 size if type == 1;
                string name if type == 2;
                uint64 value if type == 3;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 4);

        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(!field0->condition.has_value());

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->condition.has_value());

        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[2]);
        REQUIRE(field2 != nullptr);
        CHECK(field2->condition.has_value());

        auto* field3 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[3]);
        REQUIRE(field3 != nullptr);
        CHECK(field3->condition.has_value());
    }

    // ========================================
    // Structs with Bit Fields
    // ========================================

    TEST_CASE("Struct with bit field") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Flags {
                bit:1 enabled;
                bit:3 level;
                bit:4 reserved;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 3);

        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        auto* bf0 = std::get_if<datascript::ast::bit_field_type_fixed>(&field0->field_type.node);
        REQUIRE(bf0 != nullptr);
        CHECK(bf0->width == 1);

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        auto* bf1 = std::get_if<datascript::ast::bit_field_type_fixed>(&field1->field_type.node);
        REQUIRE(bf1 != nullptr);
        CHECK(bf1->width == 3);
    }

    // ========================================
    // Structs with Endianness
    // ========================================

    TEST_CASE("Struct with endianness modifiers") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Network {
                big uint32 magic;
                little uint16 version;
                uint8 flags;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 3);

        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        auto* prim0 = std::get_if<datascript::ast::primitive_type>(&field0->field_type.node);
        REQUIRE(prim0 != nullptr);
        CHECK(prim0->byte_order == datascript::ast::endian::big);

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        auto* prim1 = std::get_if<datascript::ast::primitive_type>(&field1->field_type.node);
        REQUIRE(prim1 != nullptr);
        CHECK(prim1->byte_order == datascript::ast::endian::little);
    }

    // ========================================
    // Multiple Structs
    // ========================================

    TEST_CASE("Multiple struct definitions") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Point {
                int32 x;
                int32 y;
            };

            struct Line {
                uint32 id;
                string label;
            };

            struct Empty {};
        )"));

        REQUIRE(mod.structs.size() == 3);

        CHECK(mod.structs[0].name == "Point");
        CHECK(mod.structs[0].body.size() == 2);

        CHECK(mod.structs[1].name == "Line");
        CHECK(mod.structs[1].body.size() == 2);

        CHECK(mod.structs[2].name == "Empty");
        CHECK(mod.structs[2].body.size() == 0);
    }

    // ========================================
    // Structs Mixed with Other Definitions
    // ========================================

    TEST_CASE("Struct with constants and enums") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint32 MAGIC = 0xDEADBEEF;

            enum uint8 Type {
                TYPE_A,
                TYPE_B
            };

            struct Header {
                uint32 magic;
                uint8 type;
            };
        )"));

        CHECK(mod.constants.size() == 1);
        CHECK(mod.enums.size() == 1);
        REQUIRE(mod.structs.size() == 1);

        CHECK(mod.structs[0].name == "Header");
        CHECK(mod.structs[0].body.size() == 2);
    }

    // ========================================
    // Complex Struct Examples
    // ========================================

    TEST_CASE("Complex struct with all features") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct ComplexPacket {
                uint8[4] magic;
                big uint32 length;
                uint8 flags;
                string name if flags & 1;
                uint16[..100] items if flags & 2;
                bit:5 version;
                bit:3 reserved;
                little uint64 timestamp;
                uint8[] payload;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "ComplexPacket");
        REQUIRE(mod.structs[0].body.size() == 9);

        // Verify various field types and conditions
        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(field0->name == "magic");

        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[2]);
        REQUIRE(field2 != nullptr);
        CHECK(field2->name == "flags");

        auto* field3 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[3]);
        REQUIRE(field3 != nullptr);
        CHECK(field3->condition.has_value());

        auto* field4 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[4]);
        REQUIRE(field4 != nullptr);
        CHECK(field4->condition.has_value());
    }
}

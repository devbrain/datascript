#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Field Access Expressions") {

    // ========================================
    // Simple Field Access
    // ========================================

    TEST_CASE("Simple field access in constant") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = obj.field;
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that value is a field access expression
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&mod.constants[0].value.node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "field");

        // Check that object is an identifier
        auto* obj_id = std::get_if<datascript::ast::identifier>(&field_access->object->node);
        REQUIRE(obj_id != nullptr);
        CHECK(obj_id->name == "obj");
    }

    // ========================================
    // Nested Field Access
    // ========================================

    TEST_CASE("Nested field access - two levels") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = packet.header.magic;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outermost should be field access to "magic"
        auto* outer_access = std::get_if<datascript::ast::field_access_expr>(&mod.constants[0].value.node);
        REQUIRE(outer_access != nullptr);
        CHECK(outer_access->field_name == "magic");

        // Inner should be field access to "header"
        auto* inner_access = std::get_if<datascript::ast::field_access_expr>(&outer_access->object->node);
        REQUIRE(inner_access != nullptr);
        CHECK(inner_access->field_name == "header");

        // Innermost should be identifier "packet"
        auto* id = std::get_if<datascript::ast::identifier>(&inner_access->object->node);
        REQUIRE(id != nullptr);
        CHECK(id->name == "packet");
    }

    TEST_CASE("Deeply nested field access - three levels") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = msg.packet.header.flags;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Level 1: flags
        auto* level1 = std::get_if<datascript::ast::field_access_expr>(&mod.constants[0].value.node);
        REQUIRE(level1 != nullptr);
        CHECK(level1->field_name == "flags");

        // Level 2: header
        auto* level2 = std::get_if<datascript::ast::field_access_expr>(&level1->object->node);
        REQUIRE(level2 != nullptr);
        CHECK(level2->field_name == "header");

        // Level 3: packet
        auto* level3 = std::get_if<datascript::ast::field_access_expr>(&level2->object->node);
        REQUIRE(level3 != nullptr);
        CHECK(level3->field_name == "packet");

        // Base: msg
        auto* base = std::get_if<datascript::ast::identifier>(&level3->object->node);
        REQUIRE(base != nullptr);
        CHECK(base->name == "msg");
    }

    // ========================================
    // Field Access in Expressions
    // ========================================

    TEST_CASE("Field access in binary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = header.flags & 0x01;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Should be a binary expression
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::bit_and);

        // Left side should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&bin_expr->left->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "flags");
    }

    TEST_CASE("Field access in comparison") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const bool X = packet.type == 1;
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::eq);

        // Left side should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&bin_expr->left->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "type");
    }

    TEST_CASE("Multiple field accesses in expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = header.flags + packet.type;
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::add);

        // Both sides should be field access
        auto* left_access = std::get_if<datascript::ast::field_access_expr>(&bin_expr->left->node);
        REQUIRE(left_access != nullptr);
        CHECK(left_access->field_name == "flags");

        auto* right_access = std::get_if<datascript::ast::field_access_expr>(&bin_expr->right->node);
        REQUIRE(right_access != nullptr);
        CHECK(right_access->field_name == "type");
    }

    // ========================================
    // Field Access in Struct Conditionals
    // ========================================

    TEST_CASE("Field access in struct conditional") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Packet {
                uint8 type;
                uint32 data if type > 0;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // Second field should have condition
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->condition.has_value());

        // Condition should contain comparison with identifier "type"
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&field1->condition->node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::gt);
    }

    TEST_CASE("Field access with nested object in struct conditional") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Message {
                uint8 flags;
                uint32 extended if header.flags & 0x01;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // Second field should have condition with field access
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->condition.has_value());

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&field1->condition->node);
        REQUIRE(bin_expr != nullptr);

        // Left side should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&bin_expr->left->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "flags");
    }

    // ========================================
    // Field Access in Choice Selectors
    // ========================================

    TEST_CASE("Field access in choice selector") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on header.type {
                case 1: uint8 byte;
                case 2: string text;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);

        // Selector should be field access
        REQUIRE(mod.choices[0].selector.has_value());
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&mod.choices[0].selector.value().node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "type");

        auto* obj_id = std::get_if<datascript::ast::identifier>(&field_access->object->node);
        REQUIRE(obj_id != nullptr);
        CHECK(obj_id->name == "header");
    }

    TEST_CASE("Nested field access in choice selector") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Payload on packet.header.type {
                case 1: uint8 data;
                default: uint8[] raw;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);

        // Selector should be nested field access
        REQUIRE(mod.choices[0].selector.has_value());
        auto* outer = std::get_if<datascript::ast::field_access_expr>(&mod.choices[0].selector.value().node);
        REQUIRE(outer != nullptr);
        CHECK(outer->field_name == "type");

        auto* inner = std::get_if<datascript::ast::field_access_expr>(&outer->object->node);
        REQUIRE(inner != nullptr);
        CHECK(inner->field_name == "header");
    }

    // ========================================
    // Field Access in Ternary Expressions
    // ========================================

    TEST_CASE("Field access in ternary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = header.flags > 0 ? header.type : 0;
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constants[0].value.node);
        REQUIRE(ternary != nullptr);

        // Condition should have field access
        auto* cond_bin = std::get_if<datascript::ast::binary_expr>(&ternary->condition->node);
        REQUIRE(cond_bin != nullptr);

        auto* cond_access = std::get_if<datascript::ast::field_access_expr>(&cond_bin->left->node);
        REQUIRE(cond_access != nullptr);
        CHECK(cond_access->field_name == "flags");

        // True branch should have field access
        auto* true_access = std::get_if<datascript::ast::field_access_expr>(&ternary->true_expr->node);
        REQUIRE(true_access != nullptr);
        CHECK(true_access->field_name == "type");
    }

    // ========================================
    // Field Access in Complex Expressions
    // ========================================

    TEST_CASE("Field access with arithmetic") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint32 X = packet.length + header.size * 2;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Top-level should be addition
        auto* add_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(add_expr != nullptr);
        CHECK(add_expr->op == datascript::ast::binary_op::add);

        // Left should be field access
        auto* left_access = std::get_if<datascript::ast::field_access_expr>(&add_expr->left->node);
        REQUIRE(left_access != nullptr);
        CHECK(left_access->field_name == "length");

        // Right should be multiplication with field access on left
        auto* mul_expr = std::get_if<datascript::ast::binary_expr>(&add_expr->right->node);
        REQUIRE(mul_expr != nullptr);
        CHECK(mul_expr->op == datascript::ast::binary_op::mul);

        auto* mul_left_access = std::get_if<datascript::ast::field_access_expr>(&mul_expr->left->node);
        REQUIRE(mul_left_access != nullptr);
        CHECK(mul_left_access->field_name == "size");
    }
}

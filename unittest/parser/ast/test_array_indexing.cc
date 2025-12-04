#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Array Indexing Expressions") {

    // ========================================
    // Simple Array Indexing
    // ========================================

    TEST_CASE("Simple array indexing in constant") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[0];
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that value is an array index expression
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Check that array is an identifier
        auto* arr_id = std::get_if<datascript::ast::identifier>(&array_index->array->node);
        REQUIRE(arr_id != nullptr);
        CHECK(arr_id->name == "arr");

        // Check that index is a literal
        auto* index_lit = std::get_if<datascript::ast::literal_int>(&array_index->index->node);
        REQUIRE(index_lit != nullptr);
        CHECK(index_lit->value == 0);
    }

    TEST_CASE("Array indexing with variable index") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = data[i];
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Index should be an identifier
        auto* index_id = std::get_if<datascript::ast::identifier>(&array_index->index->node);
        REQUIRE(index_id != nullptr);
        CHECK(index_id->name == "i");
    }

    // ========================================
    // Array Indexing with Expressions
    // ========================================

    TEST_CASE("Array indexing with arithmetic expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[i + 1];
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Index should be a binary expression
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&array_index->index->node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::add);
    }

    TEST_CASE("Array indexing with complex expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = data[offset * 2 + base];
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Index should be a binary expression (addition at top level)
        auto* add_expr = std::get_if<datascript::ast::binary_expr>(&array_index->index->node);
        REQUIRE(add_expr != nullptr);
        CHECK(add_expr->op == datascript::ast::binary_op::add);
    }

    // ========================================
    // Nested Array Indexing
    // ========================================

    TEST_CASE("Multi-dimensional array access") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = matrix[i][j];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be array indexing with [j]
        auto* outer_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(outer_index != nullptr);

        auto* j_id = std::get_if<datascript::ast::identifier>(&outer_index->index->node);
        REQUIRE(j_id != nullptr);
        CHECK(j_id->name == "j");

        // Inner should be array indexing with [i]
        auto* inner_index = std::get_if<datascript::ast::array_index_expr>(&outer_index->array->node);
        REQUIRE(inner_index != nullptr);

        auto* i_id = std::get_if<datascript::ast::identifier>(&inner_index->index->node);
        REQUIRE(i_id != nullptr);
        CHECK(i_id->name == "i");

        // Base should be identifier "matrix"
        auto* base_id = std::get_if<datascript::ast::identifier>(&inner_index->array->node);
        REQUIRE(base_id != nullptr);
        CHECK(base_id->name == "matrix");
    }

    TEST_CASE("Three-dimensional array access") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = cube[x][y][z];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Level 1: [z]
        auto* level1 = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(level1 != nullptr);

        // Level 2: [y]
        auto* level2 = std::get_if<datascript::ast::array_index_expr>(&level1->array->node);
        REQUIRE(level2 != nullptr);

        // Level 3: [x]
        auto* level3 = std::get_if<datascript::ast::array_index_expr>(&level2->array->node);
        REQUIRE(level3 != nullptr);

        // Base: cube
        auto* base = std::get_if<datascript::ast::identifier>(&level3->array->node);
        REQUIRE(base != nullptr);
        CHECK(base->name == "cube");
    }

    // ========================================
    // Array Indexing in Expressions
    // ========================================

    TEST_CASE("Array indexing in binary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[0] + arr[1];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Should be a binary expression
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::add);

        // Both sides should be array indexing
        auto* left_index = std::get_if<datascript::ast::array_index_expr>(&bin_expr->left->node);
        REQUIRE(left_index != nullptr);

        auto* right_index = std::get_if<datascript::ast::array_index_expr>(&bin_expr->right->node);
        REQUIRE(right_index != nullptr);
    }

    TEST_CASE("Array indexing in comparison") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const bool X = data[0] == 0xFF;
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::eq);

        // Left side should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&bin_expr->left->node);
        REQUIRE(array_index != nullptr);
    }

    // ========================================
    // Combining Field Access and Array Indexing
    // ========================================

    TEST_CASE("Field access then array indexing") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = obj.arr[0];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Array should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&array_index->array->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "arr");

        auto* obj_id = std::get_if<datascript::ast::identifier>(&field_access->object->node);
        REQUIRE(obj_id != nullptr);
        CHECK(obj_id->name == "obj");
    }

    TEST_CASE("Array indexing then field access") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[0].field;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&mod.constants[0].value.node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "field");

        // Object should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&field_access->object->node);
        REQUIRE(array_index != nullptr);

        auto* arr_id = std::get_if<datascript::ast::identifier>(&array_index->array->node);
        REQUIRE(arr_id != nullptr);
        CHECK(arr_id->name == "arr");
    }

    TEST_CASE("Complex postfix chain") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = packet.data[i].header.flags[0];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outermost: [0]
        auto* idx2 = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(idx2 != nullptr);

        // Then: .flags
        auto* field2 = std::get_if<datascript::ast::field_access_expr>(&idx2->array->node);
        REQUIRE(field2 != nullptr);
        CHECK(field2->field_name == "flags");

        // Then: .header
        auto* field1 = std::get_if<datascript::ast::field_access_expr>(&field2->object->node);
        REQUIRE(field1 != nullptr);
        CHECK(field1->field_name == "header");

        // Then: [i]
        auto* idx1 = std::get_if<datascript::ast::array_index_expr>(&field1->object->node);
        REQUIRE(idx1 != nullptr);

        // Then: .data
        auto* field0 = std::get_if<datascript::ast::field_access_expr>(&idx1->array->node);
        REQUIRE(field0 != nullptr);
        CHECK(field0->field_name == "data");

        // Base: packet
        auto* base = std::get_if<datascript::ast::identifier>(&field0->object->node);
        REQUIRE(base != nullptr);
        CHECK(base->name == "packet");
    }

    // ========================================
    // Array Indexing in Struct Conditionals
    // ========================================

    TEST_CASE("Array indexing in struct conditional") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Header {
                uint8[4] magic;
                uint32 data if magic[0] == 0xFF;
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // Second field should have condition with array indexing
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->condition.has_value());

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&field1->condition->node);
        REQUIRE(bin_expr != nullptr);

        // Left side should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&bin_expr->left->node);
        REQUIRE(array_index != nullptr);
    }

    // ========================================
    // Array Indexing in Choice Selectors
    // ========================================

    TEST_CASE("Array indexing in choice selector") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on data[0] {
                case 1: uint8 byte;
                case 2: string text;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);

        // Selector should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.choices[0].selector.node);
        REQUIRE(array_index != nullptr);

        auto* data_id = std::get_if<datascript::ast::identifier>(&array_index->array->node);
        REQUIRE(data_id != nullptr);
        CHECK(data_id->name == "data");
    }

    // ========================================
    // Array Indexing in Ternary Expressions
    // ========================================

    TEST_CASE("Array indexing in ternary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[0] > 0 ? arr[1] : arr[2];
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constants[0].value.node);
        REQUIRE(ternary != nullptr);

        // Condition should have array indexing
        auto* cond_bin = std::get_if<datascript::ast::binary_expr>(&ternary->condition->node);
        REQUIRE(cond_bin != nullptr);

        auto* cond_index = std::get_if<datascript::ast::array_index_expr>(&cond_bin->left->node);
        REQUIRE(cond_index != nullptr);

        // Both branches should have array indexing
        auto* true_index = std::get_if<datascript::ast::array_index_expr>(&ternary->true_expr->node);
        REQUIRE(true_index != nullptr);

        auto* false_index = std::get_if<datascript::ast::array_index_expr>(&ternary->false_expr->node);
        REQUIRE(false_index != nullptr);
    }
}

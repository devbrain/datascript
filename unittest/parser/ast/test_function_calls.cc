#include <datascript/parser.hh>
#include <doctest/doctest.h>

TEST_SUITE("Parser - Function Call Expressions") {

    // ========================================
    // Simple Function Calls
    // ========================================

    TEST_CASE("Function call with no arguments") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = foo();
        )"));

        REQUIRE(mod.constants.size() == 1);
        CHECK(mod.constants[0].name == "X");

        // Check that value is a function call expression
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that function is an identifier
        auto* func_id = std::get_if<datascript::ast::identifier>(&func_call->function->node);
        REQUIRE(func_id != nullptr);
        CHECK(func_id->name == "foo");

        // Check that there are no arguments
        CHECK(func_call->arguments.size() == 0);
    }

    TEST_CASE("Function call with one argument") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = sizeof(data);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that function is an identifier
        auto* func_id = std::get_if<datascript::ast::identifier>(&func_call->function->node);
        REQUIRE(func_id != nullptr);
        CHECK(func_id->name == "sizeof");

        // Check that there is one argument
        REQUIRE(func_call->arguments.size() == 1);

        // Argument should be an identifier
        auto* arg_id = std::get_if<datascript::ast::identifier>(&func_call->arguments[0].node);
        REQUIRE(arg_id != nullptr);
        CHECK(arg_id->name == "data");
    }

    TEST_CASE("Function call with literal argument") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = process(42);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that there is one argument
        REQUIRE(func_call->arguments.size() == 1);

        // Argument should be a literal
        auto* arg_lit = std::get_if<datascript::ast::literal_int>(&func_call->arguments[0].node);
        REQUIRE(arg_lit != nullptr);
        CHECK(arg_lit->value == 42);
    }

    // ========================================
    // Multiple Arguments
    // ========================================

    TEST_CASE("Function call with two arguments") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = add(a, b);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that there are two arguments
        REQUIRE(func_call->arguments.size() == 2);

        // First argument
        auto* arg1 = std::get_if<datascript::ast::identifier>(&func_call->arguments[0].node);
        REQUIRE(arg1 != nullptr);
        CHECK(arg1->name == "a");

        // Second argument
        auto* arg2 = std::get_if<datascript::ast::identifier>(&func_call->arguments[1].node);
        REQUIRE(arg2 != nullptr);
        CHECK(arg2->name == "b");
    }

    TEST_CASE("Function call with three arguments") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = clamp(value, min, max);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that there are three arguments
        REQUIRE(func_call->arguments.size() == 3);

        auto* arg1 = std::get_if<datascript::ast::identifier>(&func_call->arguments[0].node);
        REQUIRE(arg1 != nullptr);
        CHECK(arg1->name == "value");

        auto* arg2 = std::get_if<datascript::ast::identifier>(&func_call->arguments[1].node);
        REQUIRE(arg2 != nullptr);
        CHECK(arg2->name == "min");

        auto* arg3 = std::get_if<datascript::ast::identifier>(&func_call->arguments[2].node);
        REQUIRE(arg3 != nullptr);
        CHECK(arg3->name == "max");
    }

    TEST_CASE("Function call with expression arguments") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = max(a + 1, b * 2);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Check that there are two arguments
        REQUIRE(func_call->arguments.size() == 2);

        // First argument should be a binary expression (a + 1)
        auto* arg1 = std::get_if<datascript::ast::binary_expr>(&func_call->arguments[0].node);
        REQUIRE(arg1 != nullptr);
        CHECK(arg1->op == datascript::ast::binary_op::add);

        // Second argument should be a binary expression (b * 2)
        auto* arg2 = std::get_if<datascript::ast::binary_expr>(&func_call->arguments[1].node);
        REQUIRE(arg2 != nullptr);
        CHECK(arg2->op == datascript::ast::binary_op::mul);
    }

    // ========================================
    // Nested Function Calls
    // ========================================

    TEST_CASE("Nested function calls") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = outer(inner(value));
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer function call
        auto* outer_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(outer_call != nullptr);

        auto* outer_id = std::get_if<datascript::ast::identifier>(&outer_call->function->node);
        REQUIRE(outer_id != nullptr);
        CHECK(outer_id->name == "outer");

        REQUIRE(outer_call->arguments.size() == 1);

        // Inner function call
        auto* inner_call = std::get_if<datascript::ast::function_call_expr>(&outer_call->arguments[0].node);
        REQUIRE(inner_call != nullptr);

        auto* inner_id = std::get_if<datascript::ast::identifier>(&inner_call->function->node);
        REQUIRE(inner_id != nullptr);
        CHECK(inner_id->name == "inner");

        REQUIRE(inner_call->arguments.size() == 1);

        auto* value_id = std::get_if<datascript::ast::identifier>(&inner_call->arguments[0].node);
        REQUIRE(value_id != nullptr);
        CHECK(value_id->name == "value");
    }

    TEST_CASE("Multiple nested function calls") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = f(g(a), h(b));
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* f_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(f_call != nullptr);

        REQUIRE(f_call->arguments.size() == 2);

        // First argument is g(a)
        auto* g_call = std::get_if<datascript::ast::function_call_expr>(&f_call->arguments[0].node);
        REQUIRE(g_call != nullptr);

        // Second argument is h(b)
        auto* h_call = std::get_if<datascript::ast::function_call_expr>(&f_call->arguments[1].node);
        REQUIRE(h_call != nullptr);
    }

    // ========================================
    // Combining Function Calls with Field Access
    // ========================================

    TEST_CASE("Field access then function call") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = obj.method();
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Function should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&func_call->function->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "method");

        auto* obj_id = std::get_if<datascript::ast::identifier>(&field_access->object->node);
        REQUIRE(obj_id != nullptr);
        CHECK(obj_id->name == "obj");

        // No arguments
        CHECK(func_call->arguments.size() == 0);
    }

    TEST_CASE("Field access then function call with arguments") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = obj.method(a, b);
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Function should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&func_call->function->node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "method");

        // Two arguments
        REQUIRE(func_call->arguments.size() == 2);
    }

    TEST_CASE("Function call then field access") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = getObj().field;
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be field access
        auto* field_access = std::get_if<datascript::ast::field_access_expr>(&mod.constants[0].value.node);
        REQUIRE(field_access != nullptr);
        CHECK(field_access->field_name == "field");

        // Object should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&field_access->object->node);
        REQUIRE(func_call != nullptr);

        auto* func_id = std::get_if<datascript::ast::identifier>(&func_call->function->node);
        REQUIRE(func_id != nullptr);
        CHECK(func_id->name == "getObj");
    }

    // ========================================
    // Combining Function Calls with Array Indexing
    // ========================================

    TEST_CASE("Array indexing then function call") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = arr[0]();
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);

        // Function should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&func_call->function->node);
        REQUIRE(array_index != nullptr);

        auto* arr_id = std::get_if<datascript::ast::identifier>(&array_index->array->node);
        REQUIRE(arr_id != nullptr);
        CHECK(arr_id->name == "arr");
    }

    TEST_CASE("Function call then array indexing") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = getArray()[0];
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outer should be array indexing
        auto* array_index = std::get_if<datascript::ast::array_index_expr>(&mod.constants[0].value.node);
        REQUIRE(array_index != nullptr);

        // Array should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&array_index->array->node);
        REQUIRE(func_call != nullptr);
    }

    TEST_CASE("Complex postfix chain with function call") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = obj.getArray()[0].method(a, b);
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Outermost: .method(a, b)
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.constants[0].value.node);
        REQUIRE(func_call != nullptr);
        REQUIRE(func_call->arguments.size() == 2);

        // Function should be field access to "method"
        auto* method_field = std::get_if<datascript::ast::field_access_expr>(&func_call->function->node);
        REQUIRE(method_field != nullptr);
        CHECK(method_field->field_name == "method");

        // Object should be array indexing [0]
        auto* idx = std::get_if<datascript::ast::array_index_expr>(&method_field->object->node);
        REQUIRE(idx != nullptr);

        // Array should be function call getArray()
        auto* get_array = std::get_if<datascript::ast::function_call_expr>(&idx->array->node);
        REQUIRE(get_array != nullptr);

        // Function should be field access obj.getArray
        auto* get_array_field = std::get_if<datascript::ast::field_access_expr>(&get_array->function->node);
        REQUIRE(get_array_field != nullptr);
        CHECK(get_array_field->field_name == "getArray");

        // Base should be identifier "obj"
        auto* obj_id = std::get_if<datascript::ast::identifier>(&get_array_field->object->node);
        REQUIRE(obj_id != nullptr);
        CHECK(obj_id->name == "obj");
    }

    // ========================================
    // Function Calls in Expressions
    // ========================================

    TEST_CASE("Function call in binary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = foo() + bar();
        )"));

        REQUIRE(mod.constants.size() == 1);

        // Should be a binary expression
        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::add);

        // Both sides should be function calls
        auto* left_call = std::get_if<datascript::ast::function_call_expr>(&bin_expr->left->node);
        REQUIRE(left_call != nullptr);

        auto* right_call = std::get_if<datascript::ast::function_call_expr>(&bin_expr->right->node);
        REQUIRE(right_call != nullptr);
    }

    TEST_CASE("Function call in comparison") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const bool X = getSize() > 0;
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* bin_expr = std::get_if<datascript::ast::binary_expr>(&mod.constants[0].value.node);
        REQUIRE(bin_expr != nullptr);
        CHECK(bin_expr->op == datascript::ast::binary_op::gt);

        // Left side should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&bin_expr->left->node);
        REQUIRE(func_call != nullptr);
    }

    // ========================================
    // Function Calls in Struct Conditionals
    // ========================================

    TEST_CASE("Function call in struct conditional") {
        auto mod = datascript::parse_datascript(std::string(R"(
            struct Header {
                uint8 type;
                uint32 data if validate(type);
            };
        )"));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        // Second field should have condition with function call
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->condition.has_value());

        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&field1->condition->node);
        REQUIRE(func_call != nullptr);

        auto* func_id = std::get_if<datascript::ast::identifier>(&func_call->function->node);
        REQUIRE(func_id != nullptr);
        CHECK(func_id->name == "validate");
    }

    // ========================================
    // Function Calls in Choice Selectors
    // ========================================

    TEST_CASE("Function call in choice selector") {
        auto mod = datascript::parse_datascript(std::string(R"(
            choice Message on getType() {
                case 1: uint8 byte;
                case 2: string text;
            };
        )"));

        REQUIRE(mod.choices.size() == 1);

        // Selector should be function call
        auto* func_call = std::get_if<datascript::ast::function_call_expr>(&mod.choices[0].selector.node);
        REQUIRE(func_call != nullptr);

        auto* func_id = std::get_if<datascript::ast::identifier>(&func_call->function->node);
        REQUIRE(func_id != nullptr);
        CHECK(func_id->name == "getType");
    }

    // ========================================
    // Function Calls in Ternary Expressions
    // ========================================

    TEST_CASE("Function call in ternary expression") {
        auto mod = datascript::parse_datascript(std::string(R"(
            const uint8 X = check() ? getA() : getB();
        )"));

        REQUIRE(mod.constants.size() == 1);

        auto* ternary = std::get_if<datascript::ast::ternary_expr>(&mod.constants[0].value.node);
        REQUIRE(ternary != nullptr);

        // Condition should be function call
        auto* cond_call = std::get_if<datascript::ast::function_call_expr>(&ternary->condition->node);
        REQUIRE(cond_call != nullptr);

        // True branch should be function call
        auto* true_call = std::get_if<datascript::ast::function_call_expr>(&ternary->true_expr->node);
        REQUIRE(true_call != nullptr);

        // False branch should be function call
        auto* false_call = std::get_if<datascript::ast::function_call_expr>(&ternary->false_expr->node);
        REQUIRE(false_call != nullptr);
    }
}

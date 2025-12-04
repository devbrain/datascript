//
// Tests for Command Builder
//

#include <doctest/doctest.h>
#include <datascript/command_builder.hh>

using namespace datascript;
using namespace datascript::codegen;

TEST_SUITE("Codegen - Command Builder") {

    // ========================================================================
    // Helper Functions
    // ========================================================================

    ir::type_ref make_primitive_type(bool is_signed, int bit_size) {
        ir::type_ref type;
        // Map to actual type_kind enum values
        if (is_signed) {
            if (bit_size == 8) type.kind = ir::type_kind::int8;
            else if (bit_size == 16) type.kind = ir::type_kind::int16;
            else if (bit_size == 32) type.kind = ir::type_kind::int32;
            else if (bit_size == 64) type.kind = ir::type_kind::int64;
            else type.kind = ir::type_kind::int128;
        } else {
            if (bit_size == 8) type.kind = ir::type_kind::uint8;
            else if (bit_size == 16) type.kind = ir::type_kind::uint16;
            else if (bit_size == 32) type.kind = ir::type_kind::uint32;
            else if (bit_size == 64) type.kind = ir::type_kind::uint64;
            else type.kind = ir::type_kind::uint128;
        }
        return type;
    }

    ir::type_ref make_string_type() {
        ir::type_ref type;
        type.kind = ir::type_kind::string;
        return type;
    }

    ir::type_ref make_bool_type() {
        ir::type_ref type;
        type.kind = ir::type_kind::boolean;
        return type;
    }

    ir::type_ref make_array_type(ir::type_ref element_type, uint64_t size) {
        ir::type_ref type;
        type.element_type = std::make_unique<ir::type_ref>(std::move(element_type));
        type.array_size = size;
        return type;
    }

    ir::field make_field(const std::string& name, ir::type_ref type) {
        ir::field field;
        field.name = name;
        field.type = std::move(type);
        return field;
    }

    ir::struct_def make_simple_struct() {
        ir::struct_def s;
        s.name = "SimpleStruct";
        s.documentation = "A simple test struct";

        s.fields.push_back(make_field("id", make_primitive_type(false, 32)));
        s.fields.push_back(make_field("name", make_string_type()));

        return s;
    }

    // ========================================================================
    // Component Builder Tests
    // ========================================================================

    TEST_CASE("Emit struct start/end commands") {
        CommandBuilder builder;

        builder.emit_struct_start("TestStruct", "Test documentation");
        builder.emit_struct_end();

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 2);

        CHECK(commands[0]->type == Command::StartStruct);
        CHECK(commands[1]->type == Command::EndStruct);

        auto* start_cmd = static_cast<StartStructCommand*>(commands[0].get());
        CHECK(start_cmd->struct_name == "TestStruct");
        CHECK(start_cmd->doc_comment == "Test documentation");
    }

    TEST_CASE("Emit method start/end commands") {
        CommandBuilder builder;

        // Create a struct def for testing
        ir::struct_def test_struct;
        test_struct.name = "TestStruct";

        builder.emit_method_start("read", StartMethodCommand::MethodKind::StructReader,
                                 &test_struct, false, false);
        builder.emit_method_end();

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 2);

        CHECK(commands[0]->type == Command::StartMethod);
        CHECK(commands[1]->type == Command::EndMethod);

        auto* start_cmd = static_cast<StartMethodCommand*>(commands[0].get());
        CHECK(start_cmd->method_name == "read");
        CHECK(start_cmd->kind == StartMethodCommand::MethodKind::StructReader);
        CHECK(start_cmd->target_struct == &test_struct);
        CHECK(start_cmd->use_exceptions == false);
        CHECK(start_cmd->is_static == false);
    }

    TEST_CASE("Emit field declaration commands") {
        CommandBuilder builder;

        // Create IR type (language-agnostic)
        ir::type_ref type;
        type.kind = ir::type_kind::uint32;

        builder.emit_field_declaration("count", &type, "Number of items");

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::DeclareField);

        auto* decl_cmd = static_cast<DeclareFieldCommand*>(commands[0].get());
        CHECK(decl_cmd->field_name == "count");
        // Check IR type, not C++ type string
        REQUIRE(decl_cmd->field_type != nullptr);
        CHECK(decl_cmd->field_type->kind == ir::type_kind::uint32);
        CHECK(decl_cmd->doc_comment == "Number of items");
    }

    TEST_CASE("Emit variable declaration commands") {
        CommandBuilder builder;

        // Create IR type (language-agnostic)
        ir::type_ref uint64_type;
        uint64_type.kind = ir::type_kind::uint64;

        builder.emit_variable_declaration("size", &uint64_type, nullptr);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::DeclareVariable);

        auto* decl_cmd = static_cast<DeclareVariableCommand*>(commands[0].get());
        CHECK(decl_cmd->var_name == "size");
        CHECK(decl_cmd->kind == DeclareVariableCommand::VariableKind::LocalVariable);
        REQUIRE(decl_cmd->type != nullptr);
        CHECK(decl_cmd->type->kind == ir::type_kind::uint64);
        CHECK(decl_cmd->init_expr == nullptr);
    }

    TEST_CASE("Emit loop start/end commands") {
        CommandBuilder builder;

        ir::expr count_expr;
        count_expr.type = ir::expr::literal_int;
        count_expr.int_value = 10;

        builder.emit_loop_start("i", &count_expr);
        builder.emit_loop_end();

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 2);

        CHECK(commands[0]->type == Command::StartLoop);
        CHECK(commands[1]->type == Command::EndLoop);

        auto* loop_cmd = static_cast<StartLoopCommand*>(commands[0].get());
        CHECK(loop_cmd->loop_var == "i");
        CHECK(loop_cmd->count_expr != nullptr);
        CHECK(loop_cmd->count_expr->type == ir::expr::literal_int);
    }

    TEST_CASE("Emit return value command") {
        CommandBuilder builder;

        builder.emit_return_value("true");

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::ReturnValue);

        auto* ret_cmd = static_cast<ReturnValueCommand*>(commands[0].get());
        CHECK(ret_cmd->value == "true");
    }

    // ========================================================================
    // Field Read Tests
    // ========================================================================

    TEST_CASE("Emit primitive field read") {
        CommandBuilder builder;

        auto field = make_field("id", make_primitive_type(false, 32));
        builder.emit_field_read(field, true);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::ReadField);

        auto* read_cmd = static_cast<ReadFieldCommand*>(commands[0].get());
        CHECK(read_cmd->field_name == "id");
        // Check IR type, not C++ string
        REQUIRE(read_cmd->field_type != nullptr);
        CHECK(read_cmd->field_type->kind == ir::type_kind::uint32);
        CHECK(read_cmd->use_exceptions == true);
    }

    TEST_CASE("Emit string field read") {
        CommandBuilder builder;

        auto field = make_field("name", make_string_type());
        builder.emit_field_read(field, false);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::ReadField);

        auto* read_cmd = static_cast<ReadFieldCommand*>(commands[0].get());
        CHECK(read_cmd->field_name == "name");
        // Check IR type, not C++ string
        REQUIRE(read_cmd->field_type != nullptr);
        CHECK(read_cmd->field_type->kind == ir::type_kind::string);
        CHECK(read_cmd->use_exceptions == false);
    }

    TEST_CASE("Emit boolean field read") {
        CommandBuilder builder;

        auto field = make_field("is_active", make_bool_type());
        builder.emit_field_read(field, true);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::ReadField);

        auto* read_cmd = static_cast<ReadFieldCommand*>(commands[0].get());
        CHECK(read_cmd->field_name == "is_active");
        // Check IR type, not C++ string
        REQUIRE(read_cmd->field_type != nullptr);
        CHECK(read_cmd->field_type->kind == ir::type_kind::boolean);
        CHECK(read_cmd->use_exceptions == true);
    }

    TEST_CASE("Emit fixed array field read") {
        CommandBuilder builder;

        auto field = make_field(
            "data",
            make_array_type(make_primitive_type(false, 8), 10)
        );
        builder.emit_field_read(field, true);

        auto commands = builder.take_commands();

        // Fixed-size arrays don't need resize - should emit: StartLoop, ReadArrayElement, EndLoop
        REQUIRE(commands.size() == 3);

        CHECK(commands[0]->type == Command::StartLoop);
        CHECK(commands[1]->type == Command::ReadArrayElement);
        CHECK(commands[2]->type == Command::EndLoop);

        auto* loop_cmd = static_cast<StartLoopCommand*>(commands[0].get());
        CHECK(loop_cmd->loop_var == "i");

        auto* read_cmd = static_cast<ReadArrayElementCommand*>(commands[1].get());
        CHECK(read_cmd->element_name == "obj.data[i]");
        // Check IR type, not C++ string
        REQUIRE(read_cmd->element_type != nullptr);
        CHECK(read_cmd->element_type->kind == ir::type_kind::uint8);
        CHECK(read_cmd->use_exceptions == true);
    }

    // ========================================================================
    // Constraint Tests
    // ========================================================================

    TEST_CASE("Emit constraint check") {
        CommandBuilder builder;

        ir::expr condition;
        condition.type = ir::expr::literal_bool;
        condition.bool_value = true;

        builder.emit_constraint_check(&condition, "Test constraint failed", true);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::ConstraintCheck);

        auto* check_cmd = static_cast<ConstraintCheckCommand*>(commands[0].get());
        CHECK(check_cmd->condition != nullptr);
        CHECK(check_cmd->error_message == "Test constraint failed");
        CHECK(check_cmd->use_exceptions == true);
    }

    TEST_CASE("Emit bounds check") {
        CommandBuilder builder;

        ir::expr value, min_val, max_val;
        value.type = ir::expr::literal_int;
        value.int_value = 5;
        min_val.type = ir::expr::literal_int;
        min_val.int_value = 1;
        max_val.type = ir::expr::literal_int;
        max_val.int_value = 10;

        builder.emit_bounds_check("count", &value, &min_val, &max_val,
                                  "Count out of bounds", false);

        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        CHECK(commands[0]->type == Command::BoundsCheck);

        auto* check_cmd = static_cast<BoundsCheckCommand*>(commands[0].get());
        CHECK(check_cmd->value_name == "count");
        CHECK(check_cmd->error_message == "Count out of bounds");
        CHECK(check_cmd->use_exceptions == false);
    }

    // ========================================================================
    // Struct Builder Tests
    // ========================================================================

    TEST_CASE("Build simple struct declaration") {
        CommandBuilder builder;

        auto struct_def = make_simple_struct();
        auto commands = builder.build_struct_declaration(struct_def);

        // Should emit: StartStruct, DeclareField (id), DeclareField (name), EndStruct
        REQUIRE(commands.size() == 4);

        CHECK(commands[0]->type == Command::StartStruct);
        CHECK(commands[1]->type == Command::DeclareField);
        CHECK(commands[2]->type == Command::DeclareField);
        CHECK(commands[3]->type == Command::EndStruct);

        auto* start_cmd = static_cast<StartStructCommand*>(commands[0].get());
        CHECK(start_cmd->struct_name == "SimpleStruct");

        auto* field1_cmd = static_cast<DeclareFieldCommand*>(commands[1].get());
        CHECK(field1_cmd->field_name == "id");
        // Check IR type, not C++ string
        REQUIRE(field1_cmd->field_type != nullptr);
        CHECK(field1_cmd->field_type->kind == ir::type_kind::uint32);

        auto* field2_cmd = static_cast<DeclareFieldCommand*>(commands[2].get());
        CHECK(field2_cmd->field_name == "name");
        // Check IR type, not C++ string
        REQUIRE(field2_cmd->field_type != nullptr);
        CHECK(field2_cmd->field_type->kind == ir::type_kind::string);
    }

    TEST_CASE("Build struct reader method with exceptions") {
        CommandBuilder builder;

        auto struct_def = make_simple_struct();
        auto commands = builder.build_struct_reader(struct_def, true);

        // Should emit (NEW ARCHITECTURE - static factory methods):
        // StartStruct, DeclareField (id), DeclareField (name),
        // StartMethod (read), DeclareVariable (obj), ReadField (id), ReadField (name),
        // ReturnValue (obj), EndMethod, EndStruct
        REQUIRE(commands.size() == 10);

        CHECK(commands[0]->type == Command::StartStruct);
        CHECK(commands[3]->type == Command::StartMethod);
        CHECK(commands[4]->type == Command::DeclareVariable);  // NEW: obj declaration
        CHECK(commands[5]->type == Command::ReadField);
        CHECK(commands[6]->type == Command::ReadField);
        CHECK(commands[7]->type == Command::ReturnValue);      // NEW: return obj
        CHECK(commands[8]->type == Command::EndMethod);
        CHECK(commands[9]->type == Command::EndStruct);

        auto* method_cmd = static_cast<StartMethodCommand*>(commands[3].get());
        CHECK(method_cmd->method_name == "read");
        CHECK(method_cmd->kind == StartMethodCommand::MethodKind::StructReader);
        CHECK(method_cmd->use_exceptions == true);  // Exceptions mode
        CHECK(method_cmd->is_static == true);       // NEW: static factory method
    }

    TEST_CASE("Build struct reader method with safe reads") {
        CommandBuilder builder;

        auto struct_def = make_simple_struct();
        auto commands = builder.build_struct_reader(struct_def, false);

        // Should emit (NEW ARCHITECTURE - static factory methods + result handling):
        // StartStruct, DeclareField (id), DeclareField (name),
        // StartMethod (read_safe), DeclareResultVariable (result),
        // DeclareVariable (obj), ReadField (id), ReadField (name),
        // AssignVariable (result.value = obj), SetResultSuccess, ReturnValue (result),
        // EndMethod, EndStruct
        REQUIRE(commands.size() == 13);

        auto* method_cmd = static_cast<StartMethodCommand*>(commands[3].get());
        CHECK(method_cmd->method_name == "read_safe");  // NEW: read_safe name
        CHECK(method_cmd->kind == StartMethodCommand::MethodKind::StructReader);
        CHECK(method_cmd->use_exceptions == false);  // Safe mode
        CHECK(method_cmd->is_static == true);        // NEW: static factory method

        // Should have DeclareResultVariable for result
        CHECK(commands[4]->type == Command::DeclareResultVariable);

        // Should have DeclareVariable for obj
        CHECK(commands[5]->type == Command::DeclareVariable);

        // Should have AssignVariable for result.value = obj
        CHECK(commands[8]->type == Command::AssignVariable);

        // Should have SetResultSuccess
        CHECK(commands[9]->type == Command::SetResultSuccess);

        // Last command before EndMethod should be ReturnValue
        CHECK(commands[10]->type == Command::ReturnValue);
        auto* ret_cmd = static_cast<ReturnValueCommand*>(commands[10].get());
        CHECK(ret_cmd->value == "result");  // NEW: Safe mode returns result (ReadResult<T>)
    }

    TEST_CASE("Build standalone reader function") {
        CommandBuilder builder;

        auto struct_def = make_simple_struct();
        auto commands = builder.build_standalone_reader(struct_def, true);

        // Should emit:
        // StartMethod, DeclareVariable (result), ReadField (id), ReadField (name),
        // ReturnValue, EndMethod
        REQUIRE(commands.size() == 6);

        CHECK(commands[0]->type == Command::StartMethod);
        CHECK(commands[1]->type == Command::DeclareVariable);
        CHECK(commands[2]->type == Command::ReadField);
        CHECK(commands[3]->type == Command::ReadField);
        CHECK(commands[4]->type == Command::ReturnValue);
        CHECK(commands[5]->type == Command::EndMethod);

        auto* method_cmd = static_cast<StartMethodCommand*>(commands[0].get());
        CHECK(method_cmd->method_name == "read_SimpleStruct");
        CHECK(method_cmd->kind == StartMethodCommand::MethodKind::StandaloneReader);
        CHECK(method_cmd->use_exceptions == true);  // Exceptions mode
        CHECK(method_cmd->is_static == true);

        auto* var_cmd = static_cast<DeclareVariableCommand*>(commands[1].get());
        CHECK(var_cmd->var_name == "result");
        CHECK(var_cmd->kind == DeclareVariableCommand::VariableKind::StructInstance);
        REQUIRE(var_cmd->struct_type != nullptr);
        CHECK(var_cmd->struct_type->name == "SimpleStruct");
    }

    // ========================================================================
    // Type Mapping Tests
    // ========================================================================
    // Note: CommandBuilder is now language-agnostic and works with IR types.
    // Type-to-string conversion is done by language-specific renderers.

    TEST_CASE("Type mapping via field declarations") {
        CommandBuilder builder;

        // Test that primitive types work
        auto type_u32 = make_primitive_type(false, 32);
        builder.emit_field_declaration("test_field", &type_u32, "");
        auto commands = builder.take_commands();
        REQUIRE(commands.size() == 1);

        auto* decl = static_cast<DeclareFieldCommand*>(commands[0].get());
        // Check IR type, not C++ string
        REQUIRE(decl->field_type != nullptr);
        CHECK(decl->field_type->kind == ir::type_kind::uint32);
    }

    TEST_CASE("Clear commands") {
        CommandBuilder builder;

        builder.emit_struct_start("Test", "");
        builder.emit_struct_end();

        auto commands1 = builder.take_commands();
        REQUIRE(commands1.size() == 2);

        // take_commands should clear internal state
        // Create IR type for test
        ir::type_ref int_type;
        int_type.kind = ir::type_kind::int32;
        builder.emit_field_declaration("field", &int_type, "");
        auto commands2 = builder.take_commands();
        REQUIRE(commands2.size() == 1);
    }
}

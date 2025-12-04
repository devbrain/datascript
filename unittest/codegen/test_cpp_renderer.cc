//
// Tests for C++ Renderer
//

#include <doctest/doctest.h>
#include <datascript/codegen/cpp/cpp_renderer.hh>

using namespace datascript;
using namespace datascript::codegen;

TEST_SUITE("Codegen - C++ Renderer") {

    // ========================================================================
    // Helper Functions
    // ========================================================================

    std::string trim(const std::string& str) {
        size_t start = str.find_first_not_of(" \t\n\r");
        if (start == std::string::npos) return "";
        size_t end = str.find_last_not_of(" \t\n\r");
        return str.substr(start, end - start + 1);
    }

    bool contains(const std::string& haystack, const std::string& needle) {
        return haystack.find(needle) != std::string::npos;
    }

    // ========================================================================
    // Basic Output Tests
    // ========================================================================

    TEST_CASE("Empty renderer produces empty output") {
        CppRenderer renderer;
        CHECK(renderer.get_output() == "");
    }

    TEST_CASE("Clear resets output") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;
        commands.push_back(std::make_unique<CommentCommand>("Test"));
        renderer.render_commands(commands);

        CHECK(!renderer.get_output().empty());
        renderer.clear();
        CHECK(renderer.get_output() == "");
    }

    // ========================================================================
    // Comment Rendering
    // ========================================================================

    TEST_CASE("Render comment command") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;
        commands.push_back(std::make_unique<CommentCommand>("This is a test comment"));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "// This is a test comment"));
    }

    // ========================================================================
    // Struct Rendering
    // ========================================================================

    TEST_CASE("Render simple struct") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create IR types (language-agnostic)
        ir::type_ref int32_type;
        int32_type.kind = ir::type_kind::int32;

        commands.push_back(std::make_unique<StartStructCommand>("Point", ""));
        commands.push_back(std::make_unique<DeclareFieldCommand>("x", &int32_type, ""));
        commands.push_back(std::make_unique<DeclareFieldCommand>("y", &int32_type, ""));
        commands.push_back(std::make_unique<EndStructCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "struct Point {"));
        CHECK(contains(output, "int32_t x;"));
        CHECK(contains(output, "int32_t y;"));
        CHECK(contains(output, "};"));
    }

    TEST_CASE("Render struct with documentation") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        commands.push_back(std::make_unique<StartStructCommand>("Data", "A data structure"));
        commands.push_back(std::make_unique<EndStructCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "/**"));
        CHECK(contains(output, "* A data structure"));
        CHECK(contains(output, "*/"));
        CHECK(contains(output, "struct Data {"));
    }

    // ========================================================================
    // Method Rendering
    // ========================================================================

    TEST_CASE("Render method with no parameters") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create custom method (no parameters, returns void)
        commands.push_back(std::make_unique<StartMethodCommand>(
            "foo", StartMethodCommand::MethodKind::Custom, nullptr, true, false));
        commands.push_back(std::make_unique<EndMethodCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "void foo() {"));
        CHECK(contains(output, "}"));
    }

    TEST_CASE("Render method with parameters") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create a struct reader method (safe mode, static, returns ReadResult)
        ir::struct_def test_struct;
        test_struct.name = "TestData";
        commands.push_back(std::make_unique<StartMethodCommand>(
            "read_safe", StartMethodCommand::MethodKind::StructReader, &test_struct, false, true));
        commands.push_back(std::make_unique<EndMethodCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "static ReadResult<TestData> read_safe(const uint8_t*& data, const uint8_t* end) {"));
    }

    TEST_CASE("Render static method") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create struct reader (static, exceptions mode, returns Data)
        ir::struct_def test_struct;
        test_struct.name = "Data";
        commands.push_back(std::make_unique<StartMethodCommand>(
            "read", StartMethodCommand::MethodKind::StructReader, &test_struct, true, true));
        commands.push_back(std::make_unique<EndMethodCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "static Data read(const uint8_t*& data, const uint8_t* end) {"));
    }

    TEST_CASE("Render method with return value") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create struct reader (safe mode, returns bool)
        ir::struct_def test_struct;
        test_struct.name = "TestData";
        commands.push_back(std::make_unique<StartMethodCommand>(
            "test", StartMethodCommand::MethodKind::StructReader, &test_struct, false, false));
        commands.push_back(std::make_unique<ReturnValueCommand>("true"));
        commands.push_back(std::make_unique<EndMethodCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "return true;"));
    }

    // ========================================================================
    // Variable Rendering
    // ========================================================================

    TEST_CASE("Render variable declaration without initialization") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::type_ref uint32_type;
        uint32_type.kind = ir::type_kind::uint32;
        commands.push_back(std::make_unique<DeclareVariableCommand>("count", &uint32_type, nullptr));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "uint32_t count;"));
    }

    TEST_CASE("Render variable declaration with initialization") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::type_ref int32_type;
        int32_type.kind = ir::type_kind::int32;

        ir::expr init;
        init.type = ir::expr::literal_int;
        init.int_value = 42;

        commands.push_back(std::make_unique<DeclareVariableCommand>("value", &int32_type, &init));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "int32_t value = 42;"));
    }

    // ========================================================================
    // Loop Rendering
    // ========================================================================

    TEST_CASE("Render simple loop") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr count;
        count.type = ir::expr::literal_int;
        count.int_value = 10;

        commands.push_back(std::make_unique<StartLoopCommand>("i", &count));
        commands.push_back(std::make_unique<CommentCommand>("Loop body"));
        commands.push_back(std::make_unique<EndLoopCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "for (size_t i = 0; i < 10; i++) {"));
        CHECK(contains(output, "// Loop body"));
        CHECK(contains(output, "}"));
    }

    // ========================================================================
    // Array Rendering
    // ========================================================================

    TEST_CASE("Render array resize") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr size;
        size.type = ir::expr::literal_int;
        size.int_value = 5;

        commands.push_back(std::make_unique<ResizeArrayCommand>("data", &size));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "data.resize(5);"));
    }

    // ========================================================================
    // Conditional Rendering
    // ========================================================================

    TEST_CASE("Render if statement") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr condition;
        condition.type = ir::expr::literal_bool;
        condition.bool_value = true;

        commands.push_back(std::make_unique<IfCommand>(&condition));
        commands.push_back(std::make_unique<CommentCommand>("Then branch"));
        commands.push_back(std::make_unique<EndIfCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "if (true) {"));
        CHECK(contains(output, "// Then branch"));
    }

    TEST_CASE("Render if-else statement") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr condition;
        condition.type = ir::expr::literal_bool;
        condition.bool_value = false;

        commands.push_back(std::make_unique<IfCommand>(&condition));
        commands.push_back(std::make_unique<CommentCommand>("Then branch"));
        commands.push_back(std::make_unique<ElseCommand>());
        commands.push_back(std::make_unique<CommentCommand>("Else branch"));
        commands.push_back(std::make_unique<EndIfCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "if (false) {"));
        CHECK(contains(output, "} else {"));
    }

    // ========================================================================
    // Constraint Rendering
    // ========================================================================

    TEST_CASE("Render constraint check with exceptions") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr condition;
        condition.type = ir::expr::literal_bool;
        condition.bool_value = true;

        commands.push_back(std::make_unique<ConstraintCheckCommand>(&condition, "Constraint failed", true));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "if (!(true)) {"));
        CHECK(contains(output, "throw ConstraintViolation(\"Constraint failed\");"));
    }

    TEST_CASE("Render constraint check with safe reads") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr condition;
        condition.type = ir::expr::literal_bool;
        condition.bool_value = false;

        commands.push_back(std::make_unique<ConstraintCheckCommand>(&condition, "Check failed", false));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "if (!(false)) {"));
        CHECK(contains(output, "return result;"));
    }

    // ========================================================================
    // Bounds Check Rendering
    // ========================================================================

    TEST_CASE("Render bounds check") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        ir::expr value, min_val, max_val;
        value.type = ir::expr::literal_int;
        value.int_value = 5;
        min_val.type = ir::expr::literal_int;
        min_val.int_value = 1;
        max_val.type = ir::expr::literal_int;
        max_val.int_value = 10;

        commands.push_back(std::make_unique<BoundsCheckCommand>(
            "count", &value, &min_val, &max_val, "Out of bounds", true
        ));

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "if (5 < 1 || 5 > 10) {"));
        CHECK(contains(output, "throw std::runtime_error(\"Out of bounds\");"));
    }

    // ========================================================================
    // Integration Tests
    // ========================================================================

    TEST_CASE("Render complete struct with method") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create IR types (language-agnostic)
        ir::type_ref uint32_type;
        uint32_type.kind = ir::type_kind::uint32;

        ir::type_ref string_type;
        string_type.kind = ir::type_kind::string;

        // Struct declaration
        commands.push_back(std::make_unique<StartStructCommand>("Message", ""));
        commands.push_back(std::make_unique<DeclareFieldCommand>("id", &uint32_type, ""));
        commands.push_back(std::make_unique<DeclareFieldCommand>("text", &string_type, ""));

        // Read method (struct reader, exceptions mode, static)
        ir::struct_def message_struct;
        message_struct.name = "Message";
        commands.push_back(std::make_unique<StartMethodCommand>(
            "read", StartMethodCommand::MethodKind::StructReader, &message_struct, true, true));
        commands.push_back(std::make_unique<CommentCommand>("Read fields"));
        commands.push_back(std::make_unique<EndMethodCommand>());

        commands.push_back(std::make_unique<EndStructCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        CHECK(contains(output, "struct Message {"));
        CHECK(contains(output, "uint32_t id;"));
        CHECK(contains(output, "std::string text;"));
        CHECK(contains(output, "static Message read(const uint8_t*& data, const uint8_t* end) {"));
        CHECK(contains(output, "// Read fields"));
        CHECK(contains(output, "};"));
    }

    TEST_CASE("Indentation is correct") {
        CppRenderer renderer;
        std::vector<CommandPtr> commands;

        // Create IR type (language-agnostic)
        ir::type_ref int32_type;
        int32_type.kind = ir::type_kind::int32;

        commands.push_back(std::make_unique<StartStructCommand>("Test", ""));
        commands.push_back(std::make_unique<DeclareFieldCommand>("x", &int32_type, ""));
        commands.push_back(std::make_unique<EndStructCommand>());

        renderer.render_commands(commands);
        std::string output = renderer.get_output();

        // Field should be indented
        CHECK(contains(output, "    int32_t x;"));
    }
}

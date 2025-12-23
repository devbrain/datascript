//
// End-to-End Integration Tests for Command Stream Architecture
//
// Tests the full pipeline: IR → CommandBuilder → CppRenderer → C++ Code
//

#include <doctest/doctest.h>
#include <datascript/command_builder.hh>
#include <datascript/codegen/cpp/cpp_renderer.hh>

using namespace datascript;
using namespace datascript::codegen;

namespace {
    // Helper function for integration tests
    inline bool contains(const std::string& haystack, const std::string& needle) {
        return haystack.find(needle) != std::string::npos;
    }
}

TEST_SUITE("Codegen - Integration Tests") {

    // ========================================================================
    // Helper Functions
    // ========================================================================

    ir::struct_def make_test_struct(const std::string& name) {
        ir::struct_def s;
        s.name = name;
        s.documentation = "Test struct";
        return s;
    }

    void add_field(ir::struct_def& s, const std::string& name, ir::type_kind kind) {
        ir::field field;
        field.name = name;
        field.type.kind = kind;
        s.fields.emplace_back(std::move(field));
    }

    std::string build_and_render(const ir::struct_def& struct_def) {
        CommandBuilder builder;
        auto commands = builder.build_struct_declaration(struct_def);

        CppRenderer renderer;
        renderer.render_commands(commands);
        return renderer.get_output();
    }

    // ========================================================================
    // Basic Integration Tests
    // ========================================================================

    TEST_CASE("Empty struct generates valid C++ code") {
        auto s = make_test_struct("Empty");
        std::string code = build_and_render(s);

        CHECK(contains(code, "struct Empty {"));
        CHECK(contains(code, "};"));
    }

    TEST_CASE("Struct with single primitive field") {
        auto s = make_test_struct("SingleField");
        add_field(s, "value", ir::type_kind::uint32);

        std::string code = build_and_render(s);

        CHECK(contains(code, "struct SingleField {"));
        CHECK(contains(code, "uint32_t value;"));
        CHECK(contains(code, "};"));
    }

    TEST_CASE("Struct with multiple primitive fields") {
        auto s = make_test_struct("Point");
        add_field(s, "x", ir::type_kind::int32);
        add_field(s, "y", ir::type_kind::int32);
        add_field(s, "z", ir::type_kind::int32);

        std::string code = build_and_render(s);

        CHECK(contains(code, "struct Point {"));
        CHECK(contains(code, "int32_t x;"));
        CHECK(contains(code, "int32_t y;"));
        CHECK(contains(code, "int32_t z;"));
        CHECK(contains(code, "};"));
    }

    TEST_CASE("Struct with different primitive types") {
        auto s = make_test_struct("Mixed");
        add_field(s, "byte_val", ir::type_kind::uint8);
        add_field(s, "short_val", ir::type_kind::int16);
        add_field(s, "int_val", ir::type_kind::uint32);
        add_field(s, "long_val", ir::type_kind::int64);

        std::string code = build_and_render(s);

        CHECK(contains(code, "uint8_t byte_val;"));
        CHECK(contains(code, "int16_t short_val;"));
        CHECK(contains(code, "uint32_t int_val;"));
        CHECK(contains(code, "int64_t long_val;"));
    }

    TEST_CASE("Struct with string field") {
        auto s = make_test_struct("WithString");
        add_field(s, "id", ir::type_kind::uint32);
        add_field(s, "name", ir::type_kind::string);

        std::string code = build_and_render(s);

        CHECK(contains(code, "uint32_t id;"));
        CHECK(contains(code, "std::string name;"));
    }

    TEST_CASE("Struct with boolean field") {
        auto s = make_test_struct("WithBool");
        add_field(s, "flag", ir::type_kind::boolean);

        std::string code = build_and_render(s);

        CHECK(contains(code, "bool flag;"));
    }

    TEST_CASE("Struct with array field") {
        auto s = make_test_struct("WithArray");

        ir::field array_field;
        array_field.name = "data";
        array_field.type.element_type = std::make_unique<ir::type_ref>();
        array_field.type.element_type->kind = ir::type_kind::uint8;
        array_field.type.array_size = 10;

        s.fields.emplace_back(std::move(array_field));

        std::string code = build_and_render(s);

        CHECK(contains(code, "std::vector<uint8_t> data;"));
    }

    TEST_CASE("Struct with documentation") {
        ir::struct_def s;
        s.name = "Documented";
        s.documentation = "This is a documented struct";
        add_field(s, "value", ir::type_kind::uint32);

        std::string code = build_and_render(s);

        CHECK(contains(code, "/**"));
        CHECK(contains(code, "This is a documented struct"));
        CHECK(contains(code, "*/"));
        CHECK(contains(code, "struct Documented {"));
    }

    // ========================================================================
    // Method Generation Tests
    // ========================================================================

    TEST_CASE("Struct with read method") {
        auto s = make_test_struct("WithMethod");
        add_field(s, "id", ir::type_kind::uint32);
        add_field(s, "name", ir::type_kind::string);

        CommandBuilder builder;
        auto commands = builder.build_struct_reader(s, true);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        CHECK(contains(code, "struct WithMethod {"));
        CHECK(contains(code, "uint32_t id;"));
        CHECK(contains(code, "std::string name;"));
        CHECK(contains(code, "static WithMethod read(const uint8_t*& data, const uint8_t* end) {"));
        CHECK(contains(code, "};"));
    }

    TEST_CASE("Struct with safe read method") {
        auto s = make_test_struct("SafeRead");
        add_field(s, "value", ir::type_kind::uint32);

        CommandBuilder builder;
        auto commands = builder.build_struct_reader(s, false);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        CHECK(contains(code, "static ReadResult<SafeRead> read_safe(const uint8_t*& data, const uint8_t* end) {"));
        CHECK(contains(code, "return result;"));
    }

    TEST_CASE("Standalone reader function") {
        auto s = make_test_struct("Standalone");
        add_field(s, "x", ir::type_kind::int32);
        add_field(s, "y", ir::type_kind::int32);

        CommandBuilder builder;
        auto commands = builder.build_standalone_reader(s, true);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        CHECK(contains(code, "static Standalone read_Standalone(const uint8_t*& data, const uint8_t* end) {"));
        CHECK(contains(code, "Standalone result{};"));
        CHECK(contains(code, "return result;"));
    }

    // ========================================================================
    // Array Field Tests
    // ========================================================================

    TEST_CASE("Read method with fixed array") {
        auto s = make_test_struct("FixedArray");

        ir::field array_field;
        array_field.name = "bytes";
        array_field.type.element_type = std::make_unique<ir::type_ref>();
        array_field.type.element_type->kind = ir::type_kind::uint8;
        array_field.type.array_size = 16;
        s.fields.emplace_back(std::move(array_field));

        CommandBuilder builder;
        auto commands = builder.build_struct_reader(s, true);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        // Fixed-size arrays don't need resize (they're std::array, not std::vector)
        // Should NOT have resize
        CHECK(!contains(code, ".resize("));

        // Should have loop
        CHECK(contains(code, "for (size_t i = 0; i < 16; i++) {"));
    }

    TEST_CASE("Read method with variable-size array") {
        auto s = make_test_struct("VarArray");

        // Add count field
        add_field(s, "count", ir::type_kind::uint32);

        // Add variable-size array
        ir::field array_field;
        array_field.name = "items";
        array_field.type.element_type = std::make_unique<ir::type_ref>();
        array_field.type.element_type->kind = ir::type_kind::uint32;

        ir::expr count_expr;
        count_expr.type = ir::expr::field_ref;
        count_expr.ref_name = "count";
        array_field.type.array_size_expr = std::make_unique<ir::expr>(std::move(count_expr));

        s.fields.emplace_back(std::move(array_field));

        CommandBuilder builder;
        auto commands = builder.build_struct_reader(s, true);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        // Should reference count field
        CHECK(contains(code, "obj.count"));

        // Should have array operations
        CHECK(contains(code, "obj.items.resize(obj.count);"));
        CHECK(contains(code, "for (size_t i = 0; i < obj.count; i++) {"));
    }

    // ========================================================================
    // Complex Integration Tests
    // ========================================================================

    TEST_CASE("Complex struct with multiple field types") {
        auto s = make_test_struct("Complex");
        add_field(s, "id", ir::type_kind::uint32);
        add_field(s, "name", ir::type_kind::string);
        add_field(s, "active", ir::type_kind::boolean);
        add_field(s, "count", ir::type_kind::uint16);

        CommandBuilder builder;
        auto commands = builder.build_struct_reader(s, true);

        CppRenderer renderer;
        renderer.render_commands(commands);
        std::string code = renderer.get_output();

        // Verify struct declaration
        CHECK(contains(code, "struct Complex {"));
        CHECK(contains(code, "uint32_t id;"));
        CHECK(contains(code, "std::string name;"));
        CHECK(contains(code, "bool active;"));
        CHECK(contains(code, "uint16_t count;"));

        // Verify read method
        CHECK(contains(code, "static Complex read(const uint8_t*& data, const uint8_t* end) {"));
    }

    TEST_CASE("Multiple structs can be generated") {
        // Generate first struct
        auto s1 = make_test_struct("First");
        add_field(s1, "x", ir::type_kind::int32);

        CommandBuilder builder1;
        auto commands1 = builder1.build_struct_declaration(s1);

        CppRenderer renderer;
        renderer.render_commands(commands1);
        std::string code1 = renderer.get_output();

        CHECK(contains(code1, "struct First {"));

        // Clear and generate second struct
        renderer.clear();

        auto s2 = make_test_struct("Second");
        add_field(s2, "y", ir::type_kind::int64);

        CommandBuilder builder2;
        auto commands2 = builder2.build_struct_declaration(s2);

        renderer.render_commands(commands2);
        std::string code2 = renderer.get_output();

        CHECK(contains(code2, "struct Second {"));
        CHECK(!contains(code2, "struct First {"));  // Should not contain first struct
    }

    // ========================================================================
    // Validation Tests
    // ========================================================================

    TEST_CASE("Generated code has proper indentation") {
        auto s = make_test_struct("Indented");
        add_field(s, "value", ir::type_kind::uint32);

        std::string code = build_and_render(s);

        // Check that field is indented (4 spaces by default)
        CHECK(contains(code, "    uint32_t value;"));
    }

    TEST_CASE("Generated code has proper structure") {
        auto s = make_test_struct("Structured");
        add_field(s, "a", ir::type_kind::uint8);
        add_field(s, "b", ir::type_kind::uint16);
        add_field(s, "c", ir::type_kind::uint32);

        std::string code = build_and_render(s);

        // Find positions of key elements
        size_t struct_pos = code.find("struct Structured {");
        size_t field_a_pos = code.find("uint8_t a;");
        size_t field_b_pos = code.find("uint16_t b;");
        size_t field_c_pos = code.find("uint32_t c;");
        size_t end_pos = code.find("};");

        // Verify ordering
        CHECK(struct_pos < field_a_pos);
        CHECK(field_a_pos < field_b_pos);
        CHECK(field_b_pos < field_c_pos);
        CHECK(field_c_pos < end_pos);
    }
}

#include <doctest/doctest.h>
#include <datascript/codegen/datascript/datascript_renderer.hh>
#include <datascript/ir.hh>
#include <datascript/base_renderer.hh>
#include <sstream>

using namespace datascript;
using namespace datascript::codegen;

TEST_CASE("DataScriptRenderer - Simple struct") {
    // Create a simple IR bundle
    ir::bundle bundle;
    bundle.name = "test";

    // Create a simple struct
    ir::struct_def simple_struct;
    simple_struct.name = "SimpleStruct";
    simple_struct.documentation = "A simple test structure";

    // Add a uint32 field
    ir::field field1;
    field1.name = "magic";
    field1.type.kind = ir::type_kind::uint32;
    field1.type.size_bytes = 4;
    simple_struct.fields.push_back(std::move(field1));

    // Add a uint16 field
    ir::field field2;
    field2.name = "version";
    field2.type.kind = ir::type_kind::uint16;
    field2.type.size_bytes = 2;
    simple_struct.fields.push_back(std::move(field2));

    bundle.structs.push_back(std::move(simple_struct));

    // Render to DataScript
    DataScriptRenderer renderer;
    RenderOptions options;
    std::string result = renderer.render_module(bundle, options);

    // Verify output contains expected elements
    CHECK(result.find("package test;") != std::string::npos);
    CHECK(result.find("struct SimpleStruct") != std::string::npos);
    CHECK(result.find("uint32 magic;") != std::string::npos);
    CHECK(result.find("uint16 version;") != std::string::npos);
    CHECK(result.find("/**") != std::string::npos);
    CHECK(result.find("A simple test structure") != std::string::npos);
}

TEST_CASE("DataScriptRenderer - Enum") {
    ir::bundle bundle;
    bundle.name = "test";

    // Create an enum
    ir::enum_def enum_def;
    enum_def.name = "Colors";
    enum_def.base_type.kind = ir::type_kind::uint8;
    enum_def.is_bitmask = false;

    ir::enum_def::item item1;
    item1.name = "RED";
    item1.value = 1;
    enum_def.items.push_back(item1);

    ir::enum_def::item item2;
    item2.name = "GREEN";
    item2.value = 2;
    enum_def.items.push_back(item2);

    bundle.enums.push_back(std::move(enum_def));

    DataScriptRenderer renderer;
    RenderOptions options;
    std::string result = renderer.render_module(bundle, options);

    CHECK(result.find("enum Colors : uint8") != std::string::npos);
    CHECK(result.find("RED = 1,") != std::string::npos);
    CHECK(result.find("GREEN = 2,") != std::string::npos);
}

TEST_CASE("DataScriptRenderer - Constants") {
    ir::bundle bundle;
    bundle.name = "test";
    bundle.constants["MAX_SIZE"] = 1024;
    bundle.constants["MIN_SIZE"] = 16;

    DataScriptRenderer renderer;
    RenderOptions options;
    std::string result = renderer.render_module(bundle, options);

    CHECK(result.find("const uint64 MAX_SIZE = 1024;") != std::string::npos);
    CHECK(result.find("const uint64 MIN_SIZE = 16;") != std::string::npos);
}

TEST_CASE("DataScriptRenderer - Array field") {
    ir::bundle bundle;
    bundle.name = "test";

    ir::struct_def s;
    s.name = "Buffer";

    // Fixed array: uint8 data[16];
    ir::field field;
    field.name = "data";
    field.type.kind = ir::type_kind::array_fixed;
    field.type.array_size = 16;
    field.type.element_type = std::make_unique<ir::type_ref>();
    field.type.element_type->kind = ir::type_kind::uint8;
    field.type.element_type->size_bytes = 1;

    s.fields.push_back(std::move(field));
    bundle.structs.push_back(std::move(s));

    DataScriptRenderer renderer;
    RenderOptions options;
    std::string result = renderer.render_module(bundle, options);

    CHECK(result.find("struct Buffer") != std::string::npos);
    CHECK(result.find("uint8 data[16];") != std::string::npos);
}

TEST_CASE("DataScriptRenderer - Subtype") {
    ir::bundle bundle;
    bundle.name = "test";

    ir::subtype_def subtype;
    subtype.name = "PositiveInt";
    subtype.base_type.kind = ir::type_kind::int32;
    subtype.constraint.type = ir::expr::binary_op;
    subtype.constraint.op = ir::expr::gt;

    // Left side: field reference "this"
    subtype.constraint.left = std::make_unique<ir::expr>();
    subtype.constraint.left->type = ir::expr::field_ref;
    subtype.constraint.left->ref_name = "this";

    // Right side: literal 0
    subtype.constraint.right = std::make_unique<ir::expr>();
    subtype.constraint.right->type = ir::expr::literal_int;
    subtype.constraint.right->int_value = 0;

    bundle.subtypes.push_back(std::move(subtype));

    DataScriptRenderer renderer;
    RenderOptions options;
    std::string result = renderer.render_module(bundle, options);

    CHECK(result.find("subtype PositiveInt : int32") != std::string::npos);
    CHECK(result.find("> 0") != std::string::npos);
}

TEST_CASE("DataScriptRenderer - Keyword sanitization") {
    DataScriptRenderer renderer;

    // Test that DataScript keywords are detected
    CHECK(renderer.is_reserved_keyword("struct") == true);
    CHECK(renderer.is_reserved_keyword("union") == true);
    CHECK(renderer.is_reserved_keyword("uint32") == true);
    CHECK(renderer.is_reserved_keyword("my_field") == false);

    // Test sanitization
    CHECK(renderer.sanitize_identifier("struct") == "struct_");
    CHECK(renderer.sanitize_identifier("my_field") == "my_field");
}

TEST_CASE("DataScriptRenderer - Language metadata") {
    DataScriptRenderer renderer;

    LanguageMetadata meta = renderer.get_metadata();
    CHECK(meta.name == "DataScript");
    CHECK(meta.file_extension == ".ds");
    CHECK(meta.is_case_sensitive == true);
    CHECK(meta.supports_generics == true);
}

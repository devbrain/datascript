#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <doctest/doctest.h>

// Helper to access field from choice case (after parsing, items[0] contains field_def)
static const datascript::ast::field_def& get_case_field(const datascript::ast::choice_case& case_def) {
    REQUIRE(!case_def.items.empty());
    REQUIRE(std::holds_alternative<datascript::ast::field_def>(case_def.items[0]));
    return std::get<datascript::ast::field_def>(case_def.items[0]);
}

/**
 * Unit tests for bug report fixes (December 2025)
 *
 * These tests verify fixes for issues reported in bugreport/DATASCRIPT_BUG_REPORT.md:
 * - Issue 1: Type aliases
 * - Issue 2: Docstrings (confirmed working - missing semicolons in test files)
 * - Issue 3: Module imports
 * - Issue 4: Conditional arrays
 * - Issue 5: Multiple fields in choice cases
 * - Issue 6: Inline discriminator semantics
 */

TEST_SUITE("Bug Report Fixes") {

    // ========================================
    // Issue 4: Conditional Arrays
    // ========================================

    TEST_CASE("Conditional array with if keyword") {
        const char* input = R"(
            struct TestIf {
                uint16 size;
                uint8 data[size] if size > 0;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "TestIf");
        REQUIRE(mod.structs[0].body.size() == 2);

        // Verify first field (size)
        auto* field0 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[0]);
        REQUIRE(field0 != nullptr);
        CHECK(field0->name == "size");
        CHECK_FALSE(field0->condition.has_value());

        // Verify second field (conditional array)
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "data");

        // Verify field has a condition
        REQUIRE(field1->condition.has_value());

        // Verify field type is array
        auto* array_type = std::get_if<datascript::ast::array_type_fixed>(&field1->field_type.node);
        REQUIRE(array_type != nullptr);

        // Verify array element type is uint8
        auto* elem_type = std::get_if<datascript::ast::primitive_type>(&array_type->element_type->node);
        REQUIRE(elem_type != nullptr);
        CHECK(elem_type->is_signed == false);
        CHECK(elem_type->bits == 8);

        // Verify condition is binary expression (size > 0)
        auto* cond_expr = std::get_if<datascript::ast::binary_expr>(&field1->condition->node);
        REQUIRE(cond_expr != nullptr);
        CHECK(cond_expr->op == datascript::ast::binary_op::gt);
    }

    TEST_CASE("Conditional array with optional keyword") {
        const char* input = R"(
            struct TestOptional {
                uint16 size;
                uint8 data[size] optional size > 0;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "TestOptional");
        REQUIRE(mod.structs[0].body.size() == 2);

        // Verify conditional array field
        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "data");

        // Verify field has a condition
        REQUIRE(field1->condition.has_value());

        // Verify field type is array
        auto* array_type = std::get_if<datascript::ast::array_type_fixed>(&field1->field_type.node);
        REQUIRE(array_type != nullptr);
    }

    TEST_CASE("Conditional array with complex condition") {
        const char* input = R"(
            struct DialogItem {
                uint16 creation_data_size;
                uint8 creation_data[creation_data_size] if creation_data_size > 0;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "creation_data");
        REQUIRE(field1->condition.has_value());
    }

    TEST_CASE("Conditional array with docstring") {
        const char* input = R"(
            struct Test {
                uint16 size;

                /** Optional data array */
                uint8 data[size] optional size > 0;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 2);

        auto* field1 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[1]);
        REQUIRE(field1 != nullptr);
        CHECK(field1->name == "data");
        REQUIRE(field1->condition.has_value());

        // Verify docstring is attached
        REQUIRE(field1->docstring.has_value());
        CHECK(field1->docstring.value().find("Optional data array") != std::string::npos);
    }

    TEST_CASE("Multiple conditional arrays in same struct") {
        const char* input = R"(
            struct MultiArray {
                uint16 size1;
                uint16 size2;
                uint8 array1[size1] if size1 > 0;
                uint16 array2[size2] if size2 > 0;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        REQUIRE(mod.structs[0].body.size() == 4);

        // Verify first conditional array
        auto* field2 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[2]);
        REQUIRE(field2 != nullptr);
        CHECK(field2->name == "array1");
        REQUIRE(field2->condition.has_value());

        // Verify second conditional array
        auto* field3 = std::get_if<datascript::ast::field_def>(&mod.structs[0].body[3]);
        REQUIRE(field3 != nullptr);
        CHECK(field3->name == "array2");
        REQUIRE(field3->condition.has_value());
    }

    // ========================================
    // Issue 2: Docstrings (Verify they work correctly)
    // ========================================

    TEST_CASE("Docstring before struct (with semicolon)") {
        const char* input = R"(
            /** This is a struct docstring */
            struct MyStruct {
                uint32 field;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "MyStruct");

        // Verify docstring is attached
        REQUIRE(mod.structs[0].docstring.has_value());
        CHECK(mod.structs[0].docstring.value().find("This is a struct docstring") != std::string::npos);
    }

    TEST_CASE("Docstring before choice (with semicolon)") {
        const char* input = R"(
            /** Choice docstring */
            choice Data : uint16 {
                case 0:
                    uint16 value;
                default:
                    string name;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "Data");

        // Verify docstring is attached
        REQUIRE(mod.choices[0].docstring.has_value());
        CHECK(mod.choices[0].docstring.value().find("Choice docstring") != std::string::npos);
    }

    TEST_CASE("Docstring before enum (with semicolon)") {
        const char* input = R"(
            /** Enum docstring */
            enum uint8 Color {
                RED = 0,
                GREEN = 1
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.enums.size() == 1);
        CHECK(mod.enums[0].name == "Color");

        // Verify docstring is attached
        REQUIRE(mod.enums[0].docstring.has_value());
        CHECK(mod.enums[0].docstring.value().find("Enum docstring") != std::string::npos);
    }

    TEST_CASE("Multiple type definitions with docstrings") {
        const char* input = R"(
            /** First struct */
            struct First {
                uint32 x;
            };

            /** Second struct */
            struct Second {
                uint16 y;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.structs.size() == 2);

        // Verify first struct docstring
        REQUIRE(mod.structs[0].docstring.has_value());
        CHECK(mod.structs[0].docstring.value().find("First struct") != std::string::npos);

        // Verify second struct docstring
        REQUIRE(mod.structs[1].docstring.has_value());
        CHECK(mod.structs[1].docstring.value().find("Second struct") != std::string::npos);
    }

    // ========================================
    // Issue 5: Multiple Fields in Choice Cases (Struct Workaround)
    // ========================================

    TEST_CASE("Choice with struct field (multi-field workaround)") {
        const char* input = R"(
            struct OrdinalData {
                uint16 marker;
                uint16 ordinal;
            };

            choice ResourceNameOrId : uint16 {
                case 0xFFFF:
                    OrdinalData data;
                default:
                    string name;
            };
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        // Verify struct was parsed
        REQUIRE(mod.structs.size() == 1);
        CHECK(mod.structs[0].name == "OrdinalData");
        REQUIRE(mod.structs[0].body.size() == 2);

        // Verify choice was parsed
        REQUIRE(mod.choices.size() == 1);
        CHECK(mod.choices[0].name == "ResourceNameOrId");
        REQUIRE(mod.choices[0].cases.size() == 2);

        // Verify first case uses struct type
        auto& case0 = mod.choices[0].cases[0];
        CHECK(case0.case_exprs.size() == 1);
        CHECK_FALSE(case0.is_default);

        // Check that field type is a qualified name (OrdinalData)
        auto* field_type = std::get_if<datascript::ast::qualified_name>(&get_case_field(case0).field_type.node);
        REQUIRE(field_type != nullptr);
        REQUIRE(field_type->parts.size() == 1);
        CHECK(field_type->parts[0] == "OrdinalData");
    }

    // ========================================
    // Issue 1: Type Aliases (Parsing)
    // ========================================

    TEST_CASE("Type alias parsing - simple") {
        const char* input = R"(
            typedef DWORD = uint32;
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.type_aliases.size() == 1);
        CHECK(mod.type_aliases[0].name == "DWORD");

        // Verify target type is uint32
        auto* target_type = std::get_if<datascript::ast::primitive_type>(&mod.type_aliases[0].target_type.node);
        REQUIRE(target_type != nullptr);
        CHECK(target_type->is_signed == false);
        CHECK(target_type->bits == 32);
    }

    TEST_CASE("Type alias parsing - multiple aliases") {
        const char* input = R"(
            typedef DWORD = uint32;
            typedef WORD = uint16;
            typedef BYTE = uint8;
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.type_aliases.size() == 3);
        CHECK(mod.type_aliases[0].name == "DWORD");
        CHECK(mod.type_aliases[1].name == "WORD");
        CHECK(mod.type_aliases[2].name == "BYTE");
    }

    TEST_CASE("Type alias parsing - with docstring") {
        const char* input = R"(
            /** Windows DWORD type */
            typedef DWORD = uint32;
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.type_aliases.size() == 1);
        CHECK(mod.type_aliases[0].name == "DWORD");

        // Verify docstring
        REQUIRE(mod.type_aliases[0].docstring.has_value());
        CHECK(mod.type_aliases[0].docstring.value().find("Windows DWORD") != std::string::npos);
    }

    TEST_CASE("Type alias parsing - complex types") {
        const char* input = R"(
            typedef StringArray = string[];
            typedef FixedBuffer = uint8[256];
        )";

        auto mod = datascript::parse_datascript(std::string(input));

        REQUIRE(mod.type_aliases.size() == 2);

        // Verify first alias (string[])
        CHECK(mod.type_aliases[0].name == "StringArray");
        auto* array1 = std::get_if<datascript::ast::array_type_unsized>(&mod.type_aliases[0].target_type.node);
        REQUIRE(array1 != nullptr);

        // Verify second alias (uint8[256])
        CHECK(mod.type_aliases[1].name == "FixedBuffer");
        auto* array2 = std::get_if<datascript::ast::array_type_fixed>(&mod.type_aliases[1].target_type.node);
        REQUIRE(array2 != nullptr);
    }

    TEST_CASE("Type alias to struct - IR generation") {
        const char* input = R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            typedef Position = Point;

            struct Container {
                Position pos;
                Point direct;
            };
        )";

        auto parsed = datascript::parse_datascript(std::string(input));

        datascript::module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = datascript::semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir_bundle = datascript::ir::build_ir(analysis.analyzed.value());

        // Find Container struct in IR
        const datascript::ir::struct_def* container = nullptr;
        for (const auto& s : ir_bundle.structs) {
            if (s.name == "Container") {
                container = &s;
                break;
            }
        }

        REQUIRE(container != nullptr);
        REQUIRE(container->fields.size() == 2);

        // Both fields should reference the Point struct
        auto& pos_field = container->fields[0];
        auto& direct_field = container->fields[1];

        CHECK(pos_field.name == "pos");
        CHECK(direct_field.name == "direct");

        // CRITICAL: Both should have same type (Point struct)
        // The typedef should be resolved to Point
        CHECK(pos_field.type.type_index == direct_field.type.type_index);
        CHECK(pos_field.type.kind == datascript::ir::type_kind::struct_type);
        CHECK(direct_field.type.kind == datascript::ir::type_kind::struct_type);
    }

}


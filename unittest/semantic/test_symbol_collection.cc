//
// Tests for Phase 1: Symbol Collection
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>

using namespace datascript;
using namespace datascript::semantic;

namespace {
    // Helper: create a module_set from a single source string
    module_set make_module_set(const std::string& source) {
        auto main_mod = parse_datascript(source);

        module_set modules;
        modules.main.module = std::move(main_mod);
        modules.main.file_path = "<test>";
        modules.main.package_name = "";  // Default package

        return modules;
    }
}

TEST_SUITE("Semantic Analysis - Symbol Collection") {
    TEST_CASE("Empty module has no symbols") {
        auto modules = make_module_set("");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        CHECK(symbols.main.constants.empty());
        CHECK(symbols.main.structs.empty());
        CHECK(symbols.main.unions.empty());
        CHECK(symbols.main.enums.empty());
        CHECK(symbols.main.choices.empty());
        CHECK(symbols.main.constraints.empty());
    }

    TEST_CASE("Collect constants") {
        auto modules = make_module_set(R"(
            const uint32 FIRST = 1;
            const uint32 SECOND = 2;
            const uint32 THIRD = 3;
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.constants.size() == 3);
        CHECK(symbols.main.constants.contains("FIRST"));
        CHECK(symbols.main.constants.contains("SECOND"));
        CHECK(symbols.main.constants.contains("THIRD"));
    }

    TEST_CASE("Collect structs") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Rectangle {
                uint32 width;
                uint32 height;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.structs.size() == 2);
        CHECK(symbols.main.structs.contains("Point"));
        CHECK(symbols.main.structs.contains("Rectangle"));
    }

    TEST_CASE("Collect enums") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };

            enum uint8 Color {
                RED = 0,
                GREEN = 1,
                BLUE = 2
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.enums.size() == 2);
        CHECK(symbols.main.enums.contains("Status"));
        CHECK(symbols.main.enums.contains("Color"));
    }

    TEST_CASE("Collect unions") {
        auto modules = make_module_set(R"(
            union Value {
                uint32 int_val;
                string str_val;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.unions.size() == 1);
        CHECK(symbols.main.unions.contains("Value"));
    }

    TEST_CASE("Collect choices") {
        auto modules = make_module_set(R"(
            choice MyChoice on selector {
                case 1: uint32 int_field;
                case 2: string str_field;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.choices.size() == 1);
        CHECK(symbols.main.choices.contains("MyChoice"));
    }

    TEST_CASE("Collect constraints") {
        auto modules = make_module_set(R"(
            constraint InRange(uint32 value) {
                value >= 0 && value <= 100
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());
        REQUIRE(symbols.main.constraints.size() == 1);
        CHECK(symbols.main.constraints.contains("InRange"));
    }

    TEST_CASE("Duplicate constant definition error") {
        auto modules = make_module_set(R"(
            const uint32 X = 1;
            const uint32 X = 2;
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        REQUIRE(diags.size() == 1);
        CHECK(diags[0].level == diagnostic_level::error);
        CHECK(diags[0].code == std::string(diag_codes::E_DUPLICATE_DEFINITION));
        CHECK(diags[0].message.find("Constant 'X' is already defined in this module") != std::string::npos);
        CHECK(diags[0].related_position.has_value());
    }

    TEST_CASE("Duplicate struct definition error") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Point {
                uint32 a;
                uint32 b;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        REQUIRE(diags.size() == 1);
        CHECK(diags[0].level == diagnostic_level::error);
        CHECK(diags[0].code == std::string(diag_codes::E_DUPLICATE_DEFINITION));
        CHECK(diags[0].message.find("Struct 'Point' is already defined in this module") != std::string::npos);
    }

    TEST_CASE("Multiple errors reported (not stop on first)") {
        auto modules = make_module_set(R"(
            const uint32 X = 1;
            const uint32 X = 2;

            struct Point {
                uint32 x;
            };
            struct Point {
                uint32 y;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        REQUIRE(diags.size() == 2);
        CHECK(diags[0].message.find("Constant") != std::string::npos);
        CHECK(diags[1].message.find("Struct") != std::string::npos);
    }

    TEST_CASE("Lookup unqualified symbols") {
        auto modules = make_module_set(R"(
            const uint32 MY_CONST = 42;

            struct MyStruct {
                uint32 field;
            };

            union MyUnion {
                uint32 a;
                uint32 b;
            };

            enum uint8 MyEnum {
                A = 0
            };

            choice MyChoice on selector {
                case 1: uint32 field;
            };

            constraint MyConstraint(uint32 x) {
                x > 0
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        CHECK(diags.empty());

        // Test lookups
        CHECK(symbols.find_constant("MY_CONST") != nullptr);
        CHECK(symbols.find_struct("MyStruct") != nullptr);
        CHECK(symbols.find_union("MyUnion") != nullptr);
        CHECK(symbols.find_enum("MyEnum") != nullptr);
        CHECK(symbols.find_choice("MyChoice") != nullptr);
        CHECK(symbols.find_constraint("MyConstraint") != nullptr);

        // Test non-existent lookups
        CHECK(symbols.find_constant("NONEXISTENT") == nullptr);
        CHECK(symbols.find_struct("NonExistent") == nullptr);
    }

    TEST_CASE("Qualified lookup") {
        auto modules = make_module_set(R"(
            const uint32 LOCAL = 1;
            struct LocalStruct {
                uint32 field;
            };
        )");

        std::vector<diagnostic> diags;
        auto symbols = phases::collect_symbols(modules, diags);

        // Test unqualified lookup (single-part name)
        auto local_struct = symbols.find_struct_qualified({"LocalStruct"});
        REQUIRE(local_struct != nullptr);
        CHECK(local_struct->name == "LocalStruct");

        // Test qualified lookup with empty parts
        CHECK(symbols.find_struct_qualified({}) == nullptr);
    }
}

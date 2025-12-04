//
// Tests for Phase 2: Name Resolution
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
        modules.main.package_name = "";

        return modules;
    }
}

TEST_SUITE("Semantic Analysis - Name Resolution") {
    TEST_CASE("Resolve struct type in field") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Container {
                Point origin;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Check that Point was resolved
        auto& analyzed = result.analyzed.value();

        // Find the qualified_name in Container's field type
        auto& container_def = modules.main.module.structs[1];
        auto* field = std::get_if<ast::field_def>(&container_def.body[0]);
        REQUIRE(field != nullptr);
        if (auto* qname = std::get_if<ast::qualified_name>(&field->field_type.node)) {
            CHECK(analyzed.resolved_types.contains(qname));

            auto& resolved = analyzed.resolved_types[qname];
            auto* struct_def = std::get_if<const ast::struct_def*>(&resolved);
            REQUIRE(struct_def != nullptr);
            CHECK((*struct_def)->name == "Point");
        }
    }

    TEST_CASE("Resolve struct type in field") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Line {
                Point start;
                Point end;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Both Point references should be resolved
        auto& analyzed = result.analyzed.value();

        // Should have 2 resolutions (start and end fields)
        size_t point_resolutions = 0;
        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* struct_def = std::get_if<const ast::struct_def*>(&resolved)) {
                if ((*struct_def)->name == "Point") {
                    point_resolutions++;
                }
            }
        }

        CHECK(point_resolutions == 2);
    }

    TEST_CASE("Resolve enum base type") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Enum with integer base type should pass all validation
        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.symbols.main.enums.size() == 1);
    }

    TEST_CASE("Undefined type error") {
        auto modules = make_module_set(R"(
            struct Container {
                NonExistentType field;
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());
        REQUIRE(result.error_count() == 1);

        auto errors = result.get_errors();
        auto& error = errors[0];
        CHECK(error.code == std::string(diag_codes::E_UNDEFINED_TYPE));
        CHECK(error.message.find("NonExistentType") != std::string::npos);
        CHECK(error.message.find("not found") != std::string::npos);
        CHECK(error.suggestion.has_value());
    }

    TEST_CASE("Multiple undefined types") {
        auto modules = make_module_set(R"(
            struct Container {
                TypeA field1;
                TypeB field2;
                TypeC field3;
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());
        CHECK(result.error_count() == 3);

        // All should be E_UNDEFINED_TYPE errors
        for (const auto& error : result.get_errors()) {
            CHECK(error.code == std::string(diag_codes::E_UNDEFINED_TYPE));
        }
    }

    TEST_CASE("Resolve array element type") {
        auto modules = make_module_set(R"(
            struct Point {
                uint32 x;
                uint32 y;
            };

            struct Path {
                Point[10] points;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Point (as array element type) should be resolved
        auto& analyzed = result.analyzed.value();

        bool found_point_resolution = false;
        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* struct_def = std::get_if<const ast::struct_def*>(&resolved)) {
                if ((*struct_def)->name == "Point") {
                    found_point_resolution = true;
                    break;
                }
            }
        }

        CHECK(found_point_resolution);
    }

    TEST_CASE("Resolve union type") {
        auto modules = make_module_set(R"(
            union Value {
                uint32 int_val;
                string str_val;
            };

            struct Container {
                Value data;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Value should be resolved to union
        auto& analyzed = result.analyzed.value();

        bool found_union = false;
        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* union_def = std::get_if<const ast::union_def*>(&resolved)) {
                if ((*union_def)->name == "Value") {
                    found_union = true;
                    break;
                }
            }
        }

        CHECK(found_union);
    }

    TEST_CASE("Resolve choice type") {
        auto modules = make_module_set(R"(
            choice MyChoice on selector {
                case 1: uint32 int_field;
                case 2: string str_field;
            };

            struct Container {
                MyChoice data;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // MyChoice should be resolved to choice
        auto& analyzed = result.analyzed.value();

        bool found_choice = false;
        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* choice_def = std::get_if<const ast::choice_def*>(&resolved)) {
                if ((*choice_def)->name == "MyChoice") {
                    found_choice = true;
                    break;
                }
            }
        }

        CHECK(found_choice);
    }

    TEST_CASE("Primitive types don't need resolution") {
        auto modules = make_module_set(R"(
            const uint32 VALUE = 42;
            const string NAME = "test";
            const bool FLAG = true;
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // No resolutions needed for primitive types
        auto& analyzed = result.analyzed.value();
        CHECK(analyzed.resolved_types.empty());
    }

    TEST_CASE("Nested struct resolution") {
        auto modules = make_module_set(R"(
            struct Inner {
                uint32 value;
            };

            struct Middle {
                Inner data;
            };

            struct Outer {
                Middle nested;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Both Inner and Middle should be resolved
        auto& analyzed = result.analyzed.value();

        int inner_count = 0;
        int middle_count = 0;

        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* struct_def = std::get_if<const ast::struct_def*>(&resolved)) {
                if ((*struct_def)->name == "Inner") inner_count++;
                if ((*struct_def)->name == "Middle") middle_count++;
            }
        }

        CHECK(inner_count == 1);
        CHECK(middle_count == 1);
    }

    TEST_CASE("Array types with undefined element") {
        auto modules = make_module_set(R"(
            struct Container {
                NonExistent[10] items;
            };
        )");

        auto result = analyze(modules);

        REQUIRE(result.has_errors());
        CHECK(result.error_count() == 1);

        auto errors = result.get_errors();
        auto& error = errors[0];
        CHECK(error.code == std::string(diag_codes::E_UNDEFINED_TYPE));
        CHECK(error.message.find("NonExistent") != std::string::npos);
    }

    TEST_CASE("Resolve enum type in field") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };

            struct Container {
                Status current;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Status type should be resolved
        auto& analyzed = result.analyzed.value();

        bool found_enum = false;
        for (const auto& [qname, resolved] : analyzed.resolved_types) {
            if (auto* enum_def = std::get_if<const ast::enum_def*>(&resolved)) {
                if ((*enum_def)->name == "Status") {
                    found_enum = true;
                    break;
                }
            }
        }

        CHECK(found_enum);
    }
}

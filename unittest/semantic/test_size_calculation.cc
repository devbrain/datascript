//
// Tests for Phase 5: Size and Layout Calculation
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

TEST_SUITE("Semantic Analysis - Size Calculation") {
    TEST_CASE("Calculate struct field offsets") {
        auto modules = make_module_set(R"(
            struct Point {
                uint8 x;
                uint8 y;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);

        // Check field offsets
        CHECK(analyzed.field_offsets.contains(field0));  // x
        CHECK(analyzed.field_offsets.contains(field1));  // y

        CHECK(analyzed.field_offsets[field0] == 0);  // x at offset 0
        CHECK(analyzed.field_offsets[field1] == 1);  // y at offset 1
    }

    TEST_CASE("Calculate struct with aligned fields") {
        auto modules = make_module_set(R"(
            struct Aligned {
                uint8 a;
                uint32 b;
                uint8 c;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);
        auto* field2 = std::get_if<ast::field_def>(&struct_def.body[2]);
        REQUIRE(field2 != nullptr);

        // Check field offsets with alignment
        // a: uint8 at offset 0
        // padding: 3 bytes
        // b: uint32 at offset 4 (aligned to 4)
        // c: uint8 at offset 8
        CHECK(analyzed.field_offsets[field0] == 0);  // a
        CHECK(analyzed.field_offsets[field1] == 4);  // b (aligned)
        CHECK(analyzed.field_offsets[field2] == 8);  // c
    }

    TEST_CASE("Calculate struct with various integer types") {
        auto modules = make_module_set(R"(
            struct Mixed {
                uint8 byte;
                uint16 word;
                uint32 dword;
                uint64 qword;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);
        auto* field2 = std::get_if<ast::field_def>(&struct_def.body[2]);
        REQUIRE(field2 != nullptr);
        auto* field3 = std::get_if<ast::field_def>(&struct_def.body[3]);
        REQUIRE(field3 != nullptr);

        // byte: 1 byte at offset 0
        // padding: 1 byte
        // word: 2 bytes at offset 2 (aligned to 2)
        // dword: 4 bytes at offset 4 (aligned to 4)
        // qword: 8 bytes at offset 8 (aligned to 8)
        CHECK(analyzed.field_offsets[field0] == 0);  // byte
        CHECK(analyzed.field_offsets[field1] == 2);  // word
        CHECK(analyzed.field_offsets[field2] == 4);  // dword
        CHECK(analyzed.field_offsets[field3] == 8);  // qword
    }

    TEST_CASE("Calculate union field offsets") {
        auto modules = make_module_set(R"(
            union Value {
                uint32 int_val;
                string str_val;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& union_def = modules.main.module.unions[0];

        // All union fields start at offset 0 (simple fields become single-field cases)
        CHECK(analyzed.field_offsets[&std::get<ast::field_def>(union_def.cases[0].items[0])] == 0);  // int_val
        CHECK(analyzed.field_offsets[&std::get<ast::field_def>(union_def.cases[1].items[0])] == 0);  // str_val
    }

    TEST_CASE("Calculate nested struct offsets") {
        auto modules = make_module_set(R"(
            struct Inner {
                uint32 value;
            };

            struct Outer {
                uint8 flag;
                Inner data;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& outer_struct = modules.main.module.structs[1];

        // Extract field pointer from body
        auto* field0 = std::get_if<ast::field_def>(&outer_struct.body[0]);
        REQUIRE(field0 != nullptr);

        // flag: 1 byte at offset 0
        // padding: 3 bytes
        // data: Inner (4 bytes) at offset 4 (aligned to 4)
        CHECK(analyzed.field_offsets[field0] == 0);  // flag
        // Note: data offset depends on Inner's alignment (which is 4 for uint32)
        // We expect offset 4 due to alignment
    }

    TEST_CASE("Calculate struct with bool field") {
        auto modules = make_module_set(R"(
            struct Flags {
                bool enabled;
                uint32 value;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);

        // enabled: 1 byte at offset 0
        // padding: 3 bytes
        // value: 4 bytes at offset 4 (aligned to 4)
        CHECK(analyzed.field_offsets[field0] == 0);  // enabled
        CHECK(analyzed.field_offsets[field1] == 4);  // value
    }

    TEST_CASE("Calculate enum size from base type") {
        auto modules = make_module_set(R"(
            enum uint8 Status {
                OK = 0,
                ERROR = 1
            };

            struct Container {
                Status status;
                uint32 data;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);

        // status: 1 byte at offset 0 (enum uint8)
        // padding: 3 bytes
        // data: 4 bytes at offset 4 (aligned to 4)
        CHECK(analyzed.field_offsets[field0] == 0);  // status
        CHECK(analyzed.field_offsets[field1] == 4);  // data
    }

    TEST_CASE("Simple struct with no padding") {
        auto modules = make_module_set(R"(
            struct Packed {
                uint8 a;
                uint8 b;
                uint8 c;
                uint8 d;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);
        auto* field2 = std::get_if<ast::field_def>(&struct_def.body[2]);
        REQUIRE(field2 != nullptr);
        auto* field3 = std::get_if<ast::field_def>(&struct_def.body[3]);
        REQUIRE(field3 != nullptr);

        // All fields are uint8, so no padding needed
        CHECK(analyzed.field_offsets[field0] == 0);  // a
        CHECK(analyzed.field_offsets[field1] == 1);  // b
        CHECK(analyzed.field_offsets[field2] == 2);  // c
        CHECK(analyzed.field_offsets[field3] == 3);  // d
    }

    TEST_CASE("Struct with all 64-bit fields") {
        auto modules = make_module_set(R"(
            struct Large {
                uint64 a;
                uint64 b;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);

        // Both fields are uint64 (8 bytes), aligned to 8
        CHECK(analyzed.field_offsets[field0] == 0);   // a
        CHECK(analyzed.field_offsets[field1] == 8);   // b
    }

    TEST_CASE("Empty struct") {
        auto modules = make_module_set(R"(
            struct Empty {
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());
        // Empty struct is valid, size would be 0 or 1 depending on convention
    }

    TEST_CASE("Union with different sized fields") {
        auto modules = make_module_set(R"(
            union Mixed {
                uint8 byte_val;
                uint32 int_val;
                uint64 long_val;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& union_def = modules.main.module.unions[0];

        // All union fields at offset 0 (simple fields become single-field cases)
        CHECK(analyzed.field_offsets[&std::get<ast::field_def>(union_def.cases[0].items[0])] == 0);  // byte_val
        CHECK(analyzed.field_offsets[&std::get<ast::field_def>(union_def.cases[1].items[0])] == 0);  // int_val
        CHECK(analyzed.field_offsets[&std::get<ast::field_def>(union_def.cases[2].items[0])] == 0);  // long_val
        // Union size = max(1, 4, 8) = 8 bytes
    }

    TEST_CASE("Struct with signed integers") {
        auto modules = make_module_set(R"(
            struct Signed {
                int8 a;
                int16 b;
                int32 c;
            };
        )");

        auto result = analyze(modules);

        CHECK_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        auto& analyzed = result.analyzed.value();
        auto& struct_def = modules.main.module.structs[0];

        // Extract field pointers from body
        auto* field0 = std::get_if<ast::field_def>(&struct_def.body[0]);
        REQUIRE(field0 != nullptr);
        auto* field1 = std::get_if<ast::field_def>(&struct_def.body[1]);
        REQUIRE(field1 != nullptr);
        auto* field2 = std::get_if<ast::field_def>(&struct_def.body[2]);
        REQUIRE(field2 != nullptr);

        // a: 1 byte at offset 0
        // padding: 1 byte
        // b: 2 bytes at offset 2 (aligned to 2)
        // c: 4 bytes at offset 4 (aligned to 4)
        CHECK(analyzed.field_offsets[field0] == 0);  // a
        CHECK(analyzed.field_offsets[field1] == 2);  // b
        CHECK(analyzed.field_offsets[field2] == 4);  // c
    }
}

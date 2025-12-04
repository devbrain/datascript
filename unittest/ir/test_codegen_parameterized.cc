//
// Parameterized types code generation tests
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Parameterized Types") {
    TEST_CASE("Simple parameterized struct") {
        const char* schema = R"(
struct Buffer(uint16 size) {
    uint8 data[size];
};

struct Message {
    uint32 magic;
    Buffer(16) small;
    Buffer(256) large;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            // Should have Message + Buffer_16 + Buffer_256 (not Buffer itself)
            CHECK(ir.structs.size() == 3);

            // Find the concrete Buffer structs
            bool found_buffer_16 = false;
            bool found_buffer_256 = false;
            bool found_message = false;

            for (const auto& s : ir.structs) {
                if (s.name == "Buffer_16") {
                    found_buffer_16 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "data");
                }
                else if (s.name == "Buffer_256") {
                    found_buffer_256 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "data");
                }
                else if (s.name == "Message") {
                    found_message = true;
                    CHECK(s.fields.size() == 3);
                }
            }

            CHECK(found_buffer_16);
            CHECK(found_buffer_256);
            CHECK(found_message);
        }

        SUBCASE("Code generation") {
            codegen::cpp_options opts;
            opts.namespace_name = "test";

            std::string cpp_code = codegen::generate_cpp_header(ir, opts);

            // Verify concrete structs are generated
            CHECK(cpp_code.find("struct Buffer_16") != std::string::npos);
            CHECK(cpp_code.find("struct Buffer_256") != std::string::npos);

            // Verify the original parameterized struct is NOT in the output
            CHECK(cpp_code.find("struct Buffer {") == std::string::npos);

            // Verify Message uses the concrete types
            CHECK(cpp_code.find("struct Message") != std::string::npos);
        }
    }

    TEST_CASE("Reuse of identical instantiations") {
        const char* schema = R"(
struct Record(uint16 count) {
    uint8 items[count];
};

struct Data {
    Record(10) first;
    Record(20) second;
    Record(10) third;  // Should reuse Record_10
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            // Should have Data + Record_10 + Record_20 (Record_10 is reused)
            CHECK(ir.structs.size() == 3);

            bool found_record_10 = false;
            bool found_record_20 = false;

            for (const auto& s : ir.structs) {
                if (s.name == "Record_10") {
                    found_record_10 = true;
                }
                else if (s.name == "Record_20") {
                    found_record_20 = true;
                }
            }

            CHECK(found_record_10);
            CHECK(found_record_20);
        }
    }

    TEST_CASE("Multiple parameters") {
        const char* schema = R"(
struct Matrix(uint8 rows, uint8 cols) {
    uint32 values[rows * cols];
};

struct Container {
    Matrix(4, 4) transform;
    Matrix(3, 3) rotation;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            // Should have Container + Matrix_4_4 + Matrix_3_3
            CHECK(ir.structs.size() == 3);

            bool found_matrix_4_4 = false;
            bool found_matrix_3_3 = false;

            for (const auto& s : ir.structs) {
                if (s.name == "Matrix_4_4") {
                    found_matrix_4_4 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "values");
                }
                else if (s.name == "Matrix_3_3") {
                    found_matrix_3_3 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "values");
                }
            }

            CHECK(found_matrix_4_4);
            CHECK(found_matrix_3_3);
        }
    }

    TEST_CASE("Nested parameterized types") {
        const char* schema = R"(
struct Inner(uint8 size) {
    uint8 data[size];
};

struct Outer(uint16 count) {
    Inner(8) items[count];
};

struct Container {
    Outer(4) small;
    Outer(16) large;
};
)";

        auto parsed = parse_datascript(std::string(schema));
        module_set modules;
        modules.main.file_path = "test.ds";
        modules.main.module = std::move(parsed);
        modules.main.package_name = "test";

        auto analysis = semantic::analyze(modules);
        REQUIRE_FALSE(analysis.has_errors());

        auto ir = ir::build_ir(analysis.analyzed.value());

        SUBCASE("IR structure") {
            // Should have Container + Inner_8 + Outer_4 + Outer_16
            // (NOT Inner or Outer base types)
            CHECK(ir.structs.size() == 4);

            bool found_container = false;
            bool found_inner_8 = false;
            bool found_outer_4 = false;
            bool found_outer_16 = false;
            bool found_inner_base = false;
            bool found_outer_base = false;

            for (const auto& s : ir.structs) {
                if (s.name == "Container") {
                    found_container = true;
                    CHECK(s.fields.size() == 2);
                }
                else if (s.name == "Inner_8") {
                    found_inner_8 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "data");
                }
                else if (s.name == "Outer_4") {
                    found_outer_4 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "items");
                }
                else if (s.name == "Outer_16") {
                    found_outer_16 = true;
                    CHECK(s.fields.size() == 1);
                    CHECK(s.fields[0].name == "items");
                }
                else if (s.name == "Inner") {
                    found_inner_base = true;
                }
                else if (s.name == "Outer") {
                    found_outer_base = true;
                }
            }

            CHECK(found_container);
            CHECK(found_inner_8);
            CHECK(found_outer_4);
            CHECK(found_outer_16);
            CHECK_FALSE(found_inner_base);  // Base types should NOT be in IR
            CHECK_FALSE(found_outer_base);
        }
    }
}

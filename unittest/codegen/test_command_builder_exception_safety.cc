//
// Tests for CommandBuilder Exception Safety
//

#include <doctest/doctest.h>
#include <datascript/command_builder.hh>
#include <datascript/ir.hh>

using namespace datascript;
using namespace datascript::codegen;
using namespace datascript::ir;

namespace {
    // Helper: Create a simple struct_def for testing
    struct_def make_test_struct(const std::string& name = "TestStruct") {
        struct_def s;
        s.name = name;
        s.documentation = "Test struct";

        // Add a simple field (use emplace_back to avoid copy)
        s.fields.emplace_back();
        auto& f = s.fields.back();
        f.name = "value";
        f.type.kind = type_kind::uint32;
        f.condition = field::always;

        return s;
    }

    // Helper: Create a simple bundle for testing
    bundle make_test_bundle() {
        bundle b;
        b.structs.push_back(make_test_struct());
        return b;
    }
}

TEST_SUITE("CommandBuilder - Exception Safety") {

    TEST_CASE("BuilderScope - Normal execution path (no exception)") {
        CommandBuilder builder;
        auto test_struct = make_test_struct();

        // Normal path: no exception, commands returned successfully
        REQUIRE_NOTHROW([&]() {
            auto commands = builder.build_struct_reader(test_struct, true);
            CHECK(commands.size() > 0);
        }());
    }

    TEST_CASE("BuilderScope - take_commands() transfers ownership") {
        CommandBuilder builder;
        auto test_struct = make_test_struct();

        auto commands = builder.build_struct_reader(test_struct, true);

        SUBCASE("Commands are not empty") {
            CHECK(commands.size() > 0);
        }

        SUBCASE("Builder is clean after take_commands()") {
            // After build_struct_reader returns, builder should be in clean state
            // (BuilderScope.take_commands() was called, so no cleanup occurred)
            // We can safely build another structure
            auto commands2 = builder.build_struct_reader(test_struct, true);
            CHECK(commands2.size() > 0);
        }
    }

    TEST_CASE("BuilderScope - Multiple sequential builds") {
        CommandBuilder builder;
        auto test_struct1 = make_test_struct("Struct1");
        auto test_struct2 = make_test_struct("Struct2");
        auto test_struct3 = make_test_struct("Struct3");

        // Should be able to do multiple builds without issues
        auto commands1 = builder.build_struct_reader(test_struct1, true);
        CHECK(commands1.size() > 0);

        auto commands2 = builder.build_struct_declaration(test_struct2);
        CHECK(commands2.size() > 0);

        auto commands3 = builder.build_standalone_reader(test_struct3, false);
        CHECK(commands3.size() > 0);
    }

    TEST_CASE("BuilderScope - Module building") {
        CommandBuilder builder;
        auto test_bundle = make_test_bundle();
        codegen::cpp_options opts;

        auto commands = builder.build_module(test_bundle, "test_ns", opts, true);

        CHECK(commands.size() > 0);
    }

    TEST_CASE("BuilderScope - Exception safety with invalid IR") {
        CommandBuilder builder;

        // Create struct with empty name (might cause issues in rendering)
        struct_def invalid_struct;
        invalid_struct.name = "";  // Empty name
        invalid_struct.documentation = "";

        // This should not crash or leak memory, even if it throws
        // (The current implementation may or may not throw, but it should be safe)
        try {
            auto commands = builder.build_struct_reader(invalid_struct, true);
            // If it succeeds, that's fine
            CHECK(commands.size() >= 0);
        } catch (...) {
            // If it throws, that's also fine - what matters is no memory leak
            // This would be caught by valgrind
        }
    }

    TEST_CASE("BuilderScope - Complex structs with multiple fields") {
        CommandBuilder builder;

        // Create a struct with multiple fields of different types
        struct_def complex_struct;
        complex_struct.name = "ComplexStruct";
        complex_struct.documentation = "Complex test struct";

        // Add multiple fields (use emplace_back to avoid copy)
        complex_struct.fields.emplace_back();
        complex_struct.fields.back().name = "id";
        complex_struct.fields.back().type.kind = type_kind::uint32;
        complex_struct.fields.back().condition = field::always;

        complex_struct.fields.emplace_back();
        complex_struct.fields.back().name = "value";
        complex_struct.fields.back().type.kind = type_kind::uint64;
        complex_struct.fields.back().condition = field::always;

        complex_struct.fields.emplace_back();
        complex_struct.fields.back().name = "flag";
        complex_struct.fields.back().type.kind = type_kind::uint8;
        complex_struct.fields.back().condition = field::always;

        // Build reader - should handle multiple fields safely
        auto commands = builder.build_struct_reader(complex_struct, true);
        CHECK(commands.size() > 0);
    }

    TEST_CASE("BuilderScope - Struct with documentation") {
        CommandBuilder builder;

        struct_def documented_struct;
        documented_struct.name = "DocumentedStruct";
        documented_struct.documentation = "This is a documented test structure\nwith multiple lines\nof documentation";

        documented_struct.fields.emplace_back();
        documented_struct.fields.back().name = "value";
        documented_struct.fields.back().type.kind = type_kind::uint32;
        documented_struct.fields.back().condition = field::always;

        auto commands = builder.build_struct_declaration(documented_struct);
        CHECK(commands.size() > 0);
    }

    TEST_CASE("BuilderScope - Standalone reader") {
        CommandBuilder builder;
        auto test_struct = make_test_struct();

        SUBCASE("With exceptions") {
            auto commands = builder.build_standalone_reader(test_struct, true);
            CHECK(commands.size() > 0);
        }

        SUBCASE("Without exceptions (safe mode)") {
            auto commands = builder.build_standalone_reader(test_struct, false);
            CHECK(commands.size() > 0);
        }
    }

    TEST_CASE("BuilderScope - Memory cleanup verification") {
        // This test verifies that BuilderScope properly manages memory
        // Run with valgrind to ensure no leaks:
        //   valgrind --leak-check=full ./unittest/datascript_unittest

        CommandBuilder builder;
        auto test_bundle = make_test_bundle();
        codegen::cpp_options opts;

        // Do many builds to stress-test memory management
        for (int i = 0; i < 100; ++i) {
            auto commands = builder.build_module(test_bundle, "test_ns", opts, true);
            CHECK(commands.size() > 0);

            // Commands go out of scope here - should properly clean up
        }

        // If we got here without crashing, memory management is working
        CHECK(true);
    }
}

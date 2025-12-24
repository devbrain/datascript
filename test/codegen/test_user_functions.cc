//
// Tests for user-defined function code generation
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>
#include <string>
#include <iostream>

using namespace datascript;

// Helper function to parse, analyze, convert to IR, and generate code
static std::string generate_code(const std::string& source, bool exceptions = true) {
    // Parse
    auto parsed = parse_datascript(std::string(source));

    // Semantic analysis
    module_set modules;
    modules.main.module = std::move(parsed);
    modules.main.file_path = "test.ds";
    modules.main.package_name = "test";

    auto analysis = semantic::analyze(modules);
    REQUIRE_FALSE( analysis.has_errors() );

    // Convert to IR
    auto ir_module = ir::build_ir(analysis.analyzed.value());

    // Generate code
    codegen::cpp_options opts;
    opts.namespace_name = "test";
    opts.error_handling = exceptions ? codegen::cpp_options::exceptions_only : codegen::cpp_options::results_only;

    std::string code = codegen::generate_cpp_header(ir_module, opts);
    return code;
}

TEST_CASE("Simple function with no parameters") {
    std::string source = R"(
        struct Header {
            uint8 flags;

            function bool is_compressed() {
                return (flags & 1) != 0;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify struct is generated
    CHECK( code.find("struct Header {") != std::string::npos );

    // Verify field is generated
    CHECK( code.find("uint8_t flags;") != std::string::npos );

    // Verify function signature with const qualifier
    CHECK( code.find("bool is_compressed() const {") != std::string::npos );

    // Verify function body with return statement
    CHECK( code.find("return ((flags & 1) != 0);") != std::string::npos );
}

TEST_CASE("Function with single parameter") {
    std::string source = R"(
        struct Data {
            uint8 value;

            function bool is_greater_than(uint8 threshold) {
                return value > threshold;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function signature with parameter
    CHECK( code.find("bool is_greater_than(uint8_t threshold) const {") != std::string::npos );

    // Verify function body
    CHECK( code.find("return (value > threshold);") != std::string::npos );
}

TEST_CASE("Function with multiple parameters") {
    std::string source = R"(
        struct Rectangle {
            uint16 width;
            uint16 height;

            function uint32 calculate_area(uint16 scale_x, uint16 scale_y) {
                return (width * scale_x) * (height * scale_y);
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function signature with multiple parameters
    CHECK( code.find("uint32_t calculate_area(uint16_t scale_x, uint16_t scale_y) const {") != std::string::npos );

    // Verify function body
    CHECK( code.find("return ((width * scale_x) * (height * scale_y));") != std::string::npos );
}

TEST_CASE("Multiple functions in single struct") {
    std::string source = R"(
        struct Flags {
            uint8 bits;

            function bool flag_a() {
                return (bits & 1) != 0;
            }

            function bool flag_b() {
                return (bits & 2) != 0;
            }

            function uint8 get_value() {
                return bits;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify all three functions are generated
    CHECK( code.find("bool flag_a() const {") != std::string::npos );
    CHECK( code.find("bool flag_b() const {") != std::string::npos );
    CHECK( code.find("uint8_t get_value() const {") != std::string::npos );

    // Verify function bodies
    CHECK( code.find("return ((bits & 1) != 0);") != std::string::npos );
    CHECK( code.find("return ((bits & 2) != 0);") != std::string::npos );
    CHECK( code.find("return bits;") != std::string::npos );
}

TEST_CASE("Function with complex expression") {
    std::string source = R"(
        struct Complex {
            uint32 a;
            uint32 b;

            function uint32 compute() {
                return (a + b) * 2 + (a - b) / 2;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function is generated
    CHECK( code.find("uint32_t compute() const {") != std::string::npos );

    // Verify complex expression in return statement
    CHECK( code.find("return (((a + b) * 2) + ((a - b) / 2));") != std::string::npos );
}

TEST_CASE("Function returning struct field") {
    std::string source = R"(
        struct Container {
            uint16 value;
            uint8 type;

            function uint16 get_value() {
                return value;
            }

            function uint8 get_type() {
                return type;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify both functions are generated
    CHECK( code.find("uint16_t get_value() const {") != std::string::npos );
    CHECK( code.find("uint8_t get_type() const {") != std::string::npos );

    // Verify return statements
    CHECK( code.find("return value;") != std::string::npos );
    CHECK( code.find("return type;") != std::string::npos );
}

TEST_CASE("Function with parameter and field access") {
    std::string source = R"(
        struct Data {
            uint32 base;

            function uint32 add_to_base(uint32 offset) {
                return base + offset;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function signature
    CHECK( code.find("uint32_t add_to_base(uint32_t offset) const {") != std::string::npos );

    // Verify return statement
    CHECK( code.find("return (base + offset);") != std::string::npos );
}

TEST_CASE("Functions placed after read method") {
    std::string source = R"(
        struct Simple {
            uint8 value;

            function uint8 doubled() {
                return value * 2;
            }
        };
    )";

    std::string code = generate_code(source);

    // Find positions of read method and user function
    size_t read_pos = code.find("static Simple read(");
    size_t func_pos = code.find("uint8_t doubled() const {");

    // User function should come after read method
    CHECK( read_pos != std::string::npos );
    CHECK( func_pos != std::string::npos );
    CHECK( func_pos > read_pos );
}

TEST_CASE("Function with 16-bit parameter") {
    std::string source = R"(
        struct Test {
            uint16 x;

            function bool less_than(uint16 limit) {
                return x < limit;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function with proper parameter type
    CHECK( code.find("bool less_than(uint16_t limit) const {") != std::string::npos );
    CHECK( code.find("return (x < limit);") != std::string::npos );
}

TEST_CASE("Function with 64-bit return type") {
    std::string source = R"(
        struct BigData {
            uint64 timestamp;

            function uint64 get_timestamp() {
                return timestamp;
            }
        };
    )";

    std::string code = generate_code(source);

    // Verify function with 64-bit return type
    CHECK( code.find("uint64_t get_timestamp() const {") != std::string::npos );
    CHECK( code.find("return timestamp;") != std::string::npos );
}

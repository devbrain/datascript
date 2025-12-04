//
// Unit Tests for CppWriterContext
//
// Tests the stack-based variant block management for C++ code generation.
//

#include <doctest/doctest.h>
#include <datascript/codegen/cpp/cpp_writer_context.hh>
#include <sstream>

using namespace datascript::codegen;

TEST_CASE("CppWriterContext: Basic struct generation") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_struct("Point");
    ctx << "int x;" << endl;
    ctx << "int y;" << endl;
    ctx.end_struct();

    std::string result = oss.str();
    CHECK(result.find("struct Point {") != std::string::npos);
    CHECK(result.find("    int x;") != std::string::npos);
    CHECK(result.find("    int y;") != std::string::npos);
    CHECK(result.find("};") != std::string::npos);
}

TEST_CASE("CppWriterContext: Struct with doc comment") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_struct("Point", "Represents a 2D point");
    ctx << "int x;" << endl;
    ctx.end_struct();

    std::string result = oss.str();
    CHECK(result.find("/**") != std::string::npos);
    CHECK(result.find(" * Represents a 2D point") != std::string::npos);
    CHECK(result.find(" */") != std::string::npos);
    CHECK(result.find("struct Point {") != std::string::npos);
}

TEST_CASE("CppWriterContext: Namespace generation") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_namespace("geometry");
    ctx << "struct Point {};" << endl;
    ctx.end_namespace();

    std::string result = oss.str();
    CHECK(result.find("namespace geometry {") != std::string::npos);
    CHECK(result.find("}  // namespace geometry") != std::string::npos);
}

TEST_CASE("CppWriterContext: Function generation") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_function("int", "add", "int a, int b");
    ctx << "return a + b;" << endl;
    ctx.end_function();

    std::string result = oss.str();
    CHECK(result.find("int add(int a, int b) {") != std::string::npos);
    CHECK(result.find("    return a + b;") != std::string::npos);
    CHECK(result.find("}") != std::string::npos);
}

TEST_CASE("CppWriterContext: Nested structures") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_namespace("math");
    ctx.start_struct("Vector");
    ctx << "double x, y, z;" << endl;
    ctx.end_struct();
    ctx.end_namespace();

    std::string result = oss.str();
    CHECK(result.find("namespace math {") != std::string::npos);
    CHECK(result.find("    struct Vector {") != std::string::npos);
    CHECK(result.find("        double x, y, z;") != std::string::npos);
    CHECK(result.find("    };") != std::string::npos);
    CHECK(result.find("}  // namespace math") != std::string::npos);
}

TEST_CASE("CppWriterContext: If/else control flow") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_if("x > 0");
    ctx << "return x;" << endl;
    ctx.start_else();
    ctx << "return -x;" << endl;
    ctx.end_if();

    std::string result = oss.str();
    CHECK(result.find("if (x > 0) {") != std::string::npos);
    CHECK(result.find("    return x;") != std::string::npos);
    CHECK(result.find("} else {") != std::string::npos);
    CHECK(result.find("    return -x;") != std::string::npos);
}

TEST_CASE("CppWriterContext: If/else-if/else chain") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_if("x > 0");
    ctx << "return 1;" << endl;
    ctx.start_else_if("x < 0");
    ctx << "return -1;" << endl;
    ctx.start_else();
    ctx << "return 0;" << endl;
    ctx.end_if();

    std::string result = oss.str();
    CHECK(result.find("if (x > 0) {") != std::string::npos);
    CHECK(result.find("} else if (x < 0) {") != std::string::npos);
    CHECK(result.find("} else {") != std::string::npos);
}

TEST_CASE("CppWriterContext: For loop") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_for("int i = 0", "i < n", "i++");
    ctx << "sum += arr[i];" << endl;
    ctx.end_for();

    std::string result = oss.str();
    CHECK(result.find("for (int i = 0; i < n; i++) {") != std::string::npos);
    CHECK(result.find("    sum += arr[i];") != std::string::npos);
}

TEST_CASE("CppWriterContext: While loop") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_while("!done");
    ctx << "done = process();" << endl;
    ctx.end_while();

    std::string result = oss.str();
    CHECK(result.find("while (!done) {") != std::string::npos);
    CHECK(result.find("    done = process();") != std::string::npos);
}

TEST_CASE("CppWriterContext: Scope block") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_scope();
    ctx << "int temp = x;" << endl;
    ctx << "x = y;" << endl;
    ctx << "y = temp;" << endl;
    ctx.end_scope();

    std::string result = oss.str();
    CHECK(result.find("{") != std::string::npos);
    CHECK(result.find("    int temp = x;") != std::string::npos);
    CHECK(result.find("}") != std::string::npos);
}

TEST_CASE("CppWriterContext: Try/catch block") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_try();
    ctx << "risky_operation();" << endl;
    ctx.start_catch("const std::exception&", "e");
    ctx << "handle_error(e);" << endl;
    ctx.end_try();

    std::string result = oss.str();
    CHECK(result.find("try {") != std::string::npos);
    CHECK(result.find("    risky_operation();") != std::string::npos);
    CHECK(result.find("} catch (const std::exception& e) {") != std::string::npos);
    CHECK(result.find("    handle_error(e);") != std::string::npos);
}

TEST_CASE("CppWriterContext: Helper methods") {
    SUBCASE("Pragma once") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx.write_pragma_once();
        CHECK(oss.str() == "#pragma once\n");
    }

    SUBCASE("System include") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx.write_include("vector", true);
        CHECK(oss.str() == "#include <vector>\n");
    }

    SUBCASE("User include") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx.write_include("myheader.hh", false);
        CHECK(oss.str() == "#include \"myheader.hh\"\n");
    }

    SUBCASE("Comment") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx.write_comment("This is a comment");
        CHECK(oss.str() == "// This is a comment\n");
    }

    SUBCASE("Blank line") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx.write_blank_line();
        CHECK(oss.str() == "\n");
    }
}

TEST_CASE("CppWriterContext: Streaming operators") {
    SUBCASE("String streaming") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx << "Hello" << " " << "World" << endl;
        CHECK(oss.str() == "Hello World\n");
    }

    SUBCASE("Integer streaming") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx << "Value: " << 42 << endl;
        CHECK(oss.str() == "Value: 42\n");
    }

    SUBCASE("Mixed streaming") {
        std::ostringstream oss;
        CppWriterContext ctx(oss);
        ctx << "Count: " << 10 << ", Size: " << 20 << endl;
        CHECK(oss.str() == "Count: 10, Size: 20\n");
    }
}

TEST_CASE("CppWriterContext: Complete header generation") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    // Generate a complete C++ header
    ctx.write_pragma_once();
    ctx << blank;

    ctx.write_include("string", true);
    ctx.write_include("vector", true);
    ctx << blank;

    ctx.start_namespace("geometry");
    ctx << blank;

    ctx.start_struct("Point", "Represents a 2D point");
    ctx << "int x;" << endl;
    ctx << "int y;" << endl;
    ctx << blank;

    ctx.start_function("double", "distance", "const Point& other");
    ctx << "double dx = x - other.x;" << endl;
    ctx << "double dy = y - other.y;" << endl;
    ctx << "return std::sqrt(dx * dx + dy * dy);" << endl;
    ctx.end_function();

    ctx.end_struct();
    ctx.end_namespace();

    std::string result = oss.str();

    // Verify key components
    CHECK(result.find("#pragma once") != std::string::npos);
    CHECK(result.find("#include <string>") != std::string::npos);
    CHECK(result.find("#include <vector>") != std::string::npos);
    CHECK(result.find("namespace geometry {") != std::string::npos);
    CHECK(result.find("struct Point {") != std::string::npos);
    CHECK(result.find("Represents a 2D point") != std::string::npos);
    CHECK(result.find("int x;") != std::string::npos);
    CHECK(result.find("int y;") != std::string::npos);
    CHECK(result.find("double distance(const Point& other) {") != std::string::npos);
    CHECK(result.find("return std::sqrt(dx * dx + dy * dy);") != std::string::npos);
    CHECK(result.find("};") != std::string::npos);
    CHECK(result.find("}  // namespace geometry") != std::string::npos);
}

TEST_CASE("CppWriterContext: Basic enum generation") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_enum("Color", "uint8_t");
    ctx << "Red = 0xFF0000," << endl;
    ctx << "Green = 0x00FF00," << endl;
    ctx << "Blue = 0x0000FF" << endl;
    ctx.end_enum();

    std::string result = oss.str();
    CHECK(result.find("enum class Color : uint8_t {") != std::string::npos);
    CHECK(result.find("    Red = 0xFF0000,") != std::string::npos);
    CHECK(result.find("    Green = 0x00FF00,") != std::string::npos);
    CHECK(result.find("    Blue = 0x0000FF") != std::string::npos);
    CHECK(result.find("};") != std::string::npos);
}

TEST_CASE("CppWriterContext: Enum with doc comment") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_enum("Status", "uint32_t", "Operation status codes");
    ctx << "Success = 0," << endl;
    ctx << "Error = 1" << endl;
    ctx.end_enum();

    std::string result = oss.str();
    CHECK(result.find("/**") != std::string::npos);
    CHECK(result.find(" * Operation status codes") != std::string::npos);
    CHECK(result.find(" */") != std::string::npos);
    CHECK(result.find("enum class Status : uint32_t {") != std::string::npos);
    CHECK(result.find("Success = 0,") != std::string::npos);
}

TEST_CASE("CppWriterContext: Enum with different base types") {
    std::ostringstream oss;
    CppWriterContext ctx(oss);

    ctx.start_enum("Flags", "uint64_t");
    ctx << "Flag1 = 0x01," << endl;
    ctx << "Flag2 = 0x02" << endl;
    ctx.end_enum();

    std::string result = oss.str();
    CHECK(result.find("enum class Flags : uint64_t {") != std::string::npos);
}

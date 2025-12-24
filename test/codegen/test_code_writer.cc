//
// Unit tests for CodeWriter and RAII block classes
//

#include <doctest/doctest.h>
#include <datascript/codegen/code_writer.hh>
#include <datascript/codegen/cpp/cpp_code_writer.hh>
#include <sstream>
#include <string>

using namespace datascript::codegen;

// ============================================================================
// Helper Functions
// ============================================================================

std::string trim_trailing_whitespace(const std::string& str) {
    std::string result;
    std::istringstream iss(str);
    std::string line;
    while (std::getline(iss, line)) {
        // Remove trailing whitespace from each line
        size_t end = line.find_last_not_of(" \t\r\n");
        if (end != std::string::npos) {
            result += line.substr(0, end + 1);
        }
        result += '\n';
    }
    return result;
}

// ============================================================================
// CodeWriter Basic Tests
// ============================================================================

TEST_CASE("CodeWriter: Basic output") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Write single line") {
        writer.write_line("hello");
        CHECK(oss.str() == "hello\n");
    }

    SUBCASE("Write multiple lines") {
        writer.write_line("line1");
        writer.write_line("line2");
        CHECK(oss.str() == "line1\nline2\n");
    }

    SUBCASE("Write raw text") {
        writer.write_raw("raw");
        CHECK(oss.str() == "raw");
    }

    SUBCASE("Write blank line") {
        writer.write_blank_line();
        CHECK(oss.str() == "\n");
    }
}

TEST_CASE("CodeWriter: Indentation") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Indent increases level") {
        writer.indent();
        writer.write_line("indented");
        CHECK(oss.str() == "    indented\n");
    }

    SUBCASE("Multiple indent levels") {
        writer.indent();
        writer.indent();
        writer.write_line("double indented");
        CHECK(oss.str() == "        double indented\n");
    }

    SUBCASE("Unindent decreases level") {
        writer.indent();
        writer.indent();
        writer.unindent();
        writer.write_line("single indent");
        CHECK(oss.str() == "    single indent\n");
    }

    SUBCASE("Unindent at level 0 is safe") {
        writer.unindent();  // Should not crash
        writer.write_line("no indent");
        CHECK(oss.str() == "no indent\n");
    }

    SUBCASE("Custom indent string") {
        writer.set_indent_string("\t");
        writer.indent();
        writer.write_line("tab indented");
        CHECK(oss.str() == "\ttab indented\n");
    }
}

// ============================================================================
// IfBlock Tests
// ============================================================================

TEST_CASE("IfBlock: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Simple if block") {
        {
            auto if_block = writer.write_if("x > 0");
            writer.write_line("return x;");
        }

        std::string expected = "if (x > 0) {\n"
                              "    return x;\n"
                              "}\n";
        CHECK(oss.str() == expected);
    }

    SUBCASE("If-else chain") {
        {
            auto if_block = writer.write_if("x > 0");
            writer.write_line("return x;");

            auto else_block = if_block.write_else();
            writer.write_line("return 0;");
        }

        std::string expected = "if (x > 0) {\n"
                              "    return x;\n"
                              "} else {\n"
                              "    return 0;\n"
                              "}\n";
        CHECK(oss.str() == expected);
    }

    SUBCASE("If-else-if chain") {
        {
            auto if_block = writer.write_if("x > 0");
            writer.write_line("return 1;");

            auto else_if_block = if_block.write_else_if("x < 0");
            writer.write_line("return -1;");

            auto else_block = else_if_block.write_else();
            writer.write_line("return 0;");
        }

        std::string expected = "if (x > 0) {\n"
                              "    return 1;\n"
                              "} else if (x < 0) {\n"
                              "    return -1;\n"
                              "} else {\n"
                              "    return 0;\n"
                              "}\n";
        CHECK(oss.str() == expected);
    }
}

// ============================================================================
// ForBlock Tests
// ============================================================================

TEST_CASE("ForBlock: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    {
        auto for_block = writer.write_for("int i = 0", "i < 10", "i++");
        writer.write_line("process(i);");
    }

    std::string expected = "for (int i = 0; i < 10; i++) {\n"
                          "    process(i);\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

// ============================================================================
// WhileBlock Tests
// ============================================================================

TEST_CASE("WhileBlock: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    {
        auto while_block = writer.write_while("count > 0");
        writer.write_line("count--;");
    }

    std::string expected = "while (count > 0) {\n"
                          "    count--;\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

// ============================================================================
// TryBlock Tests
// ============================================================================

TEST_CASE("TryBlock: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Try-catch block") {
        {
            auto try_block = writer.write_try();
            writer.write_line("risky_operation();");

            auto catch_block = try_block.write_catch("const std::exception&", "e");
            writer.write_line("handle_error(e);");
        }

        std::string expected = "try {\n"
                              "    risky_operation();\n"
                              "} catch (const std::exception& e) {\n"
                              "    handle_error(e);\n"
                              "}\n";
        CHECK(oss.str() == expected);
    }

    SUBCASE("Try with multiple catch blocks") {
        {
            auto try_block = writer.write_try();
            writer.write_line("risky_operation();");

            auto catch1 = try_block.write_catch("const std::runtime_error&", "e");
            writer.write_line("handle_runtime_error(e);");

            auto catch2 = catch1.write_catch("const std::exception&", "e");
            writer.write_line("handle_generic_error(e);");
        }

        std::string expected = "try {\n"
                              "    risky_operation();\n"
                              "} catch (const std::runtime_error& e) {\n"
                              "    handle_runtime_error(e);\n"
                              "} catch (const std::exception& e) {\n"
                              "    handle_generic_error(e);\n"
                              "}\n";
        CHECK(oss.str() == expected);
    }
}

// ============================================================================
// ScopeBlock Tests
// ============================================================================

TEST_CASE("ScopeBlock: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    {
        auto scope = writer.write_scope();
        writer.write_line("int x = 42;");
        writer.write_line("process(x);");
    }

    std::string expected = "{\n"
                          "    int x = 42;\n"
                          "    process(x);\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

// ============================================================================
// Nested Blocks Tests
// ============================================================================

TEST_CASE("Nested blocks") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    {
        auto if_block = writer.write_if("condition1");
        writer.write_line("outer();");

        {
            auto nested_if = writer.write_if("condition2");
            writer.write_line("inner();");
        }

        writer.write_line("after_nested();");
    }

    std::string expected = "if (condition1) {\n"
                          "    outer();\n"
                          "    if (condition2) {\n"
                          "        inner();\n"
                          "    }\n"
                          "    after_nested();\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

// ============================================================================
// CppCodeWriter Tests
// ============================================================================

TEST_CASE("CppCodeWriter: Pragma and includes") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    SUBCASE("Pragma once") {
        writer.write_pragma_once();
        CHECK(oss.str() == "#pragma once\n");
    }

    SUBCASE("System include") {
        writer.write_include("iostream", true);
        CHECK(oss.str() == "#include <iostream>\n");
    }

    SUBCASE("Local include") {
        writer.write_include("my_header.hh", false);
        CHECK(oss.str() == "#include \"my_header.hh\"\n");
    }

    SUBCASE("Multiple includes") {
        writer.write_includes({"iostream", "vector", "string"}, true);
        CHECK(oss.str() == "#include <iostream>\n#include <vector>\n#include <string>\n");
    }
}

TEST_CASE("CppCodeWriter: Struct block") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    {
        auto struct_block = writer.write_struct("MyStruct");
        writer.write_line("int field1;");
        writer.write_line("std::string field2;");
    }

    std::string expected = "struct MyStruct {\n"
                          "    int field1;\n"
                          "    std::string field2;\n"
                          "};\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("CppCodeWriter: Struct with access modifiers") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    {
        auto struct_block = writer.write_struct("MyStruct");
        struct_block.write_public();
        writer.write_line("int public_field;");
        struct_block.write_private();
        writer.write_line("int private_field_;");
    }

    std::string expected = "struct MyStruct {\n"
                          "public:\n"
                          "    int public_field;\n"
                          "private:\n"
                          "    int private_field_;\n"
                          "};\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("CppCodeWriter: Function block") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    {
        auto func = writer.write_function("int", "add", "int a, int b");
        writer.write_line("return a + b;");
    }

    std::string expected = "int add(int a, int b) {\n"
                          "    return a + b;\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("CppCodeWriter: Namespace block") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    {
        auto ns = writer.write_namespace("myapp");
        writer.write_line("void function() {}");
    }

    std::string expected = "namespace myapp {\n"
                          "\n"
                          "    void function() {}\n"
                          "\n"
                          "}  // namespace myapp\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("CppCodeWriter: Comments") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    SUBCASE("Single line comment") {
        writer.write_comment("This is a comment");
        CHECK(oss.str() == "// This is a comment\n");
    }

    SUBCASE("Block comment") {
        writer.write_block_comment({"Line 1", "Line 2", "Line 3"});
        std::string expected = "/*\n"
                              " * Line 1\n"
                              " * Line 2\n"
                              " * Line 3\n"
                              " */\n";
        CHECK(oss.str() == expected);
    }
}

TEST_CASE("CppCodeWriter: Using and typedef") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    SUBCASE("Using alias") {
        writer.write_using("MyInt", "int32_t");
        CHECK(oss.str() == "using MyInt = int32_t;\n");
    }

    SUBCASE("Typedef") {
        writer.write_typedef("struct MyStruct", "MyStruct_t");
        CHECK(oss.str() == "typedef struct MyStruct MyStruct_t;\n");
    }
}

// ============================================================================
// Streaming Operator Tests
// ============================================================================

TEST_CASE("Streaming operators: Basic usage") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Simple stream with endl") {
        writer << "hello" << endl;
        CHECK(oss.str() == "hello\n");
    }

    SUBCASE("Chain multiple strings") {
        writer << "Hello" << " " << "World" << endl;
        CHECK(oss.str() == "Hello World\n");
    }

    SUBCASE("Stream with numbers") {
        writer << "Count: " << 42 << endl;
        CHECK(oss.str() == "Count: 42\n");
    }

    SUBCASE("Stream with size_t") {
        size_t size = 100;
        writer << "Size: " << size << endl;
        CHECK(oss.str() == "Size: 100\n");
    }

    SUBCASE("Multiple lines with endl") {
        writer << "Line 1" << endl;
        writer << "Line 2" << endl;
        CHECK(oss.str() == "Line 1\nLine 2\n");
    }
}

TEST_CASE("Streaming operators: Indentation manipulators") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    SUBCASE("Indent manipulator") {
        writer << "Before" << endl;
        writer << indent;
        writer << "Indented" << endl;
        CHECK(oss.str() == "Before\n    Indented\n");
    }

    SUBCASE("Unindent manipulator") {
        writer << indent;
        writer << "Indented" << endl;
        writer << unindent;
        writer << "Not indented" << endl;
        CHECK(oss.str() == "    Indented\nNot indented\n");
    }

    SUBCASE("Nested indentation") {
        writer << "Level 0" << endl;
        writer << indent;
        writer << "Level 1" << endl;
        writer << indent;
        writer << "Level 2" << endl;
        writer << unindent;
        writer << "Level 1" << endl;
        writer << unindent;
        writer << "Level 0" << endl;

        std::string expected = "Level 0\n"
                              "    Level 1\n"
                              "        Level 2\n"
                              "    Level 1\n"
                              "Level 0\n";
        CHECK(oss.str() == expected);
    }
}

TEST_CASE("Streaming operators: Blank line manipulator") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    writer << "Line 1" << endl;
    writer << blank;
    writer << "Line 2" << endl;

    std::string expected = "Line 1\n\nLine 2\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("Streaming operators: Mixed with write_line") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    writer << "Stream 1" << endl;
    writer.write_line("Write 2");
    writer << "Stream 3" << endl;

    std::string expected = "Stream 1\nWrite 2\nStream 3\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("Streaming operators: If-block with streams") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    {
        auto if_block = writer.write_if("x > 0");
        writer << "return x;" << endl;
    }

    std::string expected = "if (x > 0) {\n"
                          "    return x;\n"
                          "}\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("Streaming operators: Complete example with streams") {
    std::ostringstream oss;
    CodeWriter writer(oss);

    writer << "struct Point {" << endl;
    writer << indent;
    writer << "int x;" << endl;
    writer << "int y;" << endl;
    writer << blank;
    writer << "Point(int x_, int y_) : x(x_), y(y_) {}" << endl;
    writer << unindent;
    writer << "};" << endl;

    std::string expected = "struct Point {\n"
                          "    int x;\n"
                          "    int y;\n"
                          "\n"
                          "    Point(int x_, int y_) : x(x_), y(y_) {}\n"
                          "};\n";
    CHECK(oss.str() == expected);
}

TEST_CASE("Streaming operators: RAII blocks with streaming") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    {
        auto struct_block = writer.write_struct("Message");
        writer << "uint32_t id;" << endl;
        writer << "std::string data;" << endl;
        writer << blank;

        {
            auto func = writer.write_function("void", "print", "");
            writer << "std::cout << \"ID: \" << id << std::endl;" << endl;
        }
    }

    std::string expected = "struct Message {\n"
                          "    uint32_t id;\n"
                          "    std::string data;\n"
                          "\n"
                          "    void print() {\n"
                          "        std::cout << \"ID: \" << id << std::endl;\n"
                          "    }\n"
                          "};\n";
    CHECK(oss.str() == expected);
}

// ============================================================================
// Real-World Example Tests
// ============================================================================

TEST_CASE("Real-world example: Generate struct with read method") {
    std::ostringstream oss;
    CppCodeWriter writer(oss);

    writer.write_pragma_once();
    writer.write_blank_line();
    writer.write_include("cstdint", true);
    writer.write_blank_line();

    {
        auto struct_block = writer.write_struct("Message");
        writer.write_line("uint32_t id;");
        writer.write_line("uint16_t length;");
        writer.write_blank_line();

        {
            auto func = writer.write_function("static Message", "read", "const uint8_t*& data, const uint8_t* end");
            writer.write_line("Message msg;");

            {
                auto if_block = writer.write_if("data + 6 > end");
                writer.write_line("throw std::runtime_error(\"Buffer underflow\");");
            }

            writer.write_line("msg.id = read_uint32(data);");
            writer.write_line("msg.length = read_uint16(data);");
            writer.write_line("return msg;");
        }
    }

    std::string expected = "#pragma once\n"
                          "\n"
                          "#include <cstdint>\n"
                          "\n"
                          "struct Message {\n"
                          "    uint32_t id;\n"
                          "    uint16_t length;\n"
                          "\n"
                          "    static Message read(const uint8_t*& data, const uint8_t* end) {\n"
                          "        Message msg;\n"
                          "        if (data + 6 > end) {\n"
                          "            throw std::runtime_error(\"Buffer underflow\");\n"
                          "        }\n"
                          "        msg.id = read_uint32(data);\n"
                          "        msg.length = read_uint16(data);\n"
                          "        return msg;\n"
                          "    }\n"
                          "};\n";
    CHECK(oss.str() == expected);
}

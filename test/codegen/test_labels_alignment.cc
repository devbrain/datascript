//
// Tests for Labels and Alignment Code Generation
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;
using namespace datascript::semantic;
using namespace datascript::ir;
using namespace datascript::codegen;

namespace {
    module_set make_module_set(const std::string& source) {
        auto main_mod = parse_datascript(source);

        module_set modules;
        modules.main.module = std::move(main_mod);
        modules.main.file_path = "<test>";
        modules.main.package_name = "";

        return modules;
    }
}

TEST_SUITE("Codegen - Labels and Alignment") {
    TEST_CASE("Generate code with label directive") {
        auto modules = make_module_set(R"(
            struct FileHeader {
                uint32 magic;
                uint32 data_offset;
                data_offset:  // Label: seek to data_offset position
                uint8 data;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::exceptions_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify label seeking code is generated
        CHECK(cpp_code.find("Seek to labeled position") != std::string::npos);
        CHECK(cpp_code.find("label_pos") != std::string::npos);
        CHECK(cpp_code.find(" = start +") != std::string::npos);
        CHECK(cpp_code.find("Label position out of bounds") != std::string::npos);
    }

    TEST_CASE("Generate code with alignment directive") {
        auto modules = make_module_set(R"(
            struct AlignedData {
                uint8 header;
                align(4):  // Align to 4-byte boundary
                uint32 aligned_field;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::exceptions_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify alignment padding code is generated with relative offset (not absolute memory address)
        CHECK(cpp_code.find("Align to 4-byte boundary") != std::string::npos);
        CHECK(cpp_code.find("size_t offset = data - start") != std::string::npos);
        CHECK(cpp_code.find("size_t aligned_offset") != std::string::npos);
    }

    TEST_CASE("Generate code with both label and alignment") {
        auto modules = make_module_set(R"(
            struct Complex {
                uint32 offset;
                offset:        // Seek to offset
                align(8):      // Then align to 8-byte boundary
                uint64 value;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::exceptions_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify both label and alignment code are generated
        CHECK(cpp_code.find("Seek to labeled position") != std::string::npos);
        CHECK(cpp_code.find("Align to 8-byte boundary") != std::string::npos);

        // Verify the order: label seeking should come before alignment
        size_t label_pos = cpp_code.find("Seek to labeled position");
        size_t align_pos = cpp_code.find("Align to 8-byte boundary");
        CHECK(label_pos < align_pos);
    }

    TEST_CASE("Generate safe-mode code with label") {
        auto modules = make_module_set(R"(
            struct SafeHeader {
                uint32 offset;
                offset:
                uint8 data;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code in safe mode
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::results_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify safe mode error handling
        CHECK(cpp_code.find("result.error_message = \"Label position out of bounds\"") != std::string::npos);
        CHECK(cpp_code.find("return result") != std::string::npos);
    }

    TEST_CASE("Generate code with field access in label") {
        auto modules = make_module_set(R"(
            struct Header {
                uint32 data_offset;
            };

            struct FileWithHeader {
                Header header;
                header.data_offset:  // Field access label
                uint8[128] data;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::exceptions_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify label seeking with field access is generated
        CHECK(cpp_code.find("Seek to labeled position") != std::string::npos);
        CHECK(cpp_code.find("label_pos") != std::string::npos);
        CHECK(cpp_code.find("start + obj.header.data_offset") != std::string::npos);
        CHECK(cpp_code.find("Label position out of bounds") != std::string::npos);
    }

    TEST_CASE("Generate code with nested field access in label") {
        auto modules = make_module_set(R"(
            struct Metadata {
                uint32 offset;
            };

            struct Header {
                Metadata meta;
            };

            struct ComplexFile {
                Header header;
                header.meta.offset:  // Nested field access
                uint8 data;
            };
        )");

        auto result = analyze(modules);
        REQUIRE_FALSE(result.has_errors());
        REQUIRE(result.analyzed.has_value());

        // Build IR
        auto ir = build_ir(result.analyzed.value());

        // Generate C++ code
        cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = cpp_options::exceptions_only;
        std::string cpp_code = generate_cpp_header(ir, opts);

        // Verify nested field access is generated correctly
        CHECK(cpp_code.find("Seek to labeled position") != std::string::npos);
        CHECK(cpp_code.find("obj.header.meta.offset") != std::string::npos);
    }
}

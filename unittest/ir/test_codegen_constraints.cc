//
// Code generation tests for constraint validation
//

#include <doctest/doctest.h>
#include <datascript/parser.hh>
#include <datascript/semantic.hh>
#include <datascript/ir_builder.hh>
#include <datascript/codegen.hh>

using namespace datascript;

TEST_SUITE("IR/Codegen - Constraints") {
    TEST_CASE("Parameter constraint validation") {
        const char* schema = R"(
struct Header {
    uint32 magic;
    uint8 version;
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

        // Manually add constraint to demonstrate validation
        ir::constraint_def constraint;
        constraint.name = "ValidMagic";
        constraint.source = {"test.ds", 1, 1};

        ir::constraint_def::parameter param;
        param.name = "value";
        param.type.kind = ir::type_kind::uint32;
        param.source = {"test.ds", 1, 1};
        constraint.params.push_back(std::move(param));

        // Build condition: value == 0xDEADBEEF
        auto left = std::make_unique<ir::expr>(
            ir::expr::make_param_ref("value", {"test.ds", 1, 1})
        );
        auto right = std::make_unique<ir::expr>(
            ir::expr::make_literal_int(0xDEADBEEF, {"test.ds", 1, 1})
        );
        constraint.condition = ir::expr::make_binary_op(
            ir::expr::eq,
            std::move(left),
            std::move(right),
            {"test.ds", 1, 1}
        );
        constraint.error_message_template = "Magic must be 0xDEADBEEF";

        ir.constraints.push_back(std::move(constraint));

        // Apply to magic field
        ir::field::constraint_application app;
        app.constraint_index = 0;
        app.source = {"test.ds", 1, 1};
        ir.structs[0].fields[0].constraints.push_back(std::move(app));

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::both;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify constraint validation code
        CHECK(cpp_code.find("// Validate constraint: ValidMagic") != std::string::npos);
        CHECK(cpp_code.find("value == 3735928559") != std::string::npos);  // 0xDEADBEEF

        // Verify error handling (safe version)
        CHECK(cpp_code.find("result.error_message = \"Constraint 'ValidMagic' violated") != std::string::npos);

        // Verify exception (throwing version)
        CHECK(cpp_code.find("throw ConstraintViolation(") != std::string::npos);
    }

    TEST_CASE("Field reference constraints") {
        const char* schema = R"(
struct Header {
    uint32 magic;
    uint8 version;
    uint16 flags;
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

        // Add constraint that references fields
        ir::constraint_def constraint;
        constraint.name = "ValidHeader";
        constraint.source = {"test.ds", 1, 1};

        // Condition: magic != version
        auto left = std::make_unique<ir::expr>(
            ir::expr::make_param_ref("magic", {"test.ds", 1, 1})  // Will be treated as field_ref in context
        );
        auto right = std::make_unique<ir::expr>(
            ir::expr::make_param_ref("version", {"test.ds", 1, 1})
        );
        constraint.condition = ir::expr::make_binary_op(
            ir::expr::ne,
            std::move(left),
            std::move(right),
            {"test.ds", 1, 1}
        );
        constraint.error_message_template = "Magic and version must be different";

        ir.constraints.push_back(std::move(constraint));

        // Apply to flags field
        ir::field::constraint_application app;
        app.constraint_index = 0;
        app.source = {"test.ds", 1, 1};
        ir.structs[0].fields[2].constraints.push_back(std::move(app));

        codegen::cpp_options opts;
        opts.namespace_name = "test";
        opts.error_handling = codegen::cpp_options::both;

        std::string cpp_code = codegen::generate_cpp_header(ir, opts);

        // Verify field references are prefixed with obj.
        CHECK(cpp_code.find("obj.magic") != std::string::npos);
        CHECK(cpp_code.find("obj.version") != std::string::npos);

        // Verify context-aware generation
        size_t constraint_pos = cpp_code.find("// Validate constraint: ValidHeader");
        REQUIRE(constraint_pos != std::string::npos);

        size_t next_field_pos = cpp_code.find("result.value = obj;", constraint_pos);
        std::string constraint_code = cpp_code.substr(constraint_pos, next_field_pos - constraint_pos);

        // Should have obj. prefix
        CHECK(constraint_code.find("obj.magic != obj.version") != std::string::npos);
    }
}

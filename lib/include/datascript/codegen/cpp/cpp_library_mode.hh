#pragma once

#include <datascript/ir.hh>
#include <datascript/base_renderer.hh>
#include <filesystem>
#include <string>
#include <vector>

namespace datascript::codegen {

// Forward declarations
class CppRenderer;
class CppWriterContext;

/**
 * Helper class for generating C++ code in library mode (three headers).
 *
 * Library mode generates three header files:
 * 1. <name>_runtime.h - Format-specific exceptions and read helpers
 * 2. <name>.h - Public API (enums, constants, forward declarations)
 * 3. <name>_impl.h - Full struct definitions with methods
 *
 * This class encapsulates all library mode generation logic to keep
 * CppRenderer manageable.
 */
class CppLibraryModeGenerator {
public:
    /**
     * Constructor.
     * @param renderer Parent renderer for accessing configuration
     */
    explicit CppLibraryModeGenerator(CppRenderer& renderer);

    /**
     * Structure holding the three library mode output filenames.
     */
    struct LibraryFiles {
        std::string public_header;   // "pe_format.h"
        std::string impl_header;     // "pe_format_impl.h"
        std::string runtime_header;  // "pe_format_runtime.h"
    };

    /**
     * Generate all three header files for library mode.
     *
     * @param bundle IR bundle to generate code for
     * @param output_dir Output directory for generated files
     * @return Vector of OutputFile (runtime, public, impl)
     */
    std::vector<OutputFile> generate(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir);

private:
    CppRenderer& renderer_;

    /**
     * Extract namespace from bundle name.
     * Example: "pe.format" -> "pe::format"
     */
    std::string extract_namespace(const ir::bundle& bundle) const;

    /**
     * Determine output filenames for three headers.
     */
    LibraryFiles get_filenames(
        const ir::bundle& bundle,
        const std::filesystem::path& output_dir) const;

    /**
     * Generate runtime header using CppHelperGenerator.
     */
    std::string generate_runtime_header(
        const std::string& namespace_name,
        const LibraryFiles& files) const;

    /**
     * Generate public header (enums, constants, forward declarations).
     */
    std::string generate_public_header(
        const ir::bundle& bundle,
        const std::string& namespace_name,
        const LibraryFiles& files) const;

    /**
     * Generate implementation header (full struct definitions).
     */
    std::string generate_impl_header(
        const ir::bundle& bundle,
        const std::string& namespace_name,
        const LibraryFiles& files);

    /**
     * Generate introspection metadata for a struct.
     */
    void generate_struct_metadata(
        std::ostream& out,
        const ir::struct_def& struct_def,
        const std::string& namespace_name) const;
};

}  // namespace datascript::codegen

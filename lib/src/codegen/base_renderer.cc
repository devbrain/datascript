#include <datascript/base_renderer.hh>

namespace datascript::codegen {

std::vector<OutputFile> BaseRenderer::generate_files(
    const ir::bundle& bundle,
    const std::filesystem::path& output_dir)
{
    // Default implementation: single file using legacy render_module() method
    // Derive filename from first struct name or use "generated"
    std::string filename;
    if (!bundle.structs.empty()) {
        filename = bundle.structs[0].name;
    } else if (!bundle.choices.empty()) {
        filename = bundle.choices[0].name;
    } else if (!bundle.enums.empty()) {
        filename = bundle.enums[0].name;
    } else {
        filename = "generated";
    }

    // Add appropriate extension from renderer
    filename += get_file_extension();

    std::filesystem::path output_path = output_dir / filename;

    // Generate code using legacy render_module() method with default options
    RenderOptions default_options;
    std::string content = render_module(bundle, default_options);

    return {{output_path, content}};
}

} // namespace datascript::codegen

//
// C++ Renderer Plugin
//
// Automatically registers CppRenderer with the RendererRegistry during
// static initialization, eliminating the circular dependency between
// RendererRegistry and CppRenderer.
//

#include <datascript/renderer_plugin.hh>
#include <datascript/renderer_registry.hh>
#include <datascript/codegen/cpp/cpp_renderer.hh>

namespace datascript::codegen {

// ============================================================================
// C++ Renderer Plugin Implementation
// ============================================================================

/**
 * Plugin that registers the C++ code renderer.
 *
 * This plugin is automatically instantiated and registered during static
 * initialization via the REGISTER_RENDERER_PLUGIN macro below.
 */
class CppRendererPlugin : public RendererPlugin {
public:
    void register_renderer(RendererRegistry& registry) override {
        // Create and register the C++ renderer
        registry.register_renderer("cpp", std::make_unique<CppRenderer>());
    }

    [[nodiscard]] std::string get_name() const override {
        return "cpp";
    }

    [[nodiscard]] std::string get_version() const override {
        return "1.0.0";
    }
};

// ============================================================================
// Automatic Registration
// ============================================================================

// This macro creates a static instance of CppRendererPlugin that registers
// itself with the RendererRegistry during program initialization (before main()).
//
// The registration happens automatically when this translation unit is linked
// into the final executable, without requiring any explicit initialization code.
REGISTER_RENDERER_PLUGIN(CppRendererPlugin);

// ============================================================================
// Explicit Initialization Function
// ============================================================================

/**
 * Ensures CppRenderer is registered.
 *
 * This function is called by RendererRegistry::instance() to guarantee
 * that this translation unit is linked and its static initializers have run.
 * The function body is intentionally empty - its mere existence forces the
 * linker to include this translation unit, which triggers the static
 * initialization of g_CppRendererPlugin_registrar above.
 */
void ensure_cpp_renderer_registered() {
    // Empty - existence of this function forces linkage of this translation unit
}

} // namespace datascript::codegen

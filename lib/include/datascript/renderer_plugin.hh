//
// Renderer Plugin Interface
//
// Enables dynamic registration of language-specific renderers without
// modifying the RendererRegistry. Resolves circular dependency between
// RendererRegistry and concrete renderer implementations.
//

#pragma once

#include <memory>
#include <string>

namespace datascript::codegen {

// Forward declarations
class RendererRegistry;
class BaseRenderer;

// ============================================================================
// Renderer Plugin Interface
// ============================================================================

/**
 * Abstract interface for renderer plugins.
 *
 * Renderer plugins enable automatic registration of language-specific
 * renderers without creating compile-time dependencies in the registry.
 *
 * Example usage:
 *
 * ```cpp
 * class CppRendererPlugin : public RendererPlugin {
 * public:
 *     void register_renderer(RendererRegistry& registry) override {
 *         registry.register_renderer("cpp", std::make_unique<CppRenderer>());
 *     }
 *
 *     std::string get_name() const override { return "cpp"; }
 *     std::string get_version() const override { return "1.0.0"; }
 * };
 *
 * // Automatic registration via static initialization
 * REGISTER_RENDERER_PLUGIN(CppRendererPlugin);
 * ```
 *
 * The plugin is automatically registered when the containing translation
 * unit is linked into the final executable.
 */
class RendererPlugin {
public:
    virtual ~RendererPlugin() = default;

    /**
     * Called during plugin registration to register the renderer.
     *
     * @param registry The renderer registry to register with
     */
    virtual void register_renderer(RendererRegistry& registry) = 0;

    /**
     * Get the language name this plugin provides.
     *
     * @return Language name (e.g., "cpp", "rust", "python")
     */
    virtual std::string get_name() const = 0;

    /**
     * Get the plugin version.
     *
     * @return Version string (e.g., "1.0.0")
     */
    virtual std::string get_version() const = 0;
};

// ============================================================================
// Plugin Registration Helper
// ============================================================================

/**
 * Helper macro for automatic plugin registration via static initialization.
 *
 * Usage:
 *   REGISTER_RENDERER_PLUGIN(MyRendererPlugin);
 *
 * This creates a static instance of the plugin that registers itself
 * during program initialization (before main() is called).
 *
 * How it works:
 * 1. Creates a unique static struct with constructor
 * 2. Constructor instantiates plugin and registers with RendererRegistry
 * 3. Static initialization guarantees registration before main()
 * 4. Works across translation units in same executable
 */
#define REGISTER_RENDERER_PLUGIN(PluginClass)                                  \
    namespace {                                                                 \
        struct PluginClass##_Registrar {                                        \
            PluginClass##_Registrar();                                          \
        };                                                                      \
        static PluginClass##_Registrar g_##PluginClass##_registrar;            \
        PluginClass##_Registrar::PluginClass##_Registrar() {                   \
            RendererRegistry::instance().register_plugin(                       \
                std::make_unique<PluginClass>()                                 \
            );                                                                  \
        }                                                                       \
    }

} // namespace datascript::codegen

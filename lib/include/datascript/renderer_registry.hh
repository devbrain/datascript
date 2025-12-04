#pragma once

#include <string>
#include <memory>
#include <map>
#include <vector>
#include <optional>
#include <datascript/base_renderer.hh>

namespace datascript::codegen {

// Forward declaration
class RendererPlugin;

// ============================================================================
// Renderer Registry
// ============================================================================

/**
 * Singleton registry for managing language-specific code renderers.
 *
 * The registry provides centralized access to all available code generators
 * and enables querying language capabilities during semantic analysis.
 *
 * **Features:**
 * - Singleton pattern for global access
 * - Renderer lookup by language name (case-insensitive)
 * - Query available languages
 * - Access language metadata and keywords
 * - Automatic CppRenderer registration on first use
 *
 * **Usage Example:**
 * \code
 *   auto& registry = RendererRegistry::instance();
 *
 *   // Get available languages
 *   auto languages = registry.get_available_languages();
 *
 *   // Look up renderer
 *   auto* renderer = registry.get_renderer("cpp");
 *   if (renderer) {
 *       std::string code = renderer->render_module(ir_bundle, options);
 *   }
 *
 *   // Check if identifier conflicts with keywords
 *   if (registry.is_keyword_in_any_language("class")) {
 *       // Warn user about potential conflict
 *   }
 * \endcode
 *
 * **Thread Safety:** Registry is initialized once and read-only after initialization.
 */
class RendererRegistry {
public:
    // ========================================================================
    // Singleton Access
    // ========================================================================

    /**
     * Get the global renderer registry instance.
     * @return Reference to singleton registry
     */
    static RendererRegistry& instance();

    // Delete copy/move constructors and assignment operators
    RendererRegistry(const RendererRegistry&) = delete;
    RendererRegistry(RendererRegistry&&) = delete;
    RendererRegistry& operator=(const RendererRegistry&) = delete;
    RendererRegistry& operator=(RendererRegistry&&) = delete;

    // ========================================================================
    // Renderer Registration & Lookup
    // ========================================================================

    /**
     * Register a renderer for a specific language.
     *
     * @param language_name Language identifier (e.g., "cpp", "rust", "python")
     * @param renderer Unique pointer to renderer implementation
     *
     * @note Language names are stored in lowercase for case-insensitive lookup
     * @note If a renderer is already registered, it will be replaced
     */
    void register_renderer(const std::string& language_name,
                          std::unique_ptr<BaseRenderer> renderer);

    /**
     * Look up a renderer by language name.
     *
     * @param language_name Language identifier (case-insensitive)
     * @return Pointer to renderer, or nullptr if not found
     *
     * @note Lookup is case-insensitive ("CPP" == "cpp" == "Cpp")
     */
    BaseRenderer* get_renderer(const std::string& language_name) const;

    /**
     * Check if a renderer is registered for a language.
     *
     * @param language_name Language identifier (case-insensitive)
     * @return true if renderer exists
     */
    bool has_renderer(const std::string& language_name) const;

    /**
     * Register a renderer plugin.
     *
     * Plugins enable automatic renderer registration without creating
     * compile-time dependencies in the registry. The plugin's
     * register_renderer() method is called immediately.
     *
     * @param plugin Unique pointer to plugin implementation
     *
     * @note Plugins are stored for lifetime management and metadata queries
     * @note This method is typically called via REGISTER_RENDERER_PLUGIN macro
     *
     * @see renderer_plugin.hh for plugin interface and examples
     */
    void register_plugin(std::unique_ptr<RendererPlugin> plugin);

    // ========================================================================
    // Language Queries
    // ========================================================================

    /**
     * Get list of all registered language names.
     *
     * @return Vector of language identifiers (lowercase)
     */
    std::vector<std::string> get_available_languages() const;

    /**
     * Get language metadata for a specific language.
     *
     * @param language_name Language identifier (case-insensitive)
     * @return Language metadata, or nullopt if language not found
     */
    std::optional<LanguageMetadata> get_language_metadata(const std::string& language_name) const;

    // ========================================================================
    // Keyword Validation Helpers
    // ========================================================================

    /**
     * Check if identifier is a reserved keyword in a specific language.
     *
     * @param language_name Language identifier (case-insensitive)
     * @param identifier The identifier to check
     * @return true if identifier is a keyword in that language
     */
    bool is_keyword(const std::string& language_name, const std::string& identifier) const;

    /**
     * Check if identifier is a reserved keyword in ANY registered language.
     *
     * Useful for detecting potential cross-language conflicts.
     *
     * @param identifier The identifier to check
     * @return true if identifier is a keyword in any language
     */
    bool is_keyword_in_any_language(const std::string& identifier) const;

    /**
     * Find all languages where identifier is a reserved keyword.
     *
     * @param identifier The identifier to check
     * @return Vector of language names where identifier conflicts
     */
    std::vector<std::string> get_conflicting_languages(const std::string& identifier) const;

    /**
     * Get all keywords from all registered languages.
     *
     * @return Set of all keywords across all languages (deduplicated)
     */
    std::set<std::string> get_all_keywords() const;

private:
    // ========================================================================
    // Private Constructor & Initialization
    // ========================================================================

    /**
     * Private constructor for singleton pattern.
     * Renderers are registered via plugins during static initialization.
     */
    RendererRegistry();

    /**
     * Normalize language name to lowercase for case-insensitive lookup.
     */
    static std::string normalize_language_name(const std::string& name);

    // ========================================================================
    // State
    // ========================================================================

    /// Map of language name (lowercase) → renderer
    std::map<std::string, std::unique_ptr<BaseRenderer>> renderers_;

    /// Registered plugins (for lifetime management and metadata)
    std::vector<std::unique_ptr<RendererPlugin>> plugins_;
};

} // namespace datascript::codegen

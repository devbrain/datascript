//
// Renderer Registry Implementation
//
// Provides centralized management of language-specific code renderers.
//

#include <datascript/renderer_registry.hh>
#include <datascript/renderer_plugin.hh>
#include <algorithm>
#include <cctype>

namespace datascript::codegen {
    // ============================================================================
    // Singleton Access
    // ============================================================================

    RendererRegistry& RendererRegistry::instance() {
        static RendererRegistry registry;

        // Force plugin initialization by calling a function that triggers
        // static initialization in cpp_renderer_plugin.cc
        // This ensures CppRenderer is registered before first use
        extern void ensure_cpp_renderer_registered(); // Defined in cpp_renderer_plugin.cc
        static bool initialized = false;
        if (!initialized) {
            ensure_cpp_renderer_registered();
            initialized = true;
        }

        return registry;
    }

    // ============================================================================
    // Private Constructor
    // ============================================================================

    // Plugins will register themselves via static initialization (cpp_renderer_plugin.cc)
    // However, static initialization order is not guaranteed across translation units.
    // The RendererRegistry might be accessed before plugin initializers run (e.g., in tests).
    //
    // Solution: Ensure CppRenderer is registered after construction completes.
    // The plugin will call register_renderer() again, but that's okay - it will just replace
    // the existing registration with the same renderer instance.
    RendererRegistry::RendererRegistry() = default;

    // ============================================================================
    // Renderer Registration & Lookup
    // ============================================================================

    void RendererRegistry::register_renderer(const std::string& language_name,
                                             std::unique_ptr <BaseRenderer> renderer) {
        std::string normalized = normalize_language_name(language_name);
        renderers_[normalized] = std::move(renderer);
    }

    BaseRenderer* RendererRegistry::get_renderer(const std::string& language_name) const {
        std::string normalized = normalize_language_name(language_name);
        auto it = renderers_.find(normalized);
        return (it != renderers_.end()) ? it->second.get() : nullptr;
    }

    bool RendererRegistry::has_renderer(const std::string& language_name) const {
        std::string normalized = normalize_language_name(language_name);
        return renderers_.find(normalized) != renderers_.end();
    }

    void RendererRegistry::register_plugin(std::unique_ptr <RendererPlugin> plugin) {
        if (plugin) {
            // Call the plugin's registration method
            plugin->register_renderer(*this);

            // Store the plugin for lifetime management
            plugins_.push_back(std::move(plugin));
        }
    }

    // ============================================================================
    // Language Queries
    // ============================================================================

    std::vector <std::string> RendererRegistry::get_available_languages() const {
        std::vector <std::string> languages;
        languages.reserve(renderers_.size());

        for (const auto& [name, _] : renderers_) {
            languages.push_back(name);
        }

        return languages;
    }

    std::optional <LanguageMetadata> RendererRegistry::get_language_metadata(
        const std::string& language_name) const {
        BaseRenderer* renderer = get_renderer(language_name);
        if (!renderer) {
            return std::nullopt;
        }

        return renderer->get_metadata();
    }

    // ============================================================================
    // Keyword Validation Helpers
    // ============================================================================

    bool RendererRegistry::is_keyword(const std::string& language_name,
                                      const std::string& identifier) const {
        BaseRenderer* renderer = get_renderer(language_name);
        if (!renderer) {
            return false;
        }

        return renderer->is_reserved_keyword(identifier);
    }

    bool RendererRegistry::is_keyword_in_any_language(const std::string& identifier) const {
        for (const auto& [_, renderer] : renderers_) {
            if (renderer->is_reserved_keyword(identifier)) {
                return true;
            }
        }
        return false;
    }

    std::vector <std::string> RendererRegistry::get_conflicting_languages(
        const std::string& identifier) const {
        std::vector <std::string> conflicts;

        for (const auto& [language_name, renderer] : renderers_) {
            if (renderer->is_reserved_keyword(identifier)) {
                conflicts.push_back(language_name);
            }
        }

        return conflicts;
    }

    std::set <std::string> RendererRegistry::get_all_keywords() const {
        std::set <std::string> all_keywords;

        for (const auto& [_, renderer] : renderers_) {
            auto keywords = renderer->get_all_keywords();
            all_keywords.insert(keywords.begin(), keywords.end());
        }

        return all_keywords;
    }

    // ============================================================================
    // Private Helpers
    // ============================================================================

    std::string RendererRegistry::normalize_language_name(const std::string& name) {
        std::string normalized = name;
        std::transform(normalized.begin(), normalized.end(), normalized.begin(),
                       [](unsigned char c) { return std::tolower(c); });
        return normalized;
    }
} // namespace datascript::codegen

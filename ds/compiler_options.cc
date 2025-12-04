#include "compiler_options.hh"
#include "logger.hh"
#include <datascript/codegen/option_description.hh>
#include <datascript/renderer_registry.hh>
#include <algorithm>
#include <cstring>
#include <iostream>
#include <optional>
#include <stdexcept>
#include <tuple>

namespace datascript::driver {

using namespace datascript::codegen;

// ============================================================================
// Helper Functions
// ============================================================================

static bool starts_with(const char* str, const char* prefix) {
    return std::strncmp(str, prefix, std::strlen(prefix)) == 0;
}

static std::string get_option_value(const char* arg, const char* prefix) {
    return arg + std::strlen(prefix);
}

// Parse generator-specific option: --cpp-exceptions=false
// Returns: (renderer, option_name, parsed_value)
static std::optional<std::tuple<BaseRenderer*, std::string, OptionValue>>
parse_generator_option(
    const char* arg,
    const std::map<std::string, BaseRenderer*>& prefix_to_renderer)
{
    // arg format: --<prefix>-<option>=<value>
    std::string_view sv(arg + 2);  // Skip "--"

    // Find first dash: "cpp-exceptions=false" -> dash at position 3
    size_t dash_pos = sv.find('-');
    if (dash_pos == std::string_view::npos) {
        return std::nullopt;
    }

    std::string prefix(sv.substr(0, dash_pos));

    // Check if this prefix exists
    auto it = prefix_to_renderer.find(prefix);
    if (it == prefix_to_renderer.end()) {
        return std::nullopt;  // Not a generator option
    }

    BaseRenderer* renderer = it->second;

    // Parse option name and value: "exceptions=false"
    std::string_view rest = sv.substr(dash_pos + 1);
    size_t eq_pos = rest.find('=');
    if (eq_pos == std::string_view::npos) {
        throw std::runtime_error(
            "Generator option requires value: --" + prefix + "-<option>=<value>"
        );
    }

    std::string option_name(rest.substr(0, eq_pos));
    std::string value_str(rest.substr(eq_pos + 1));

    // Find option description to parse value correctly
    auto options = renderer->get_options();
    auto opt_it = std::find_if(options.begin(), options.end(),
        [&](const OptionDescription& opt) { return opt.name == option_name; });

    if (opt_it == options.end()) {
        throw std::runtime_error(
            "Unknown option for " + prefix + " generator: " + option_name
        );
    }

    // Parse value according to type
    OptionValue value;
    switch (opt_it->type) {
        case OptionType::Bool: {
            if (value_str == "true" || value_str == "yes" ||
                value_str == "on" || value_str == "1") {
                value = true;
            } else if (value_str == "false" || value_str == "no" ||
                       value_str == "off" || value_str == "0") {
                value = false;
            } else {
                throw std::runtime_error(
                    "Invalid boolean value: " + value_str +
                    " (expected: true/false, yes/no, on/off, 1/0)"
                );
            }
            break;
        }

        case OptionType::Int: {
            try {
                value = static_cast<int64_t>(std::stoll(value_str));
            } catch (...) {
                throw std::runtime_error("Invalid integer value: " + value_str);
            }
            break;
        }

        case OptionType::String: {
            value = value_str;
            break;
        }

        case OptionType::Choice: {
            if (std::find(opt_it->choices.begin(), opt_it->choices.end(), value_str)
                == opt_it->choices.end()) {
                std::string choices_str;
                for (size_t i = 0; i < opt_it->choices.size(); ++i) {
                    if (i > 0) choices_str += ", ";
                    choices_str += opt_it->choices[i];
                }
                throw std::runtime_error(
                    "Invalid choice for " + opt_it->name + ": " + value_str +
                    "\nValid choices: " + choices_str
                );
            }
            value = value_str;
            break;
        }
    }

    return std::make_tuple(renderer, option_name, value);
}

// ============================================================================
// Main Parser
// ============================================================================

CompilerOptions parse_command_line(int argc, char** argv) {
    CompilerOptions opts;

    // Build prefix->renderer map for generator options
    auto& registry = RendererRegistry::instance();
    std::map<std::string, BaseRenderer*> prefix_to_renderer;
    for (const auto& name : registry.get_available_languages()) {
        auto* renderer = registry.get_renderer(name);
        if (renderer) {
            prefix_to_renderer[renderer->get_option_prefix()] = renderer;
        }
    }

    for (int i = 1; i < argc; ++i) {
        const char* arg = argv[i];

        // Help options
        if (std::strcmp(arg, "-h") == 0 || std::strcmp(arg, "--help") == 0) {
            print_help(argv[0]);
            std::exit(0);
        }

        // Version
        if (std::strcmp(arg, "--version") == 0) {
            print_version();
            std::exit(0);
        }

        // List generators
        if (std::strcmp(arg, "--list-generators") == 0) {
            print_generators();
            std::exit(0);
        }

        // Verbosity
        if (std::strcmp(arg, "-v") == 0 || std::strcmp(arg, "--verbose") == 0) {
            opts.verbose = true;
            continue;
        }

        if (std::strcmp(arg, "-q") == 0 || std::strcmp(arg, "--quiet") == 0) {
            opts.quiet = true;
            continue;
        }

        // Output directory
        if (starts_with(arg, "-o")) {
            std::string value = get_option_value(arg, "-o");
            if (value.empty() && i + 1 < argc) {
                value = argv[++i];
            }
            if (value.empty()) {
                throw std::runtime_error("Option -o requires argument");
            }
            opts.output_dir = value;
            continue;
        }

        // Include paths
        if (starts_with(arg, "-I")) {
            std::string value = get_option_value(arg, "-I");
            if (value.empty() && i + 1 < argc) {
                value = argv[++i];
            }
            if (value.empty()) {
                throw std::runtime_error("Option -I requires argument");
            }
            opts.include_dirs.push_back(value);
            continue;
        }

        // Target language
        if (starts_with(arg, "-t")) {
            std::string value = get_option_value(arg, "-t");
            if (value.empty() && i + 1 < argc) {
                value = argv[++i];
            }
            if (value.empty()) {
                throw std::runtime_error("Option -t requires argument");
            }
            opts.target_language = value;
            continue;
        }

        // Warning options
        if (std::strcmp(arg, "-Werror") == 0) {
            opts.warnings_as_errors = true;
            continue;
        }

        if (std::strcmp(arg, "-w") == 0) {
            opts.suppress_all_warnings = true;
            continue;
        }

        if (starts_with(arg, "-Wno-")) {
            std::string warning = get_option_value(arg, "-Wno-");
            opts.disabled_warnings.insert(warning);
            continue;
        }

        if (starts_with(arg, "-Werror=")) {
            std::string warning = get_option_value(arg, "-Werror=");
            opts.warning_overrides[warning] = true;
            continue;
        }

        // Generator-specific options (--cpp-exceptions=false)
        if (starts_with(arg, "--") && std::strchr(arg + 2, '=')) {
            auto result = parse_generator_option(arg, prefix_to_renderer);
            if (result) {
                auto [renderer, option_name, value] = *result;
                // Store in CompilerOptions instead of setting immediately
                opts.generator_options[option_name] = value;
                continue;
            }
            // Fall through if not a generator option
        }

        // Unknown option starting with dash
        if (arg[0] == '-') {
            throw std::runtime_error(std::string("Unknown option: ") + arg);
        }

        // Input file
        opts.input_files.push_back(arg);
    }

    // Validation
    if (opts.input_files.empty()) {
        throw std::runtime_error("No input files specified");
    }

    if (opts.quiet && opts.verbose) {
        throw std::runtime_error("Cannot specify both -q/--quiet and -v/--verbose");
    }

    // Validate target language exists
    if (!registry.get_renderer(opts.target_language)) {
        auto available = registry.get_available_languages();
        std::string error_msg = "Unknown target language: " + opts.target_language;

        if (!available.empty()) {
            error_msg += "\n\nAvailable languages:";
            for (const auto& lang : available) {
                error_msg += "\n  - " + lang;
            }
        }

        error_msg += "\n\nUse --list-generators for more details.";
        throw std::runtime_error(error_msg);
    }

    return opts;
}

// ============================================================================
// Help and Info Functions
// ============================================================================

void print_help(const char* program_name) {
    std::cout << "Usage: " << program_name << " [options] <input-files>\n\n";

    std::cout << "Options:\n";
    std::cout << "  -h, --help              Show this help message\n";
    std::cout << "  --version               Show version information\n";
    std::cout << "  --list-generators       List available code generators\n";
    std::cout << "\n";

    std::cout << "Output:\n";
    std::cout << "  -o <dir>                Output directory (default: current directory)\n";
    std::cout << "  -t <lang>               Target language (default: cpp)\n";
    std::cout << "\n";

    std::cout << "Input:\n";
    std::cout << "  -I <dir>                Add include search path\n";
    std::cout << "\n";

    std::cout << "Diagnostics:\n";
    std::cout << "  -v, --verbose           Verbose output\n";
    std::cout << "  -q, --quiet             Quiet mode (errors only)\n";
    std::cout << "  -w                      Suppress all warnings\n";
    std::cout << "  -Werror                 Treat all warnings as errors\n";
    std::cout << "  -Werror=<code>          Treat specific warning as error\n";
    std::cout << "  -Wno-<code>             Disable specific warning\n";
    std::cout << "\n";

    std::cout << "Generator Options:\n";
    std::cout << "  --<prefix>-<option>=<value>    Set generator-specific option\n";
    std::cout << "\n";
    std::cout << "  Use --list-generators to see available generators and their options.\n";
    std::cout << "\n";

    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " protocol.ds\n";
    std::cout << "  " << program_name << " -t cpp -o output protocol.ds\n";
    std::cout << "  " << program_name << " --cpp-exceptions=false protocol.ds\n";
    std::cout << "  " << program_name << " --cpp-output-name=MyProtocol protocol.ds\n";
}

void print_version() {
    std::cout << "DataScript Compiler v0.1.0\n";
    std::cout << "Build: " << __DATE__ << " " << __TIME__ << "\n";
}

void print_generators() {
    auto& registry = RendererRegistry::instance();
    auto names = registry.get_available_languages();

    std::cout << "Available code generators:\n\n";

    for (const auto& name : names) {
        auto* renderer = registry.get_renderer(name);
        if (!renderer) continue;

        std::cout << "  " << name << " (" << renderer->get_language_name() << ")\n";
        std::cout << "    Prefix: " << renderer->get_option_prefix() << "\n";
        std::cout << "    Extension: " << renderer->get_file_extension() << "\n";

        auto options = renderer->get_options();
        if (!options.empty()) {
            std::cout << "    Options:\n";
            for (const auto& opt : options) {
                std::cout << "      --" << renderer->get_option_prefix()
                         << "-" << opt.name << "=<value>\n";
                std::cout << "        " << opt.description << "\n";
                if (opt.default_value) {
                    std::cout << "        Default: " << *opt.default_value << "\n";
                }
            }
        }
        std::cout << "\n";
    }
}

void print_search_dirs(const CompilerOptions& opts) {
    std::cout << "Include search paths:\n";
    for (const auto& dir : opts.include_dirs) {
        std::cout << "  " << dir << "\n";
    }
}

}  // namespace datascript::driver

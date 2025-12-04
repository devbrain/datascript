#pragma once

#include <iostream>
#include <string>

namespace datascript::driver {

enum class LogLevel {
    Quiet,   // Errors only
    Normal,  // Errors, warnings, info, success
    Verbose, // + verbose messages
    Debug    // + debug messages
};

enum class ColorMode {
    Auto,    // Auto-detect TTY
    Always,  // Force colors
    Never    // Disable colors
};

/**
 * Simple logger for CLI output with color support.
 * Uses termcolor for automatic TTY detection and color handling.
 *
 * Output routing:
 * - Errors, warnings → stderr
 * - Info, success, verbose, debug → stdout
 */
class Logger {
public:
    explicit Logger(LogLevel level = LogLevel::Normal,
                   ColorMode color = ColorMode::Auto);

    // Message types (errors/warnings to stderr, others to stdout)
    void error(const std::string& message);
    void warning(const std::string& message);
    void info(const std::string& message);
    void success(const std::string& message);
    void verbose(const std::string& message);
    void debug(const std::string& message);

    // Formatting helpers
    void bullet(const std::string& message, LogLevel min_level = LogLevel::Normal);
    void indent(const std::string& message, int spaces = 2, LogLevel min_level = LogLevel::Normal);
    void progress(const std::string& message);

    // Level control
    void set_level(LogLevel level) { level_ = level; }
    LogLevel get_level() const { return level_; }

private:
    LogLevel level_;
    ColorMode color_mode_;

    bool should_log(LogLevel required_level) const;
};

} // namespace datascript::driver

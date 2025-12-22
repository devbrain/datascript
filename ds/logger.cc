#include "logger.hh"
#include "vendor/termcolor.hpp"

namespace datascript::driver {

Logger::Logger(LogLevel level, ColorMode color)
    : level_(level)
    , color_mode_(color)
{
    // Configure color mode for all streams
    switch (color_mode_) {
        case ColorMode::Always:
            std::cout << termcolor::colorize;
            std::cerr << termcolor::colorize;
            break;
        case ColorMode::Never:
            std::cout << termcolor::nocolorize;
            std::cerr << termcolor::nocolorize;
            break;
        case ColorMode::Auto:
            // termcolor auto-detects TTY by default, no action needed
            break;
    }
}

bool Logger::should_log(LogLevel required_level) const {
    return static_cast<int>(level_) >= static_cast<int>(required_level);
}

void Logger::error(const std::string& message) {
    if (!should_log(LogLevel::Quiet)) return;

    std::cerr << termcolor::bold << termcolor::red
              << "error: " << termcolor::reset
              << message << "\n";
}

void Logger::warning(const std::string& message) {
    if (!should_log(LogLevel::Normal)) return;

    std::cerr << termcolor::bold << termcolor::yellow
              << "warning: " << termcolor::reset
              << message << "\n";
}

void Logger::info(const std::string& message) {
    if (!should_log(LogLevel::Normal)) return;

    std::cout << message << "\n";
}

void Logger::success(const std::string& message) {
    if (!should_log(LogLevel::Normal)) return;

    std::cout << termcolor::bold << termcolor::green
              << "✓ " << termcolor::reset
              << message << "\n";
}

void Logger::verbose(const std::string& message) {
    if (!should_log(LogLevel::Verbose)) return;

    std::cout << termcolor::cyan
              << message << termcolor::reset << "\n";
}

void Logger::debug(const std::string& message) {
    if (!should_log(LogLevel::Debug)) return;

    std::cout << termcolor::magenta
              << "[debug] " << termcolor::reset
              << message << "\n";
}

void Logger::bullet(const std::string& message, LogLevel min_level) {
    if (!should_log(min_level)) return;

    std::cout << "  • " << message << "\n";
}

void Logger::indent(const std::string& message, int spaces, LogLevel min_level) {
    if (!should_log(min_level)) return;

    std::cout << std::string(static_cast<size_t>(spaces), ' ') << message << "\n";
}

void Logger::progress(const std::string& message) {
    if (!should_log(LogLevel::Normal)) return;

    std::cout << termcolor::bold
              << message << termcolor::reset << "\n";
}

} // namespace datascript::driver

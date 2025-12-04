//
// Created by igor on 19/11/2025.
//

#include <iostream>
#include <sstream>
#include <string>
#include <filesystem>

#include "compiler.hh"
#include "compiler_options.hh"
#include "logger.hh"

int main(int argc, char* argv[]) {
    using namespace datascript::driver;

    try {
        // Parse command-line options (handles --help, --version, --list-generators automatically)
        CompilerOptions opts = parse_command_line(argc, argv);

        // Create logger based on verbosity options
        LogLevel log_level = LogLevel::Normal;
        if (opts.quiet) log_level = LogLevel::Quiet;
        if (opts.verbose) log_level = LogLevel::Verbose;

        Logger logger(log_level, ColorMode::Auto);

        // Create and run compiler
        Compiler compiler(opts, logger);
        return compiler.compile();



    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << std::endl;
        return 1;
    }
}
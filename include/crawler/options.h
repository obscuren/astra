#pragma once

#include <string>
#include <vector>
#include <iostream>

namespace crawler {

enum class RendererBackend {
    Terminal,
    SDL,
};

struct Options {
    RendererBackend backend = RendererBackend::Terminal;

    static Options parse(int argc, char* argv[]) {
        Options opts;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];

            if (arg == "--sdl") {
                opts.backend = RendererBackend::SDL;
            } else if (arg == "--terminal") {
                opts.backend = RendererBackend::Terminal;
            } else if (arg == "--help" || arg == "-h") {
                print_help(argv[0]);
                std::exit(0);
            } else {
                std::cerr << "Unknown option: " << arg << "\n";
                print_help(argv[0]);
                std::exit(1);
            }
        }

        return opts;
    }

    static void print_help(const char* program) {
        std::cout << "Usage: " << program << " [options]\n"
                  << "\n"
                  << "Options:\n"
                  << "  --terminal   Use terminal renderer (default)\n"
                  << "  --sdl        Use SDL2 graphical renderer\n"
                  << "  -h, --help   Show this help message\n";
    }
};

} // namespace crawler

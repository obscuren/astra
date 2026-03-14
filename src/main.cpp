#include "crawler/game.h"
#include "crawler/options.h"
#include "crawler/terminal_renderer.h"

#ifdef CRAWLER_HAS_SDL
#include "crawler/sdl_renderer.h"
#include <SDL3/SDL_main.h>
#endif

#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    try {
        auto opts = crawler::Options::parse(argc, argv);

        std::unique_ptr<crawler::Renderer> renderer;

        switch (opts.backend) {
            case crawler::RendererBackend::Terminal:
                renderer = std::make_unique<crawler::TerminalRenderer>();
                break;
            case crawler::RendererBackend::SDL:
#ifdef CRAWLER_HAS_SDL
                renderer = std::make_unique<crawler::SdlRenderer>();
#else
                std::cerr << "SDL support was not compiled in.\n"
                          << "Rebuild with: cmake -B build -DSDL=ON\n";
                return 1;
#endif
                break;
        }

        crawler::Game game(std::move(renderer));
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

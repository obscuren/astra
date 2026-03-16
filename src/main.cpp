#include "astra/game.h"
#include "astra/options.h"
#include "astra/terminal_renderer.h"

#ifdef ASTRA_HAS_SDL
#include "astra/sdl_renderer.h"
#include <SDL3/SDL_main.h>
#endif

#include <iostream>
#include <memory>

int main(int argc, char* argv[]) {
    try {
        auto opts = astra::Options::parse(argc, argv);

        std::unique_ptr<astra::Renderer> renderer;

        switch (opts.backend) {
            case astra::RendererBackend::Terminal:
                renderer = std::make_unique<astra::TerminalRenderer>();
                break;
            case astra::RendererBackend::SDL:
#ifdef ASTRA_HAS_SDL
                renderer = std::make_unique<astra::SdlRenderer>();
#else
                std::cerr << "SDL support was not compiled in.\n"
                          << "Rebuild with: cmake -B build -DSDL=ON\n";
                return 1;
#endif
                break;
        }

        astra::Game game(std::move(renderer));
        game.run();
    } catch (const std::exception& e) {
        std::cerr << "Fatal error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}

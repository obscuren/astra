#include "astra/game.h"
#include "astra/options.h"
#include "astra/terminal_renderer.h"
#include "astra/dungeon_level_generator.h"
#include "astra/dungeon_recipe.h"
#include "astra/map_generator.h"
#include "astra/map_properties.h"
#include "astra/tilemap.h"
#include "astra/dungeon/dungeon_style.h"

#ifdef ASTRA_HAS_SDL
#include "astra/sdl_renderer.h"
#include <SDL3/SDL_main.h>
#endif

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>

namespace {

// Headless pipeline dump — no renderer, no Game. Exercises just the dungeon
// generator + writes an ASCII grid to `path`. For diagnostic use.
int dump_dungeon_headless(const std::string& style_name,
                          const std::string& civ_name,
                          uint32_t seed,
                          int depth,
                          const std::string& path) {
    astra::dungeon::StyleId sid;
    if (!astra::dungeon::parse_style_id(style_name, sid)) {
        std::cerr << "unknown style: " << style_name << "\n";
        return 2;
    }

    astra::DungeonRecipe r;
    r.kind_tag    = "dev_dungen";
    r.level_count = depth;
    for (int d = 0; d < depth; ++d) {
        astra::DungeonLevelSpec s;
        s.style_id    = sid;
        s.civ_name    = civ_name;
        s.decay_level = std::max(0, 3 - d);   // 3 at L1, 2 at L2, 0 at L3
        s.is_boss_level = (d == depth - 1);
        if (s.is_boss_level) {
            s.fixtures.push_back(
                astra::PlannedFixture{ "nova_resonance_crystal", "required_plinth" });
        }
        r.levels.push_back(s);
    }

    auto props = astra::default_properties(astra::MapType::DerelictStation);
    astra::TileMap map(props.width, props.height, astra::MapType::DerelictStation);
    astra::generate_dungeon_level(map, r, depth, seed, {-1, -1});

    std::ofstream out(path);
    if (!out) {
        std::cerr << "cannot open " << path << "\n";
        return 3;
    }

    const int w = map.width();
    const int h = map.height();
    out << "=== astra dumpmap (headless) ===\n";
    out << "style: " << style_name << " civ: " << civ_name
        << " seed: " << seed << " depth: " << depth << "\n";
    out << "size: " << w << "x" << h << "\n";
    out << "regions: " << map.region_count() << "\n";
    out << "fixtures: " << map.fixture_count() << "\n\n";

    out << "--- tiles (fixtures overlay) ---\n";
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int fid = map.fixture_id(x, y);
            if (fid >= 0) {
                auto ft = map.fixture(fid).type;
                char c = '?';
                switch (ft) {
                    case astra::FixtureType::StairsUp:         c = '<'; break;
                    case astra::FixtureType::StairsDown:       c = '>'; break;
                    case astra::FixtureType::Plinth:           c = 'T'; break;
                    case astra::FixtureType::Altar:            c = 'A'; break;
                    case astra::FixtureType::Inscription:      c = 'i'; break;
                    case astra::FixtureType::Pillar:           c = 'I'; break;
                    case astra::FixtureType::Brazier:          c = '*'; break;
                    case astra::FixtureType::ResonancePillar:  c = '%'; break;
                    case astra::FixtureType::ResonancePillarTop:c = '^'; break;
                    case astra::FixtureType::ResonancePillarBot:c = 'v'; break;
                    case astra::FixtureType::PrecursorBracketL:c = '('; break;
                    case astra::FixtureType::PrecursorBracketR:c = ')'; break;
                    case astra::FixtureType::QuestFixture:     c = 'Q'; break;
                    case astra::FixtureType::Debris:           c = ','; break;
                    case astra::FixtureType::Crate:            c = 'x'; break;
                    case astra::FixtureType::Shelf:            c = '['; break;
                    case astra::FixtureType::Table:            c = 'o'; break;
                    case astra::FixtureType::DungeonHatch:     c = 'v'; break;
                    default:                                    c = 'F'; break;
                }
                out << c;
                continue;
            }
            auto t = map.get(x, y);
            char c = '?';
            switch (t) {
                case astra::Tile::Floor:          c = '.'; break;
                case astra::Tile::Wall:           c = '#'; break;
                case astra::Tile::StructuralWall: c = 'H'; break;
                case astra::Tile::Empty:          c = ' '; break;
                case astra::Tile::Water:          c = '~'; break;
                case astra::Tile::Portal:         c = 'O'; break;
                default:                           c = '?'; break;
            }
            out << c;
        }
        out << '\n';
    }

    out << "\n--- region ids ---\n";
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            int rid = map.region_id(x, y);
            if (rid < 0)       out << '.';
            else if (rid < 10) out << static_cast<char>('0' + rid);
            else if (rid < 36) out << static_cast<char>('a' + (rid - 10));
            else               out << '+';
        }
        out << '\n';
    }

    out << "\n--- fixtures list ---\n";
    for (int i = 0; i < map.fixture_count(); ++i) {
        const auto& f = map.fixture(i);
        int fx = -1, fy = -1;
        for (int y = 0; y < h && fx < 0; ++y) {
            for (int x = 0; x < w && fx < 0; ++x) {
                if (map.fixture_id(x, y) == i) { fx = x; fy = y; }
            }
        }
        out << "  [" << i << "] type=" << static_cast<int>(f.type)
            << " at (" << fx << "," << fy << ")"
            << " passable=" << (f.passable ? 'y' : 'n');
        if (!f.quest_fixture_id.empty()) out << " qid=\"" << f.quest_fixture_id << "\"";
        out << '\n';
    }

    std::cout << "wrote " << path << "\n";
    return 0;
}

} // namespace

int main(int argc, char* argv[]) {
    try {
        // Handle --dump-dungeon before normal arg parsing so we don't need a renderer.
        for (int i = 1; i < argc; ++i) {
            if (std::strcmp(argv[i], "--dump-dungeon") == 0) {
                if (i + 4 >= argc) {
                    std::cerr << "usage: --dump-dungeon <style> <civ> <seed> <depth> [<out-path>]\n";
                    return 1;
                }
                std::string style = argv[i + 1];
                std::string civ   = argv[i + 2];
                uint32_t seed     = static_cast<uint32_t>(std::strtoul(argv[i + 3], nullptr, 10));
                int depth         = std::atoi(argv[i + 4]);
                std::string out   = (i + 5 < argc) ? argv[i + 5] : std::string{"/tmp/astra_map.txt"};
                return dump_dungeon_headless(style, civ, seed, depth, out);
            }
        }

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

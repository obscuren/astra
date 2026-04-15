// src/terminal_theme.cpp
#include "terminal_theme.h"
#include "astra/item_ids.h"
#include "astra/npc.h"
#include "astra/quest_fixture.h"
#include "astra/race.h"
#include "astra/render_descriptor.h"

namespace astra {

// ---------------------------------------------------------------------------
// Biome palette — mirrors biome_colors() in tilemap.cpp
// ---------------------------------------------------------------------------

// Unified remembered/shadow color — dark navy blue
static constexpr Color REMEMBERED_COLOR = static_cast<Color>(60);  // 256-color muted slate blue

ThemeBiomeColors biome_palette(Biome biome) {
    switch (biome) {
        case Biome::Station:
            return {Color::White, Color::Default, Color::Blue, Color::Blue};
        case Biome::Rocky:
            return {Color::White, Color::DarkGray, Color::Blue, REMEMBERED_COLOR};
        case Biome::Volcanic:
            return {Color::Red, static_cast<Color>(52), Color::Red, REMEMBERED_COLOR};
        case Biome::Ice:
            return {Color::Cyan, Color::White, static_cast<Color>(39), REMEMBERED_COLOR};
        case Biome::Sandy:
            return {Color::Yellow, static_cast<Color>(180), Color::Blue, REMEMBERED_COLOR};
        case Biome::Aquatic:
            return {static_cast<Color>(30), static_cast<Color>(24), Color::Blue, REMEMBERED_COLOR};
        case Biome::Fungal:
            return {Color::Green, static_cast<Color>(22), Color::Green, REMEMBERED_COLOR};
        case Biome::Crystal:
            return {Color::BrightMagenta, Color::Magenta, Color::Magenta, REMEMBERED_COLOR};
        case Biome::Corroded:
            return {static_cast<Color>(142), static_cast<Color>(58), static_cast<Color>(148), static_cast<Color>(58)};
        case Biome::Forest:
            return {Color::Green, static_cast<Color>(58), Color::Blue, REMEMBERED_COLOR};
        case Biome::Grassland:
            return {Color::DarkGray, Color::Green, Color::Blue, REMEMBERED_COLOR};
        case Biome::Jungle:
            return {static_cast<Color>(22), static_cast<Color>(22), static_cast<Color>(30), REMEMBERED_COLOR};
        case Biome::Marsh:
            return {static_cast<Color>(23), static_cast<Color>(29), static_cast<Color>(33), REMEMBERED_COLOR};
        case Biome::Mountains:
            return {Color::White, static_cast<Color>(243), Color::Blue, REMEMBERED_COLOR};
        case Biome::MartianBarren:
            return {static_cast<Color>(130), static_cast<Color>(94), Color::Blue, REMEMBERED_COLOR};
        case Biome::MartianPolar:
            return {static_cast<Color>(225), static_cast<Color>(180), static_cast<Color>(39), REMEMBERED_COLOR};
        case Biome::AlienCrystalline:
            return {Color::Cyan, static_cast<Color>(51), static_cast<Color>(39), REMEMBERED_COLOR};
        case Biome::AlienOrganic:
            return {Color::Red, Color::Magenta, static_cast<Color>(88), REMEMBERED_COLOR};
        case Biome::AlienGeometric:
            return {Color::Yellow, static_cast<Color>(136), Color::Yellow, REMEMBERED_COLOR};
        case Biome::AlienVoid:
            return {static_cast<Color>(90), Color::DarkGray, Color::Magenta, REMEMBERED_COLOR};
        case Biome::AlienLight:
            return {static_cast<Color>(228), Color::White, static_cast<Color>(230), REMEMBERED_COLOR};
        case Biome::ScarredGlassed:
            return {static_cast<Color>(208), static_cast<Color>(94), static_cast<Color>(136), REMEMBERED_COLOR};
        case Biome::ScarredScorched:
            return {Color::DarkGray, static_cast<Color>(52), Color::DarkGray, REMEMBERED_COLOR};
    }
    return {Color::White, Color::Default, Color::Blue, REMEMBERED_COLOR};
}

// ---------------------------------------------------------------------------
// Helper: select variant from an array using seed
// ---------------------------------------------------------------------------

template<int N>
static const char* select_variant(const char* const (&arr)[N], uint8_t seed) {
    return arr[seed % N];
}

// ---------------------------------------------------------------------------
// Starfield — mirrors star_at() in map_renderer.cpp
// seed < 8 gives ~3% density (8/256 = 3.125%)
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_starfield(uint8_t seed) {
    if (seed >= 8) return {' ', nullptr, Color::Default, Color::Default};
    // seed 0-4: dim dot, 5-6: bright star, 7: cross
    if (seed < 5) return {'.', nullptr, Color::Cyan, Color::Default};
    if (seed < 7) return {'*', nullptr, Color::White, Color::Default};
    return {'+', nullptr, Color::White, Color::Default};
}

// ---------------------------------------------------------------------------
// Overworld tile color — mirrors overworld_tile_color() in map_renderer.cpp
// ---------------------------------------------------------------------------

static Color ow_tile_color(Tile tile, Biome biome) {
    switch (tile) {
        case Tile::OW_Plains:
            switch (biome) {
                case Biome::Ice:   return Color::White;
                case Biome::Rocky: return Color::DarkGray;
                case Biome::Sandy: return Color::Yellow;
                default:           return Color::Green;
            }
        case Tile::OW_Mountains:
            if (biome == Biome::Sandy) return static_cast<Color>(130); // rust mountains
            return Color::White;
        case Tile::OW_Crater:
            if (biome == Biome::Sandy) return static_cast<Color>(94);  // dark rust craters
            return Color::DarkGray;
        case Tile::OW_IceField:    return Color::Cyan;
        case Tile::OW_LavaFlow:    return Color::Red;
        case Tile::OW_Desert:      return Color::Yellow;
        case Tile::OW_Fungal:      return Color::Green;
        case Tile::OW_Forest:      return static_cast<Color>(34);  // overridden per-variant in resolve
        case Tile::OW_River:       return Color::Blue;
        case Tile::OW_Lake:        return Color::Cyan;
        case Tile::OW_Swamp:       return static_cast<Color>(22);
        case Tile::OW_Barren:
            // Sandy biome = Mars-like rust color
            if (biome == Biome::Sandy) return static_cast<Color>(166); // rust red
            return Color::DarkGray;
        case Tile::OW_CaveEntrance:return Color::Magenta;
        case Tile::OW_Ruins:       return static_cast<Color>(178); // warm gold
        case Tile::OW_Settlement:  return Color::Yellow;
        case Tile::OW_CrashedShip: return Color::Cyan;
        case Tile::OW_Outpost:     return Color::Green;
        case Tile::OW_Beacon:      return Color::Cyan;
        case Tile::OW_Megastructure: return Color::Yellow;
        case Tile::OW_AlienTerrain: return Color::BrightMagenta;
        case Tile::OW_ScorchedEarth: return static_cast<Color>(208); // orange
        case Tile::OW_GlassedCrater: return static_cast<Color>(136); // dark yellow/brown
        case Tile::OW_Landing:     return static_cast<Color>(14); // bright cyan
        default:                   return Color::White;
    }
}

// ---------------------------------------------------------------------------
// Overworld glyph — mirrors overworld_glyph() in tilemap.h
// Uses seed for position variation instead of inline hash.
// ---------------------------------------------------------------------------

static const char* ow_glyph(Tile t, uint8_t seed) {
    switch (t) {
        case Tile::OW_Mountains: {
            static const char* g[] = {
                "\xe2\x96\xb2",  // ▲
                "\xe2\x88\xa9",  // ∩
                "^",
                "\xce\x93",      // Γ
                "\xe2\x96\xb2",  // ▲
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Forest: {
            static const char* g[] = {
                "\xe2\x99\xa3",  // ♣
                "\xca\xae",      // ʮ
                "\xd0\xa6",      // Ц
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Plains: {
            static const char* g[] = {
                "\xc2\xb7",      // ·
                ".",
                ",",
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Desert: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xc2\xb7",      // ·
                ".",
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Lake:
            return "\xe2\x89\x88"; // ≈
        case Tile::OW_River: {
            static const char* g[] = {
                "\xe2\x89\x88",  // ≈
                "~",
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Swamp: {
            static const char* g[] = {
                "\xd5\x88",      // Ո
                "\xca\x83",      // ʃ
                "\xd5\x88",      // Ո
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Barren: {
            static const char* g[] = {
                "\xc2\xb7",      // ·
                ",",
                "\xe2\x96\x91",  // ░
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Fungal: {
            static const char* g[] = {
                "\xce\xa6",      // Φ
                "\"",
                "\xcf\x84",      // τ
            };
            return select_variant(g, seed);
        }
        case Tile::OW_IceField: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xc2\xb7",      // ·
                "'",
            };
            return select_variant(g, seed);
        }
        case Tile::OW_LavaFlow: {
            static const char* g[] = {
                "\xe2\x89\x88",  // ≈
                "~",
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Crater: {
            static const char* g[] = {
                "o",
                "\xc2\xb0",      // °
            };
            return select_variant(g, seed);
        }
        case Tile::OW_CaveEntrance: {
            static const char* g[] = {
                "\xe2\x96\xbc",  // ▼
                "\xce\x98",      // Θ
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Ruins: {
            // Baroque pipe rendering handled via neighbor mask in resolve();
            // fallback glyph for legacy path
            return "\xe2\x96\xa0";  // ■
        }
        case Tile::OW_Settlement:  return "\xe2\x96\xb2"; // ▲
        case Tile::OW_CrashedShip: {
            static const char* g[] = {
                "%",
                "\xc2\xa4",      // ¤
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Outpost:     return "+";
        case Tile::OW_Beacon:      return "\xe2\x8c\xbe"; // ⌾
        case Tile::OW_Megastructure: return "\xe2\x97\x88"; // ◈
        case Tile::OW_AlienTerrain: {
            static const char* g[] = {
                "\xe2\x88\x97",  // ∗
                "\xc2\xb7",      // ·
                "\xe2\x97\x87",  // ◇
            };
            return select_variant(g, seed);
        }
        case Tile::OW_ScorchedEarth: {
            static const char* g[] = {
                "\xe2\x89\x88",  // ≈
                "~",
                "\xc2\xb7",      // ·
            };
            return select_variant(g, seed);
        }
        case Tile::OW_GlassedCrater: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "\xe2\x96\x91",  // ░
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Landing:     return "\xe2\x89\xa1"; // ≡
        default:                   return " ";
    }
}

// ---------------------------------------------------------------------------
// Dungeon wall glyph — mirrors dungeon_wall_glyph() in tilemap.h
// ---------------------------------------------------------------------------

static const char* wall_glyph_for_biome(Biome biome, uint8_t seed) {
    switch (biome) {
        case Biome::Station:
            return "\xe2\x96\x88";  // █
        case Biome::Rocky: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xe2\x96\x91",  // ░
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Volcanic: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
            };
            return select_variant(g, seed);
        }
        case Biome::Ice: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xe2\x96\x91",  // ░
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Sandy: {
            static const char* g[] = {
                "\xe2\x96\x92",  // ▒
                "\xe2\x96\x91",  // ░
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Aquatic: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
            };
            return select_variant(g, seed);
        }
        case Biome::Fungal: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Crystal: {
            static const char* g[] = {
                "\xe2\x97\x86",  // ◆
                "\xe2\x97\x87",  // ◇
            };
            return select_variant(g, seed);
        }
        case Biome::Corroded: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "#",
                "\xe2\x96\x92",  // ▒
            };
            return select_variant(g, seed);
        }
        case Biome::Forest: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Grassland: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░
                "\xe2\x96\x91",  // ░
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Jungle: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
            };
            return select_variant(g, seed);
        }
        case Biome::Marsh: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::Mountains: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓ dense rock
                "\xe2\x96\x92",  // ▒ rough stone
                "\xe2\x96\x91",  // ░ crumbling face
                "#",              // solid cliff
                "\xe2\x88\xa9",  // ∩ rounded peak
            };
            return select_variant(g, seed);
        }
        case Biome::MartianBarren: {
            static const char* g[] = {
                "\xe2\x96\x93",  // ▓
                "\xe2\x96\x92",  // ▒
                "\xe2\x96\x91",  // ░
                "#",
            };
            return select_variant(g, seed);
        }
        case Biome::MartianPolar: {
            static const char* g[] = {
                "\xe2\x96\x91",  // ░ frost-rimed stone
                "\xe2\x96\x92",  // ▒
                "#",
            };
            return select_variant(g, seed);
        }
        default:
            return "#";
    }
}

// ---------------------------------------------------------------------------
// Water glyph — mirrors dungeon_water_glyph() in tilemap.h
// ---------------------------------------------------------------------------

static const char* water_glyph(uint8_t seed) {
    static const char* g[] = {
        "\xe2\x89\x88",  // ≈
        "\xe2\x89\x88",  // ≈
        "~",
    };
    return select_variant(g, seed);
}

// ---------------------------------------------------------------------------
// Floor scatter — mirrors floor_scatter() in map_renderer.cpp
// seed % 100 for threshold, (seed >> 4) % count for glyph selection
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_floor(uint8_t seed, Biome biome, Color floor_color, Color remembered_color) {
    if (biome == Biome::Station) {
        return {'.', nullptr, floor_color, Color::Default};
    }

    int roll = seed % 100;
    int variant = (seed >> 4) % 8; // use different bits for variant selection

    // --- Basic scatter in dim color (~15% density) ---
    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:    s = {15, ",:`",  3}; break;
        case Biome::Volcanic: s = {15, ",';" , 3}; break;
        case Biome::Ice:      s = {12, "'`,",  3}; break;
        case Biome::Sandy:    s = {15, ",`:",  3}; break;
        case Biome::Aquatic:  s = {10, ",:",   2}; break;
        case Biome::Fungal:   s = {15, "\",'", 3}; break;
        case Biome::Crystal:  s = {15, "*'`",  3}; break;
        case Biome::Corroded: s = {15, ",:;",  3}; break;
        case Biome::Forest:   s = {15, "\",'", 3}; break;
        case Biome::Grassland:s = {15, ",`.",  3}; break;
        case Biome::Jungle:   s = {15, "\",'", 3}; break;
        case Biome::Marsh:    s = {15, "\",~", 3}; break;
        case Biome::Mountains:s = {18, ",:^",  3}; break;
        case Biome::MartianBarren: s = {15, ",.:",  3}; break;
        case Biome::MartianPolar:  s = {12, "'`,",  3}; break;
        default: return {'.', nullptr, floor_color, Color::Default};
    }

    if (roll >= s.threshold) {
        // Base floor with subtle green shade variation for organic biomes
        if (biome == Biome::Grassland || biome == Biome::Forest
            || biome == Biome::Jungle || biome == Biome::Marsh) {
            // Vary between 2-3 shades of green based on position
            static const Color greens[] = {
                Color::Green,
                static_cast<Color>(28),  // darker green
                static_cast<Color>(34),  // mid green
            };
            return {'.', nullptr, greens[variant % 3], Color::Default};
        }
        return {'.', nullptr, floor_color, Color::Default};
    }

    // Grassland gets richer scatter set with occasional Y
    if (biome == Biome::Grassland) {
        static const ResolvedVisual grass_scatter[] = {
            {',', nullptr, Color::Green, Color::Default},
            {',', nullptr, static_cast<Color>(28), Color::Default},      // dark green comma
            {'`', nullptr, static_cast<Color>(34), Color::Default},      // mid green backtick
            {'\'', nullptr, Color::Green, Color::Default},               // apostrophe
            {'.', "\xc2\xb7", static_cast<Color>(64), Color::Default},  // · yellow-green
        };
        return grass_scatter[variant % 5];
    }

    char scatter = s.glyphs[variant % s.count];
    return {scatter, nullptr, remembered_color, Color::Default};
}

// ---------------------------------------------------------------------------
// Structural wall — mirrors structural wall rendering in map_renderer.cpp
// Material encoded via decode_wall_material() on the seed.
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_structural_wall(uint8_t seed) {
    uint8_t mat = decode_wall_material(seed);
    switch (mat) {
        case 1:  // Concrete
            return {'#', "\xe2\x96\x93", static_cast<Color>(245), Color::Default};  // ▓ medium gray
        case 2:  // Wood
            return {'#', "\xe2\x96\x92", static_cast<Color>(137), Color::Default};  // ▒ brown/tan
        case 3:  // Salvage
            return {'#', "\xe2\x96\x91", static_cast<Color>(240), Color::Default};  // ░ dark gray
        default: // Metal (0)
            return {'#', "\xe2\x96\x88", Color::White, Color::Default};              // █
    }
}

// ---------------------------------------------------------------------------
// Fixture resolution — mirrors make_fixture() in tilemap.cpp
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_fixture(uint16_t type_id, uint8_t flags, Biome biome, uint8_t seed) {
    auto type = static_cast<FixtureType>(type_id);
    bool remembered = (flags & RF_Remembered) != 0;
    bool open = (flags & RF_Open) != 0;

    ResolvedVisual vis;
    switch (type) {
        case FixtureType::Door:
            vis = open
                ? ResolvedVisual{'\'', nullptr, static_cast<Color>(137), Color::Default}
                : ResolvedVisual{'+', nullptr, static_cast<Color>(137), Color::Default};
            break;
        case FixtureType::Window:
            vis = {'O', nullptr, Color::Cyan, Color::Default}; break;
        case FixtureType::Table:
            vis = {'o', "\xc2\xa4", Color::DarkGray, Color::Default}; break;           // ¤
        case FixtureType::Console:
            vis = {'#', "\xe2\x95\xac", Color::Cyan, Color::Default}; break;           // ╬
        case FixtureType::Crate:
            vis = {'=', "\xe2\x96\xa0", Color::Yellow, Color::Default}; break;          // ■
        case FixtureType::Bunk:
            vis = {'=', "\xe2\x89\xa1", Color::DarkGray, Color::Default}; break;        // ≡
        case FixtureType::Rack:
            vis = {'|', "\xe2\x95\x8f", Color::DarkGray, Color::Default}; break;        // ╏
        case FixtureType::Conduit:
            vis = {'%', "\xe2\x95\xa3", Color::DarkGray, Color::Default}; break;        // ╣
        case FixtureType::ShuttleClamp:
            vis = {'=', "\xe2\x95\xa4", Color::White, Color::Default}; break;           // ╤
        case FixtureType::Shelf:
            vis = {'[', "\xe2\x95\x94", Color::DarkGray, Color::Default}; break;        // ╔
        case FixtureType::Viewport:
            vis = {'"', "\xe2\x96\x91", Color::Cyan, Color::Default}; break;            // ░
        case FixtureType::Torch:
            vis = {'*', nullptr, Color::Yellow, Color::Default}; break;
        case FixtureType::Stool:
            vis = {'o', "\xc2\xb7", Color::DarkGray, Color::Default}; break;            // ·
        case FixtureType::Debris:
            vis = {',', nullptr, Color::DarkGray, Color::Default}; break;
        case FixtureType::HealPod:
            vis = {'+', "\xe2\x9c\x9a", Color::Green, Color::Default}; break;           // ✚
        case FixtureType::FoodTerminal:
            vis = {'$', nullptr, Color::Yellow, Color::Default}; break;
        case FixtureType::WeaponDisplay:
            vis = {'/', "\xe2\x80\xa0", Color::Red, Color::Default}; break;             // †
        case FixtureType::RepairBench:
            vis = {'%', "\xe2\x95\xaa", Color::Cyan, Color::Default}; break;            // ╪
        case FixtureType::SupplyLocker:
            vis = {'&', "\xe2\x96\xaa", Color::Yellow, Color::Default}; break;          // ▪
        case FixtureType::StarChart:
            vis = {'*', nullptr, Color::Cyan, Color::Default}; break;
        case FixtureType::RestPod:
            vis = {'=', "\xe2\x88\xa9", Color::Green, Color::Default}; break;           // ∩
        case FixtureType::ShipTerminal:
            vis = {'>', "\xc2\xbb", Color::Yellow, Color::Default}; break;              // »
        case FixtureType::CommandTerminal:
            vis = {'#', "\xe2\x96\xa3", Color::Cyan, Color::Default}; break;            // ▣
        case FixtureType::DungeonHatch:
            vis = {'v', "\xe2\x96\xbc", Color::Yellow, Color::Default}; break;          // ▼
        case FixtureType::StairsUp:
            vis = {'<', "\xe2\x96\xb2", Color::White, Color::Default}; break;           // ▲
        case FixtureType::NaturalObstacle: {
            switch (biome) {
                case Biome::Grassland: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::DarkGray, Color::Default},              // ° grey boulder
                        {'o', "\xc2\xb0", Color::White, Color::Default},                  // ° pale rock
                        {'o', "\xe2\x97\x8b", static_cast<Color>(94), Color::Default},    // ○ brown rock
                        {'T', "\xce\xa6", Color::Green, Color::Default},                  // Φ lone tree
                        {'o', "\xe2\x88\x99", Color::DarkGray, Color::Default},           // ∙ small stone
                        {'#', "\xe2\x96\x91", static_cast<Color>(94), Color::Default},    // ░ log
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::Rocky: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::DarkGray, Color::Default},              // ° boulder
                        {'o', "\xc2\xb0", Color::White, Color::Default},                  // ° pale rock
                        {'#', "\xe2\x96\x93", Color::DarkGray, Color::Default},           // ▓ rock face
                        {'o', "\xe2\x97\x8b", Color::White, Color::Default},              // ○ round stone
                        {'^', nullptr, Color::DarkGray, Color::Default},                  // jagged rock
                    };
                    vis = variants[seed % 5]; break;
                }
                case Biome::Forest: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xa4", static_cast<Color>(94), Color::Default},        // ¤ brown stump
                        {'T', "\xce\xa6", static_cast<Color>(22), Color::Default},         // Φ pine tree (dark green)
                        {'#', "\xe2\x96\x92", Color::Green, Color::Default},              // ▒ thicket (bright green)
                        {'o', "\xe2\x97\x8b", static_cast<Color>(58), Color::Default},    // ○ mossy rock (olive)
                        {'T', "\xce\xa6", static_cast<Color>(28), Color::Default},         // Φ oak tree (green)
                        {'Y', nullptr, static_cast<Color>(64), Color::Default},             // birch tree (yellow-green)
                        {'T', "\xce\xa6", static_cast<Color>(34), Color::Default},         // Φ maple (mid green)
                        {'#', "\xe2\x96\x92", static_cast<Color>(22), Color::Default},    // ▒ underbrush (dark green)
                    };
                    vis = variants[seed % 8]; break;
                }
                case Biome::Jungle: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xa4", static_cast<Color>(22), Color::Default},        // ¤ thick trunk (dark green)
                        {'#', "\xe2\x96\x93", static_cast<Color>(28), Color::Default},    // ▓ root mass (green)
                        {'T', "\xce\xa6", Color::Green, Color::Default},                  // Φ giant fern (bright green)
                        {'#', "\xe2\x96\x92", static_cast<Color>(22), Color::Default},    // ▒ tangled vines (dark green)
                        {'T', "\xce\xa6", static_cast<Color>(64), Color::Default},         // Φ palm frond (yellow-green)
                        {'o', "\xc2\xa4", static_cast<Color>(34), Color::Default},         // ¤ tropical tree (mid green)
                        {'#', "\xe2\x96\x93", static_cast<Color>(29), Color::Default},    // ▓ moss cluster (teal-green)
                        {'Y', nullptr, static_cast<Color>(70), Color::Default},             // branching palm (light green)
                    };
                    vis = variants[seed % 8]; break;
                }
                case Biome::Sandy: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::DarkGray, Color::Default},              // ° large rock
                        {'o', "\xc2\xb0", Color::Yellow, Color::Default},                 // ° sandstone
                        {'^', nullptr, static_cast<Color>(180), Color::Default},           // sandy outcrop
                        {'T', "\xe2\x80\xa0", Color::Green, Color::Default},              // † tall cactus
                        {'Y', nullptr, Color::Green, Color::Default},                      // branching cactus
                        {'|', "\xe2\x94\x82", static_cast<Color>(22), Color::Default},    // │ thin cactus
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::Ice: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::Cyan, Color::Default},                  // ° frozen boulder
                        {'*', "\xe2\x97\x87", Color::White, Color::Default},              // ◇ ice crystal
                        {'#', "\xe2\x96\x91", Color::Cyan, Color::Default},               // ░ ice wall
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::Fungal: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xce\xa6", Color::Green, Color::Default},                  // Φ large mushroom
                        {'o', "\xce\xa6", Color::Magenta, Color::Default},                // Φ purple mushroom
                        {'#', "\xe2\x96\x93", static_cast<Color>(22), Color::Default},    // ▓ fungal mass
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::Volcanic: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::DarkGray, Color::Default},              // ° lava rock
                        {'o', "\xc2\xb0", Color::Red, Color::Default},                    // ° hot rock
                        {'^', nullptr, static_cast<Color>(52), Color::Default},            // obsidian spike
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::Aquatic: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", static_cast<Color>(30), Color::Default},        // ° wet rock
                        {'#', "\xe2\x96\x92", Color::Blue, Color::Default},               // ▒ coral
                    };
                    vis = variants[seed % 2]; break;
                }
                case Biome::Crystal: {
                    static const ResolvedVisual variants[] = {
                        {'*', "\xe2\x97\x87", Color::BrightMagenta, Color::Default},     // ◇ crystal
                        {'*', "\xe2\x97\x86", Color::Magenta, Color::Default},            // ◆ dark crystal
                        {'*', "\xe2\x9c\xb6", Color::Cyan, Color::Default},               // ✶ prism
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::Corroded: {
                    static const ResolvedVisual variants[] = {
                        {'#', "\xe2\x96\x91", static_cast<Color>(142), Color::Default},  // ░ collapsed
                        {'%', "\xe2\x9a\x99", Color::DarkGray, Color::Default},           // ⚙ wreckage
                        {'o', nullptr, static_cast<Color>(58), Color::Default},            // rusted lump
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::Marsh: {
                    static const ResolvedVisual variants[] = {
                        {'"', "\xcf\x84", static_cast<Color>(29), Color::Default},        // τ tall reeds
                        {'o', "\xc2\xb0", static_cast<Color>(23), Color::Default},        // ° soggy log
                        {'#', "\xe2\x96\x92", static_cast<Color>(23), Color::Default},    // ▒ dense reeds
                        {'~', "\xe2\x89\x88", static_cast<Color>(33), Color::Default},    // ≈ deep pool
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::Mountains: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", Color::White, Color::Default},                   // ° round boulder
                        {'^', "\xe2\x96\xb2", static_cast<Color>(245), Color::Default},    // ▲ stone peak
                        {'#', "\xe2\x96\x92", static_cast<Color>(243), Color::Default},    // ▒ rock face
                        {'o', nullptr, Color::DarkGray, Color::Default},                    // rubble pile
                        {'^', nullptr, static_cast<Color>(250), Color::Default},            // crag
                        {'#', "\xe2\x96\x91", static_cast<Color>(247), Color::Default},    // ░ scree
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::MartianBarren: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", static_cast<Color>(166), Color::Default},        // ° rust boulder
                        {'#', "\xe2\x96\x92", static_cast<Color>(130), Color::Default},    // ▒ iron-rich rock
                        {'o', nullptr, static_cast<Color>(94), Color::Default},             // dark rust pile
                        {'^', "\xe2\x96\xb2", static_cast<Color>(173), Color::Default},    // ▲ rust crag
                        {'#', "\xe2\x96\x91", static_cast<Color>(137), Color::Default},    // ░ dusty gravel
                    };
                    vis = variants[seed % 5]; break;
                }
                case Biome::MartianPolar: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xc2\xb0", static_cast<Color>(225), Color::Default},        // ° frosted rock
                        {'*', "\xe2\x97\x87", Color::White, Color::Default},                // ◇ ice chunk
                        {'#', "\xe2\x96\x92", static_cast<Color>(145), Color::Default},    // ▒ frozen dust
                    };
                    vis = variants[seed % 3]; break;
                }
                case Biome::AlienCrystalline: {
                    static const ResolvedVisual variants[] = {
                        {'*', "\xe2\x97\x86", Color::Cyan, Color::Default},            // ◆
                        {'o', "\xe2\x97\x87", Color::White, Color::Default},            // ◇
                        {'^', "\xe2\x96\xb3", static_cast<Color>(51), Color::Default},  // △ bright cyan
                        {'v', "\xe2\x96\xbd", Color::Cyan, Color::Default},             // ▽
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::AlienOrganic: {
                    static const ResolvedVisual variants[] = {
                        {'O', "\xce\x98", Color::Red, Color::Default},                  // Θ
                        {'~', "\xe2\x88\x9e", Color::Magenta, Color::Default},          // ∞
                        {'S', "\xc2\xa7", static_cast<Color>(52), Color::Default},      // §
                        {'~', "~", static_cast<Color>(88), Color::Default},             // ~ dark red
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::AlienGeometric: {
                    static const ResolvedVisual variants[] = {
                        {'o', "\xe2\x96\xa1", Color::Yellow, Color::Default},            // □
                        {'*', "\xe2\x96\xaa", Color::White, Color::Default},             // ▪
                        {'#', "\xe2\x95\xac", static_cast<Color>(136), Color::Default},  // ╬
                        {'+', "\xe2\x94\xbc", Color::Yellow, Color::Default},            // ┼
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::AlienVoid: {
                    static const ResolvedVisual variants[] = {
                        {'O', "\xe2\x97\x8f", static_cast<Color>(90), Color::Default},  // ●
                        {'o', "\xe2\x97\x8e", Color::DarkGray, Color::Default},          // ◎
                        {'0', "\xe2\x88\x85", Color::Magenta, Color::Default},           // ∅
                        {'v', "\xe2\x96\xbc", static_cast<Color>(90), Color::Default},   // ▼
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::AlienLight: {
                    static const ResolvedVisual variants[] = {
                        {'*', "\xe2\x9c\xa6", static_cast<Color>(228), Color::Default},  // ✦ bright yellow
                        {'o', "\xe2\x9c\xa7", Color::White, Color::Default},              // ✧
                        {'*', "\xe2\x88\x97", Color::Yellow, Color::Default},             // ∗
                        {'*', "\xe2\x98\x86", static_cast<Color>(230), Color::Default},  // ☆ bright white
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::ScarredGlassed: {
                    static const ResolvedVisual variants[] = {
                        {'#', "\xe2\x96\x93", static_cast<Color>(136), Color::Default},  // ▓
                        {'.', "\xe2\x96\x91", static_cast<Color>(208), Color::Default},  // ░ orange
                        {'~', "\xe2\x89\x88", Color::DarkGray, Color::Default},          // ≈
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},              // · dim
                    };
                    vis = variants[seed % 4]; break;
                }
                case Biome::ScarredScorched: {
                    static const ResolvedVisual variants[] = {
                        {'!', "\xe2\x89\xa0", Color::DarkGray, Color::Default},          // ≠
                        {',', ",", static_cast<Color>(52), Color::Default},              // , dark red
                        {'~', "~", static_cast<Color>(208), Color::Default},             // ~ orange
                        {'%', "%", Color::DarkGray, Color::Default},                     // % ash
                    };
                    vis = variants[seed % 4]; break;
                }
                default:
                    vis = {'o', "\xc2\xb0", Color::DarkGray, Color::Default}; break;
            }
            break;
        }
        case FixtureType::ShoreDebris: {
            // Each biome has a mix of sand, dirt, rocks, and biome-specific material.
            // Ratios differ: grassland is sandy, forest is dirty, jungle is mixed, etc.
            switch (biome) {
                case Biome::Grassland: {
                    // Mostly sand, some gravel, little dirt
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", static_cast<Color>(180), Color::Default},    // · sand
                        {',', nullptr, static_cast<Color>(180), Color::Default},         // , sand grain
                        {'.', "\xc2\xb7", static_cast<Color>(137), Color::Default},    // · wet sand
                        {',', nullptr, static_cast<Color>(180), Color::Default},         // , sand
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : gravel
                        {'.', "\xc2\xb7", static_cast<Color>(94), Color::Default},     // · dirt patch
                        {',', nullptr, static_cast<Color>(180), Color::Default},         // , sand
                        {'o', "\xc2\xb7", Color::DarkGray, Color::Default},             // · river stone
                    };
                    vis = variants[seed % 8]; break;
                }
                case Biome::Forest: {
                    // Mostly dirt and mud, some rocks, little sand
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", static_cast<Color>(94), Color::Default},     // · dirt
                        {',', nullptr, static_cast<Color>(130), Color::Default},         // , brown mud
                        {'.', "\xc2\xb7", static_cast<Color>(58), Color::Default},     // · dark earth
                        {',', nullptr, static_cast<Color>(94), Color::Default},          // , dirt
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : pebbles
                        {'.', "\xc2\xb7", static_cast<Color>(130), Color::Default},    // · wet mud
                        {'.', "\xc2\xb7", static_cast<Color>(180), Color::Default},    // · sand patch
                        {'o', "\xc2\xb7", Color::DarkGray, Color::Default},             // · stone
                    };
                    vis = variants[seed % 8]; break;
                }
                case Biome::Jungle: {
                    // Mix of sand, mud, rocks, exposed roots
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", static_cast<Color>(137), Color::Default},    // · sandy mud
                        {',', nullptr, static_cast<Color>(94), Color::Default},          // , wet earth
                        {'.', "\xc2\xb7", static_cast<Color>(180), Color::Default},    // · sand
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : river stones
                        {',', nullptr, static_cast<Color>(130), Color::Default},         // , clay
                        {'.', "\xc2\xb7", static_cast<Color>(94), Color::Default},     // · dirt
                        {',', nullptr, static_cast<Color>(22), Color::Default},          // , exposed root
                        {':', nullptr, static_cast<Color>(180), Color::Default},          // : coarse sand
                    };
                    vis = variants[seed % 8]; break;
                }
                case Biome::Volcanic: {
                    // Scorched rock, obsidian, basalt, embers
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},             // · scorched rock
                        {',', nullptr, static_cast<Color>(52), Color::Default},          // , obsidian chip
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : basalt gravel
                        {'.', "\xc2\xb7", static_cast<Color>(208), Color::Default},    // · cooling ember
                        {',', nullptr, Color::DarkGray, Color::Default},                 // , ash
                        {':', nullptr, static_cast<Color>(52), Color::Default},           // : dark rock
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::Marsh: {
                    // Soggy mud, peat, wet earth, some sand
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", static_cast<Color>(94), Color::Default},     // · soggy earth
                        {',', nullptr, static_cast<Color>(130), Color::Default},         // , wet mud
                        {'.', "\xc2\xb7", static_cast<Color>(58), Color::Default},     // · peat
                        {',', nullptr, static_cast<Color>(94), Color::Default},          // , muck
                        {'.', "\xc2\xb7", static_cast<Color>(137), Color::Default},    // · damp sand
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : clay pebbles
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::Fungal: {
                    // Spore residue, slime, moss, some dirt
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", static_cast<Color>(22), Color::Default},     // · spore residue
                        {',', nullptr, Color::Magenta, Color::Default},                  // , fungal film
                        {'.', "\xc2\xb7", static_cast<Color>(28), Color::Default},     // · moss
                        {',', nullptr, static_cast<Color>(94), Color::Default},          // , earthy muck
                        {':', nullptr, static_cast<Color>(22), Color::Default},           // : fungal crust
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},             // · damp rock
                    };
                    vis = variants[seed % 6]; break;
                }
                case Biome::Ice: {
                    // Frost, snow, ice chips, some gravel
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", Color::White, Color::Default},                // · frost
                        {',', nullptr, Color::Cyan, Color::Default},                     // , ice chip
                        {':', nullptr, Color::White, Color::Default},                     // : snow
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},             // · frozen gravel
                        {',', nullptr, Color::White, Color::Default},                    // , snow flake
                    };
                    vis = variants[seed % 5]; break;
                }
                case Biome::Crystal: {
                    // Crystal dust, shards, fragments, some rock
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", Color::Magenta, Color::Default},              // · crystal dust
                        {',', nullptr, Color::BrightMagenta, Color::Default},            // , shard
                        {':', nullptr, static_cast<Color>(54), Color::Default},           // : fragments
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},             // · mineral rock
                        {',', nullptr, Color::Magenta, Color::Default},                  // , crystal chip
                    };
                    vis = variants[seed % 5]; break;
                }
                default: {
                    // Generic: mix of sand, dirt, gravel
                    static const ResolvedVisual variants[] = {
                        {'.', "\xc2\xb7", Color::DarkGray, Color::Default},             // · gravel
                        {',', nullptr, static_cast<Color>(180), Color::Default},         // , sand
                        {':', nullptr, Color::DarkGray, Color::Default},                  // : stones
                        {'.', "\xc2\xb7", static_cast<Color>(94), Color::Default},     // · dirt
                    };
                    vis = variants[seed % 4]; break;
                }
            }
            break;
        }
        case FixtureType::SettlementProp: {
            static const ResolvedVisual props[] = {
                {'T', "\xe2\x94\xb4", Color::Cyan, Color::Default},         // ┴ antenna
                {'O', "\xc2\xb0", Color::Blue, Color::Default},              // ° water well
                {'=', "\xe2\x95\x90", Color::DarkGray, Color::Default},      // ═ bench
                {'*', "\xe2\x9c\xb6", Color::Yellow, Color::Default},        // ✶ lamp post
                {'%', "\xe2\x9a\x99", Color::DarkGray, Color::Default},      // ⚙ machinery
            };
            vis = props[seed % 5];
            break;
        }
        case FixtureType::CampStove:
            vis = {'o', "\xe2\x97\x8b", Color::Red, Color::Default}; break;             // ○ red stove
        case FixtureType::Lamp:
            vis = {'*', "\xe2\x9c\xb6", Color::Yellow, Color::Default}; break;          // ✶ warm lamp
        case FixtureType::HoloLight:
            vis = {'*', "\xe2\x9c\xb6", Color::Cyan, Color::Default}; break;            // ✶ holo light
        case FixtureType::Locker:
            vis = {'=', "\xe2\x96\xa0", Color::White, Color::Default}; break;            // ■ locker
        case FixtureType::BookCabinet:
            vis = {'[', "\xe2\x95\x94", static_cast<Color>(137), Color::Default}; break; // ╔ tan cabinet
        case FixtureType::DataTerminal:
            vis = {'#', "\xe2\x95\xac", Color::Cyan, Color::Default}; break;            // ╬ data terminal
        case FixtureType::Bench:
            vis = {'|', "\xe2\x95\x91", static_cast<Color>(137), Color::Default}; break; // ║ tan bench
        case FixtureType::Chair:
            vis = {'h', nullptr, Color::White, Color::Default}; break;
        case FixtureType::Gate:
            vis = {'/', nullptr, static_cast<Color>(137), Color::Default}; break;        // tan gate
        case FixtureType::BridgeRail:
            vis = {'|', "\xe2\x95\x8f", static_cast<Color>(137), Color::Default}; break; // ╏ tan rail
        case FixtureType::BridgeFloor:
            vis = {'.', "\xc2\xb7", static_cast<Color>(137), Color::Default}; break;     // · tan floor
        case FixtureType::Planter:
            vis = {'"', nullptr, Color::Green, Color::Default}; break;
        case FixtureType::FloraFlower: {
            switch (biome) {
                case Biome::Grassland: {
                    static const ResolvedVisual v[] = {
                        {'*', "\xc2\xb7", Color::Yellow, Color::Default},               // · yellow wildflower
                        {'*', "\xe2\x9c\xbf", Color::Red, Color::Default},              // ✿ red bloom
                        {'*', "\xe2\x9c\xb6", static_cast<Color>(208), Color::Default}, // ✶ orange flower
                        {'*', "\xe2\x80\xa2", Color::Magenta, Color::Default},           // • violet bud
                        {'*', "\xc2\xb7", Color::BrightYellow, Color::Default},          // · bright daisy
                        {'*', "\xe2\x9c\xbf", Color::Yellow, Color::Default},            // ✿ yellow bloom
                        {'*', "\xe2\x80\xa2", Color::Red, Color::Default},               // • red bud
                        {'*', "\xe2\x9c\xb6", Color::Cyan, Color::Default},              // ✶ blue flower
                    };
                    vis = v[seed % 8]; break;
                }
                case Biome::Jungle: {
                    static const ResolvedVisual v[] = {
                        {'*', "\xe2\x9c\xbf", Color::Yellow, Color::Default},
                        {'*', "\xe2\x80\xa2", Color::Red, Color::Default},
                        {'*', "\xe2\x9c\xb6", Color::Magenta, Color::Default},
                    };
                    vis = v[seed % 3]; break;
                }
                case Biome::Marsh: {
                    static const ResolvedVisual v[] = {
                        {'*', "\xc2\xb7", Color::Yellow, Color::Default},
                        {'*', "\xe2\x80\xa2", Color::Magenta, Color::Default},
                    };
                    vis = v[seed % 2]; break;
                }
                default:
                    vis = {'*', "\xe2\x9c\xbf", Color::Yellow, Color::Default}; break;
            }
            break;
        }
        case FixtureType::FloraHerb: {
            static const ResolvedVisual v[] = {
                {',', "\xcf\x84", Color::Green, Color::Default},                // τ grass tuft
                {',', "\xc6\x92", Color::Green, Color::Default},                // ƒ fern sprout
                {',', nullptr, static_cast<Color>(22), Color::Default},          // dark green
            };
            vis = v[seed % 3]; break;
        }
        case FixtureType::FloraMushroom: {
            switch (biome) {
                case Biome::Fungal: {
                    static const ResolvedVisual v[] = {
                        {'o', "\xce\xa6", Color::Green, Color::Default},         // Φ green mushroom
                        {'o', "\xce\xa6", Color::Magenta, Color::Default},       // Φ purple mushroom
                        {'o', "\xce\xa6", static_cast<Color>(22), Color::Default}, // Φ dark green
                    };
                    vis = v[seed % 3]; break;
                }
                default: {
                    static const ResolvedVisual v[] = {
                        {'o', "\xce\xa6", Color::DarkGray, Color::Default},      // Φ gray
                        {'o', "\xce\xa6", static_cast<Color>(94), Color::Default}, // Φ brown
                    };
                    vis = v[seed % 2]; break;
                }
            }
            break;
        }
        case FixtureType::FloraGrass: {
            switch (biome) {
                case Biome::Marsh: {
                    static const ResolvedVisual v[] = {
                        {'"', "\xcf\x84", static_cast<Color>(29), Color::Default}, // τ tall reeds
                        {'"', nullptr, static_cast<Color>(23), Color::Default},     // marsh grass
                    };
                    vis = v[seed % 2]; break;
                }
                default: {
                    static const ResolvedVisual v[] = {
                        {'"', nullptr, Color::Green, Color::Default},             // tall grass
                        {',', "\xcf\x84", Color::Green, Color::Default},          // τ grass tuft
                    };
                    vis = v[seed % 2]; break;
                }
            }
            break;
        }
        case FixtureType::FloraLichen: {
            static const ResolvedVisual v[] = {
                {'.', "\xc2\xb0", Color::DarkGray, Color::Default},             // ° gray lichen
                {'.', "\xc2\xb7", Color::Cyan, Color::Default},                 // · cyan moss
            };
            vis = v[seed % 2]; break;
        }
        case FixtureType::MineralOre: {
            switch (biome) {
                case Biome::Volcanic: {
                    static const ResolvedVisual v[] = {
                        {',', nullptr, static_cast<Color>(52), Color::Default},  // dark red slag
                        {',', nullptr, Color::Red, Color::Default},              // hot ore
                    };
                    vis = v[seed % 2]; break;
                }
                default: {
                    static const ResolvedVisual v[] = {
                        {',', nullptr, Color::DarkGray, Color::Default},         // gravel
                        {'`', nullptr, Color::White, Color::Default},            // quartz chip
                        {',', nullptr, static_cast<Color>(137), Color::Default}, // brown ore
                    };
                    vis = v[seed % 3]; break;
                }
            }
            break;
        }
        case FixtureType::MineralCrystal: {
            static const ResolvedVisual v[] = {
                {'*', "\xe2\x97\x87", Color::BrightMagenta, Color::Default},    // ◇ crystal
                {'*', "\xe2\x97\x86", Color::Magenta, Color::Default},          // ◆ dark crystal
                {'*', "\xe2\x97\x87", Color::Cyan, Color::Default},             // ◇ ice crystal
            };
            vis = v[seed % 3]; break;
        }
        case FixtureType::ScrapComponent: {
            static const ResolvedVisual v[] = {
                {'%', "\xe2\x9a\x99", Color::DarkGray, Color::Default},         // ⚙ wreckage
                {'%', nullptr, static_cast<Color>(208), Color::Default},         // orange scrap
                {';', nullptr, Color::DarkGray, Color::Default},                 // residue
            };
            vis = v[seed % 3]; break;
        }
        case FixtureType::QuestFixture:
            // Registry lookup requires FixtureData (quest_fixture_id string),
            // which RenderDescriptor doesn't carry. Render placeholder here;
            // richer resolution can be added when the descriptor is extended.
            vis = {'?', nullptr, Color::Magenta, Color::Default}; break;
    }

    if (remembered) {
        vis.fg = biome_palette(biome).remembered;
    }
    return vis;
}

// ---------------------------------------------------------------------------
// NPC resolution — NpcRole + Race → glyph + color
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_npc(uint16_t type_id, uint8_t seed, uint8_t /*flags*/) {
    auto role = static_cast<NpcRole>(type_id);

    switch (role) {
        case NpcRole::StationKeeper: return {'K', nullptr, Color::Green, Color::Default};
        case NpcRole::Merchant:      return {'M', nullptr, Color::Cyan, Color::Default};
        case NpcRole::Drifter:       return {'D', nullptr, Color::White, Color::Default};
        case NpcRole::Xytomorph:     return {'X', nullptr, Color::Red, Color::Default};
        case NpcRole::FoodMerchant:  return {'F', nullptr, Color::Yellow, Color::Default};
        case NpcRole::Medic:         return {'D', nullptr, Color::Green, Color::Default};
        case NpcRole::Commander:     return {'C', nullptr, Color::White, Color::Default};
        case NpcRole::ArmsDealer:    return {'A', nullptr, Color::Red, Color::Default};
        case NpcRole::Astronomer:    return {'P', nullptr, Color::Cyan, Color::Default};
        case NpcRole::Engineer:      return {'E', nullptr, Color::Yellow, Color::Default};
        case NpcRole::Nova:          return {'N', nullptr, static_cast<Color>(135), Color::Default};
        case NpcRole::Scavenger:     return {'S', nullptr, Color::Yellow, Color::Default};
        case NpcRole::Prospector:    return {'P', nullptr, Color::White, Color::Default};
        case NpcRole::ArchonRemnant: return {'R', nullptr, Color::Red, Color::Default};
        case NpcRole::VoidReaver:    return {'r', nullptr, Color::DarkGray, Color::Default};
        case NpcRole::Civilian: {
            auto race = static_cast<Race>(seed);
            switch (race) {
                case Race::Human:     return {'H', nullptr, Color::White, Color::Default};
                case Race::Veldrani:  return {'V', nullptr, Color::Cyan, Color::Default};
                case Race::Kreth:     return {'R', nullptr, Color::Yellow, Color::Default};
                case Race::Sylphari:  return {'S', nullptr, Color::Green, Color::Default};
                case Race::Stellari:  return {'L', nullptr, Color::Magenta, Color::Default};
                case Race::Xytomorph: return {'X', nullptr, Color::Red, Color::Default};
                default:              return {'H', nullptr, Color::White, Color::Default};
            }
        }
        default: return {'?', nullptr, Color::Magenta, Color::Default};
    }
}

// ---------------------------------------------------------------------------
// Item resolution — item_def_id → glyph + color
// ---------------------------------------------------------------------------

static ResolvedVisual resolve_item(uint16_t item_def_id) {
    switch (item_def_id) {
        // Ranged weapons (1-5)
        case ITEM_PLASMA_PISTOL:       return {')', nullptr, Color::Cyan, Color::Default};
        case ITEM_ION_BLASTER:         return {')', nullptr, Color::Green, Color::Default};
        case ITEM_PULSE_RIFLE:         return {')', nullptr, Color::Blue, Color::Default};
        case ITEM_ARC_CASTER:          return {')', nullptr, Color::Magenta, Color::Default};
        case ITEM_VOID_LANCE:          return {')', nullptr, static_cast<Color>(208), Color::Default};

        // Consumables (6-8)
        case ITEM_BATTERY:             return {'=', nullptr, Color::Yellow, Color::Default};
        case ITEM_RATION_PACK:         return {'%', nullptr, Color::Green, Color::Default};
        case ITEM_COMBAT_STIM:         return {'!', nullptr, Color::Red, Color::Default};

        // Melee weapons (9-13)
        case ITEM_COMBAT_KNIFE:        return {'/', nullptr, Color::White, Color::Default};
        case ITEM_VIBRO_BLADE:         return {'/', nullptr, Color::Green, Color::Default};
        case ITEM_PLASMA_SABER:        return {'/', nullptr, Color::Blue, Color::Default};
        case ITEM_STUN_BATON:          return {'/', nullptr, Color::Yellow, Color::Default};
        case ITEM_ANCIENT_MONO_EDGE:   return {'/', nullptr, Color::Magenta, Color::Default};

        // Armor — body (14-16)
        case ITEM_PADDED_VEST:         return {'[', nullptr, Color::White, Color::Default};
        case ITEM_COMPOSITE_ARMOR:     return {'[', nullptr, Color::Green, Color::Default};
        case ITEM_EXO_SUIT:            return {'[', nullptr, Color::Blue, Color::Default};

        // Armor — head (17-18)
        case ITEM_FLIGHT_HELMET:       return {'^', nullptr, Color::White, Color::Default};
        case ITEM_TACTICAL_HELMET:     return {'^', nullptr, Color::Green, Color::Default};

        // Armor — feet (19-20)
        case ITEM_COMBAT_BOOTS:        return {'_', nullptr, Color::White, Color::Default};
        case ITEM_MAG_LOCK_BOOTS:      return {'_', nullptr, Color::Green, Color::Default};

        // Armor — arm / shield (21-22)
        case ITEM_ARM_GUARD:           return {'}', nullptr, Color::White, Color::Default};
        case ITEM_RIOT_SHIELD:         return {'0', nullptr, Color::Green, Color::Default};

        // Accessories (23-26)
        case ITEM_RECON_VISOR:         return {'&', nullptr, Color::Green, Color::Default};
        case ITEM_NIGHT_GOGGLES:       return {'&', nullptr, Color::White, Color::Default};
        case ITEM_JETPACK:             return {'\\', nullptr, Color::Blue, Color::Default};
        case ITEM_CARGO_PACK:          return {'\\', nullptr, Color::White, Color::Default};

        // Grenades (27-29)
        case ITEM_FRAG_GRENADE:        return {'*', nullptr, Color::Red, Color::Default};
        case ITEM_EMP_GRENADE:         return {'*', nullptr, Color::Cyan, Color::Default};
        case ITEM_CRYO_GRENADE:        return {'*', nullptr, Color::Blue, Color::Default};

        // Junk (30-32)
        case ITEM_SCRAP_METAL:         return {'~', nullptr, Color::DarkGray, Color::Default};
        case ITEM_BROKEN_CIRCUIT:      return {'~', nullptr, Color::DarkGray, Color::Default};
        case ITEM_EMPTY_CASING:        return {'~', nullptr, Color::DarkGray, Color::Default};

        // Crafting materials (33-36)
        case ITEM_NANO_FIBER:          return {'+', nullptr, Color::Cyan, Color::Default};
        case ITEM_POWER_CORE:          return {'+', nullptr, Color::Yellow, Color::Default};
        case ITEM_CIRCUIT_BOARD:       return {'+', nullptr, Color::Green, Color::Default};
        case ITEM_ALLOY_INGOT:         return {'+', nullptr, Color::White, Color::Default};

        // Energy shields (41-46)
        case ITEM_BASIC_DEFLECTOR:     return {'0', nullptr, Color::White, Color::Default};
        case ITEM_PLASMA_SCREEN:       return {'0', nullptr, Color::Red, Color::Default};
        case ITEM_ION_BARRIER:         return {'0', nullptr, Color::Cyan, Color::Default};
        case ITEM_COMPOSITE_BARRIER:   return {'0', nullptr, Color::Green, Color::Default};
        case ITEM_HARDLIGHT_AEGIS:     return {'0', nullptr, Color::Yellow, Color::Default};
        case ITEM_VOID_MANTLE:         return {'0', nullptr, Color::Magenta, Color::Default};

        // Ship components (37-40)
        case ITEM_ENGINE_COIL_MK1:     return {'#', nullptr, Color::Yellow, Color::Default};
        case ITEM_HULL_PLATE:          return {'#', nullptr, Color::White, Color::Default};
        case ITEM_SHIELD_GENERATOR:    return {'#', nullptr, Color::Cyan, Color::Default};
        case ITEM_NAVI_COMPUTER_MK2:   return {'#', nullptr, Color::Green, Color::Default};

        // Synthesized items (1000+)
        case ITEM_SYNTH_PLASMA_EDGE:       return {'/', nullptr, Color::Cyan, Color::Default};
        case ITEM_SYNTH_THRUSTER_PLATE:    return {'[', nullptr, Color::Yellow, Color::Default};
        case ITEM_SYNTH_TARGETING_ARRAY:   return {'&', nullptr, Color::Cyan, Color::Default};
        case ITEM_SYNTH_DUAL_EDGE:         return {'/', nullptr, Color::BrightMagenta, Color::Default};
        case ITEM_SYNTH_REINFORCED_PACK:   return {'\\', nullptr, Color::Green, Color::Default};
        case ITEM_SYNTH_OVERCHARGED_ENGINE:return {'#', nullptr, static_cast<Color>(208), Color::Default};
        case ITEM_SYNTH_ARTICULATED_ARMOR: return {'[', nullptr, Color::Magenta, Color::Default};
        case ITEM_SYNTH_GUIDED_BLASTER:    return {')', nullptr, Color::Yellow, Color::Default};
        case ITEM_SYNTH_COMBAT_GAUNTLET:   return {'}', nullptr, Color::Red, Color::Default};
        case ITEM_SYNTH_ARMORED_BLADE:     return {'/', nullptr, Color::Red, Color::Default};

        default: return {'?', nullptr, Color::Magenta, Color::Default};
    }
}

// ---------------------------------------------------------------------------
// Main resolve — dispatches on RenderCategory
// ---------------------------------------------------------------------------

ResolvedVisual resolve(const RenderDescriptor& desc) {
    if (desc.category == RenderCategory::Fixture) {
        return resolve_fixture(desc.type_id, desc.flags, desc.biome, desc.seed);
    }

    if (desc.category == RenderCategory::Npc) {
        return resolve_npc(desc.type_id, desc.seed, desc.flags);
    }

    if (desc.category == RenderCategory::Item) {
        return resolve_item(desc.type_id);
    }

    if (desc.category == RenderCategory::Player) {
        return {'@', nullptr, Color::Yellow, Color::Default};
    }

    if (desc.category != RenderCategory::Tile) {
        return {'?', nullptr, Color::Magenta, Color::Default};
    }

    auto tile = static_cast<Tile>(desc.type_id);
    uint8_t seed = desc.seed;
    uint8_t flags = desc.flags;
    Biome biome = desc.biome;
    bool remembered = (flags & RF_Remembered) != 0;

    auto bc = biome_palette(biome);

    // --- Starfield (Station Empty tiles) ---
    if (flags & RF_Starfield) {
        return resolve_starfield(seed);
    }

    // --- Empty tile ---
    if (tile == Tile::Empty) {
        return {' ', nullptr, Color::Default, Color::Default};
    }

    // --- Overworld tiles (OW_*) ---
    if (tile >= Tile::OW_Plains && tile <= Tile::OW_Landing) {
        // OW_Ruins: seed carries neighbor bitmask for baroque pipe rendering
        if (tile == Tile::OW_Ruins) {
            static const char* baroque_conn[] = {
                "\xe2\x95\xac",  // 0000 isolated -> ╬
                "\xe2\x95\x91",  // 0001 N        -> ║
                "\xe2\x95\x91",  // 0010 S        -> ║
                "\xe2\x95\x91",  // 0011 NS       -> ║
                "\xe2\x95\x90",  // 0100 E        -> ═
                "\xe2\x95\x9a",  // 0101 NE       -> ╚
                "\xe2\x95\x94",  // 0110 SE       -> ╔
                "\xe2\x95\xa0",  // 0111 NSE      -> ╠
                "\xe2\x95\x90",  // 1000 W        -> ═
                "\xe2\x95\x9d",  // 1001 NW       -> ╝
                "\xe2\x95\x97",  // 1010 SW       -> ╗
                "\xe2\x95\xa3",  // 1011 NSW      -> ╣
                "\xe2\x95\x90",  // 1100 EW       -> ═
                "\xe2\x95\xa9",  // 1101 NEW      -> ╩
                "\xe2\x95\xa6",  // 1110 SEW      -> ╦
                "\xe2\x95\xac",  // 1111 NSEW     -> ╬
            };
            uint8_t nb = seed & 0x0F;
            const char* utf8 = baroque_conn[nb];
            // Isolated ruin tiles: vary glyph by position seed for visual variety
            if (nb == 0) {
                static const char* isolated_variants[] = {
                    "\xe2\x95\xac",  // ╬
                    "\xe2\x95\x91",  // ║
                    "\xe2\x95\x90",  // ═
                    "\xe2\x95\x94",  // ╔
                    "\xe2\x95\x97",  // ╗
                    "\xe2\x95\x9a",  // ╚
                    "\xe2\x95\x9d",  // ╝
                    "\xe2\x95\xa3",  // ╣
                    "\xe2\x95\xa0",  // ╠
                    "\xe2\x95\xa9",  // ╩
                    "\xe2\x95\xa6",  // ╦
                };
                // Use upper bits of seed for variant selection
                uint8_t variant = (seed >> 4) & 0x0F;
                utf8 = isolated_variants[variant % 11];
            }
            Color c = remembered ? bc.remembered : static_cast<Color>(15);
            return {'#', utf8, c, Color::Default};
        }

        Color c = ow_tile_color(tile, biome);
        const char* utf8 = ow_glyph(tile, seed);

        // Forest variant colors from top 2 bits of seed
        if (tile == Tile::OW_Forest) {
            uint8_t variant = (seed >> 6) & 0x03;
            switch (variant) {
                case 0: // Temperate — standard green
                    c = static_cast<Color>(34);
                    break;
                case 1: { // Autumn — red/orange/amber mix
                    static const Color autumn[] = {
                        static_cast<Color>(166),  // orange
                        static_cast<Color>(130),  // brown-red
                        static_cast<Color>(172),  // amber
                        static_cast<Color>(136),  // dark gold
                    };
                    c = autumn[seed % 4];
                    break;
                }
                case 2: // Conifer — dark blue-green
                    c = static_cast<Color>(23);
                    break;
                default:
                    break;
            }
        }

        // Alien terrain: architecture-specific color palette
        if (tile == Tile::OW_AlienTerrain) {
            switch (biome) {
                case Biome::AlienCrystalline: {
                    static const Color p[] = {Color::Cyan, static_cast<Color>(51), static_cast<Color>(37), Color::White};
                    c = p[seed % 4]; break;
                }
                case Biome::AlienOrganic: {
                    static const Color p[] = {Color::Red, Color::Magenta, static_cast<Color>(52), static_cast<Color>(88)};
                    c = p[seed % 4]; break;
                }
                case Biome::AlienGeometric: {
                    static const Color p[] = {Color::Yellow, static_cast<Color>(136), Color::White, static_cast<Color>(178)};
                    c = p[seed % 4]; break;
                }
                case Biome::AlienVoid: {
                    static const Color p[] = {static_cast<Color>(90), Color::DarkGray, Color::Magenta, static_cast<Color>(54)};
                    c = p[seed % 4]; break;
                }
                case Biome::AlienLight: {
                    static const Color p[] = {static_cast<Color>(228), Color::Yellow, Color::White, static_cast<Color>(230)};
                    c = p[seed % 4]; break;
                }
                default: {
                    static const Color p[] = {Color::BrightMagenta, Color::Magenta, static_cast<Color>(133), static_cast<Color>(170)};
                    c = p[seed % 4]; break;
                }
            }
        }
        // Scorched earth: orange/red variation
        if (tile == Tile::OW_ScorchedEarth) {
            static const Color scorch_palette[] = {
                static_cast<Color>(208),  // orange
                static_cast<Color>(202),  // bright orange-red
                static_cast<Color>(166),  // dark orange
                static_cast<Color>(130),  // brown-orange
            };
            c = scorch_palette[seed % 4];
        }
        // Glassed crater: dark amber/brown variation
        if (tile == Tile::OW_GlassedCrater) {
            static const Color glass_palette[] = {
                static_cast<Color>(136),  // dark yellow
                static_cast<Color>(94),   // brown
                static_cast<Color>(130),  // dark amber
                static_cast<Color>(172),  // golden brown
            };
            c = glass_palette[seed % 4];
        }

        return {tile_glyph(tile), utf8, c, Color::Default};
    }

    // --- Dungeon / station tiles ---

    if (tile == Tile::StructuralWall) {
        ResolvedVisual vis = resolve_structural_wall(seed);
        if (remembered) vis.fg = bc.remembered;
        return vis;
    }

    if (tile == Tile::Wall) {
        bool ruin_tint = (seed & 0x80) != 0;
        uint8_t clean_seed = seed & 0x7F;
        if (ruin_tint) {
            // Extract civ index from bits 4-6, neighbor mask from bits 0-3
            int civ = (clean_seed >> 4) & 0x07;
            uint8_t nb = clean_seed & 0x0F;  // N=1, S=2, E=4, W=8

            // Neighbor-aware pipe glyph lookup for double-line (baroque)
            // Index by neighbor bitmask: 0=none, 1=N, 2=S, 3=NS, 4=E, ...
            // Key connections: ║=NS, ═=EW, ╔=SE, ╗=SW, ╚=NE, ╝=NW, ╬=NSEW, etc.
            static const char* baroque_conn[] = {
                "\xe2\x96\xa0",  // 0000 isolated -> ■
                "\xe2\x95\x91",  // 0001 N        -> ║
                "\xe2\x95\x91",  // 0010 S        -> ║
                "\xe2\x95\x91",  // 0011 NS       -> ║
                "\xe2\x95\x90",  // 0100 E        -> ═
                "\xe2\x95\x9a",  // 0101 NE       -> ╚
                "\xe2\x95\x94",  // 0110 SE       -> ╔
                "\xe2\x95\xa0",  // 0111 NSE      -> ╠
                "\xe2\x95\x90",  // 1000 W        -> ═
                "\xe2\x95\x9d",  // 1001 NW       -> ╝
                "\xe2\x95\x97",  // 1010 SW       -> ╗
                "\xe2\x95\xa3",  // 1011 NSW      -> ╣
                "\xe2\x95\x90",  // 1100 EW       -> ═
                "\xe2\x95\xa9",  // 1101 NEW      -> ╩
                "\xe2\x95\xa6",  // 1110 SEW      -> ╦
                "\xe2\x95\xac",  // 1111 NSEW     -> ╬
            };

            // Single-line version (crystal)
            static const char* crystal_conn[] = {
                "\xc2\xb7",      // 0000 isolated -> ·
                "\xe2\x94\x82",  // 0001 N        -> │
                "\xe2\x94\x82",  // 0010 S        -> │
                "\xe2\x94\x82",  // 0011 NS       -> │
                "\xe2\x94\x80",  // 0100 E        -> ─
                "\xe2\x94\x94",  // 0101 NE       -> └
                "\xe2\x94\x8c",  // 0110 SE       -> ┌
                "\xe2\x94\x9c",  // 0111 NSE      -> ├
                "\xe2\x94\x80",  // 1000 W        -> ─
                "\xe2\x94\x98",  // 1001 NW       -> ┘
                "\xe2\x94\x90",  // 1010 SW       -> ┐
                "\xe2\x94\xa4",  // 1011 NSW      -> ┤
                "\xe2\x94\x80",  // 1100 EW       -> ─
                "\xe2\x94\xb4",  // 1101 NEW      -> ┴
                "\xe2\x94\xac",  // 1110 SEW      -> ┬
                "\xe2\x94\xbc",  // 1111 NSEW     -> ┼
            };

            // Block-based civs (monolithic, industrial) — no neighbor awareness
            static const char* monolithic_glyphs[] = {
                "\xe2\x96\x88", "\xe2\x96\x93", "\xe2\x96\x93",  // █ ▓ ▓
                "\xe2\x96\x92", "\xe2\x96\x88",                    // ▒ █
            };
            static const char* industrial_glyphs[] = {
                "\xe2\x96\x93", "\xe2\x96\x88", "\xe2\x96\x91",  // ▓ █ ░
                "\xe2\x8a\x9e", "\xe2\x8a\xa0",                    // ⊞ ⊠
            };

            const char* utf8;
            Color c_primary, c_secondary;
            // Interior tiles of thick walls → solid fill
            static const char* solid_fill = "\xe2\x96\x88";  // █

            switch (civ) {
                case 1:  // Baroque — connected double-line pipes
                    utf8 = (nb == 0x0F) ? solid_fill : baroque_conn[nb];
                    c_primary = static_cast<Color>(178);   // warm gold
                    c_secondary = static_cast<Color>(172);  // darker gold
                    break;
                case 2:  // Crystal — connected single-line pipes
                    utf8 = (nb == 0x0F) ? solid_fill : crystal_conn[nb];
                    c_primary = static_cast<Color>(51);    // bright cyan
                    c_secondary = static_cast<Color>(45);   // slightly dimmer cyan
                    break;
                case 3:  // Industrial — block elements
                    utf8 = industrial_glyphs[nb % 5];
                    c_primary = static_cast<Color>(166);   // corroded orange
                    c_secondary = static_cast<Color>(124);  // rust red
                    break;
                default: // Monolithic — block elements
                    utf8 = monolithic_glyphs[nb % 5];
                    c_primary = static_cast<Color>(250);   // bright stone
                    c_secondary = static_cast<Color>(245);  // mid gray
                    break;
            }

            Color c;
            if (remembered) {
                c = REMEMBERED_COLOR;
            } else {
                // Color variation: mostly primary, some secondary, rare mossy
                // Use position-based hash (not nb which is structural)
                unsigned color_var = static_cast<unsigned>(
                    (seed >> 4) * 7u + (seed & 0x0F) * 13u);
                if (civ == 0) {
                    // Monolithic: gray with some mossy/brown
                    switch (color_var % 6) {
                        case 0: case 1: case 2: c = c_primary; break;
                        case 3: case 4:         c = c_secondary; break;
                        case 5:                 c = static_cast<Color>(28); break;
                    }
                } else {
                    // Colored civs: stick to their palette, no mossy
                    c = (color_var % 3 == 0) ? c_secondary : c_primary;
                }
            }
            return {'#', utf8, c, Color::Default};
        }
        const char* utf8 = wall_glyph_for_biome(biome, clean_seed);
        Color c = remembered ? bc.remembered : bc.wall;
        return {'#', utf8, c, Color::Default};
    }

    if (tile == Tile::Portal) {
        const char* utf8 = "\xe2\x96\xbc";  // ▼
        Color c = remembered ? bc.remembered : Color::Yellow;
        return {'>', utf8, c, Color::Default};
    }

    if (tile == Tile::Water) {
        const char* utf8 = water_glyph(seed);
        Color c = remembered ? bc.remembered : bc.water;
        return {'~', utf8, c, Color::Default};
    }

    if (tile == Tile::Ice) {
        const char* utf8 = water_glyph(seed);
        Color c = remembered ? bc.remembered : static_cast<Color>(39);
        return {'~', utf8, c, Color::Default};
    }

    if (tile == Tile::IndoorFloor) {
        const char* utf8 = "\xe2\x96\xaa";  // ▪
        Color c = remembered ? bc.remembered : static_cast<Color>(137); // warm tan/brown
        return {'.', utf8, c, Color::Default};
    }

    if (tile == Tile::Path) {
        // Outdoor settlement path — packed dirt/stone, distinct from natural floor
        static const char* path_glyphs[] = {
            "\xc2\xb7",  // ·
            "\xe2\x80\xa2",  // •
        };
        const char* utf8 = path_glyphs[seed % 2];
        Color c = remembered ? bc.remembered : static_cast<Color>(180); // sandy/tan
        return {'.', utf8, c, Color::Default};
    }

    if (tile == Tile::Floor) {
        if (remembered) {
            return {'.', nullptr, bc.remembered, Color::Default};
        }
        return resolve_floor(seed, biome, bc.floor, bc.remembered);
    }

    // Fallback for unhandled tile types (e.g. Fixture — handled by a different category)
    return {'?', nullptr, Color::Magenta, Color::Default};
}

// ---------------------------------------------------------------------------
// Animation stub
// ---------------------------------------------------------------------------

ResolvedVisual resolve_animation(AnimationType type, int frame_index) {
    switch (type) {
        case AnimationType::ConsoleBlink: {
            static const ResolvedVisual frames[] = {
                {'#', nullptr, Color::Cyan, Color::Default},
                {'#', nullptr, Color::DarkGray, Color::Default},
            };
            return frames[frame_index % 2];
        }
        case AnimationType::WaterShimmer: {
            static const ResolvedVisual frames[] = {
                {'~', nullptr, Color::Blue, static_cast<Color>(17)},
                {'~', "\xe2\x89\x88", Color::Cyan, static_cast<Color>(18)},
                {'~', nullptr, Color::Blue, static_cast<Color>(17)},
            };
            return frames[frame_index % 3];
        }
        case AnimationType::LavaShimmer: {
            static const ResolvedVisual frames[] = {
                {'~', nullptr, Color::Red, Color::Default},
                {'~', "\xe2\x89\x88", Color::Yellow, Color::Default},
                {'~', nullptr, Color::Red, Color::Default},
            };
            return frames[frame_index % 3];
        }
        case AnimationType::ViewportShimmer: {
            static const ResolvedVisual frames[] = {
                {'"', nullptr, Color::Cyan, Color::Default},
                {'"', nullptr, Color::DarkGray, Color::Default},
            };
            return frames[frame_index % 2];
        }
        case AnimationType::TorchFlicker: {
            static const ResolvedVisual frames[] = {
                {'*', "\xe2\x9c\xb6", Color::Yellow, Color::Default},
                {'*', "\xe2\x9c\xb8", Color::Red, Color::Default},
                {'*', "\xe2\x9c\xba", Color::BrightYellow, Color::Default},
                {'*', "\xe2\x9c\xb7", Color::Yellow, Color::Default},
                {'*', "\xe2\x9c\xb9", Color::Red, Color::Default},
            };
            return frames[frame_index % 5];
        }
        case AnimationType::DamageFlash: {
            static const ResolvedVisual frames[] = {
                {'*', nullptr, Color::Red, Color::Default},
                {' ', nullptr, Color::Red, Color::Default},
            };
            return frames[frame_index % 2];
        }
        case AnimationType::HealPulse: {
            static const ResolvedVisual frames[] = {
                {'+', nullptr, Color::Green, Color::Default},
                {'+', nullptr, static_cast<Color>(10), Color::Default},
                {'+', nullptr, Color::Green, Color::Default},
            };
            return frames[frame_index % 3];
        }
        case AnimationType::Projectile:
            return {'*', nullptr, Color::Yellow, Color::Default};
        case AnimationType::LevelUp: {
            static const ResolvedVisual frames[] = {
                {'!', nullptr, Color::Yellow, Color::Default},
                {'!', nullptr, Color::BrightYellow, Color::Default},
                {'!', nullptr, Color::Yellow, Color::Default},
            };
            return frames[frame_index % 3];
        }
        case AnimationType::AlienPulse: {
            static const ResolvedVisual frames[] = {
                {'*', "\xe2\x9c\xa6", Color::Red, Color::Default},
                {'*', "\xe2\x9c\xa6", static_cast<Color>(52), Color::Default},
                {'*', "\xe2\x9c\xa6", Color::Magenta, Color::Default},
                {'*', "\xe2\x9c\xa6", static_cast<Color>(52), Color::Default},
            };
            return frames[frame_index % 4];
        }
        case AnimationType::ScarSmolder: {
            static const ResolvedVisual frames[] = {
                {'.', "\xc2\xb7", Color::DarkGray, Color::Default},
                {'.', "\xc2\xb7", static_cast<Color>(208), Color::Default},
                {'.', "\xc2\xb7", Color::DarkGray, Color::Default},
            };
            return frames[frame_index % 3];
        }
        case AnimationType::BeaconGlow: {
            static const ResolvedVisual frames[] = {
                {'*', "\xe2\x8c\xbe", Color::Cyan, Color::Default},
                {'*', "\xe2\x8c\xbe", static_cast<Color>(51), Color::Default},
                {'*', "\xe2\x8c\xbe", Color::White, Color::Default},
                {'*', "\xe2\x8c\xbe", static_cast<Color>(51), Color::Default},
                {'*', "\xe2\x8c\xbe", Color::Cyan, Color::Default},
                {'*', "\xe2\x8c\xbe", static_cast<Color>(37), Color::Default},
            };
            return frames[frame_index % 6];
        }
        case AnimationType::MegastructureShift: {
            static const ResolvedVisual frames[] = {
                {'#', "\xe2\x97\x88", Color::Yellow, Color::Default},
                {'#', "\xe2\x97\x88", static_cast<Color>(136), Color::Default},
                {'#', "\xe2\x96\xa3", Color::Yellow, Color::Default},
                {'#', "\xe2\x96\xa3", static_cast<Color>(136), Color::Default},
            };
            return frames[frame_index % 4];
        }
    }
    return {'?', nullptr, Color::Magenta, Color::Default};
}

// ---------------------------------------------------------------------------
// Fixture glyph — ASCII glyph for UI / editor palette display
// ---------------------------------------------------------------------------

char fixture_glyph(FixtureType type) {
    switch (type) {
        case FixtureType::Door:            return '+';
        case FixtureType::Window:          return 'O';
        case FixtureType::Table:           return 'o';
        case FixtureType::Console:         return '#';
        case FixtureType::Crate:           return '=';
        case FixtureType::Bunk:            return '=';
        case FixtureType::Rack:            return '|';
        case FixtureType::Conduit:         return '%';
        case FixtureType::ShuttleClamp:    return '=';
        case FixtureType::Shelf:           return '[';
        case FixtureType::Viewport:        return '"';
        case FixtureType::Torch:           return '*';
        case FixtureType::Stool:           return 'o';
        case FixtureType::Debris:          return ',';
        case FixtureType::HealPod:         return '+';
        case FixtureType::FoodTerminal:    return '$';
        case FixtureType::WeaponDisplay:   return '/';
        case FixtureType::RepairBench:     return '%';
        case FixtureType::SupplyLocker:    return '&';
        case FixtureType::StarChart:       return '*';
        case FixtureType::RestPod:         return '=';
        case FixtureType::ShipTerminal:    return '>';
        case FixtureType::CommandTerminal: return '#';
        case FixtureType::DungeonHatch:    return 'v';
        case FixtureType::StairsUp:        return '<';
        case FixtureType::NaturalObstacle: return 'o';
        case FixtureType::ShoreDebris:     return '.';
        case FixtureType::SettlementProp:  return '*';
        case FixtureType::CampStove:       return 'o';
        case FixtureType::Lamp:            return '*';
        case FixtureType::HoloLight:       return '*';
        case FixtureType::Locker:          return '=';
        case FixtureType::BookCabinet:     return '[';
        case FixtureType::DataTerminal:    return '#';
        case FixtureType::Bench:           return '|';
        case FixtureType::Chair:           return 'h';
        case FixtureType::Gate:            return '/';
        case FixtureType::BridgeRail:      return '|';
        case FixtureType::BridgeFloor:     return '.';
        case FixtureType::Planter:         return '"';
        case FixtureType::FloraFlower:     return '*';
        case FixtureType::FloraHerb:       return ',';
        case FixtureType::FloraMushroom:   return 'o';
        case FixtureType::FloraGrass:      return '"';
        case FixtureType::FloraLichen:     return '.';
        case FixtureType::MineralOre:      return ',';
        case FixtureType::MineralCrystal:  return '*';
        case FixtureType::ScrapComponent:  return '%';
        case FixtureType::QuestFixture:    return '?';
    }
    return '?';
}

char quest_fixture_glyph(const std::string& id) {
    if (const auto* def = find_quest_fixture(id)) return def->glyph;
    return '?';
}

int quest_fixture_color(const std::string& id, int fallback) {
    if (const auto* def = find_quest_fixture(id)) return def->color;
    return fallback;
}

char npc_glyph(NpcRole role, Race race) {
    switch (role) {
        case NpcRole::StationKeeper: return 'K';
        case NpcRole::Merchant:      return 'M';
        case NpcRole::Drifter:       return 'D';
        case NpcRole::Xytomorph:     return 'X';
        case NpcRole::FoodMerchant:  return 'F';
        case NpcRole::Medic:         return 'D';
        case NpcRole::Commander:     return 'C';
        case NpcRole::ArmsDealer:    return 'A';
        case NpcRole::Astronomer:    return 'P';
        case NpcRole::Engineer:      return 'E';
        case NpcRole::Nova:          return 'N';
        case NpcRole::Scavenger:     return 'S';
        case NpcRole::Prospector:    return 'P';
        case NpcRole::Civilian: {
            switch (race) {
                case Race::Human:     return 'H';
                case Race::Veldrani:  return 'V';
                case Race::Kreth:     return 'R';
                case Race::Sylphari:  return 'S';
                case Race::Stellari:  return 'L';
                case Race::Xytomorph: return 'X';
                default:              return 'H';
            }
        }
        default: return '?';
    }
}

// ---------------------------------------------------------------------------
// Public item visual — for UI screens (inventory, shops, etc.)
// ---------------------------------------------------------------------------

ResolvedVisual item_visual(uint16_t item_def_id) {
    return resolve_item(item_def_id);
}

} // namespace astra

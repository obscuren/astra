// src/terminal_theme.cpp
#include "terminal_theme.h"
#include "astra/render_descriptor.h"

namespace astra {

// ---------------------------------------------------------------------------
// Biome palette — mirrors biome_colors() in tilemap.cpp
// ---------------------------------------------------------------------------

ThemeBiomeColors biome_palette(Biome biome) {
    switch (biome) {
        case Biome::Station:
            return {Color::White, Color::Default, Color::Blue, Color::Blue};
        case Biome::Rocky:
            return {Color::White, Color::DarkGray, Color::Blue, Color::Blue};
        case Biome::Volcanic:
            return {Color::Red, static_cast<Color>(52), Color::Red, static_cast<Color>(52)};
        case Biome::Ice:
            return {Color::Cyan, Color::White, static_cast<Color>(39), Color::Blue};
        case Biome::Sandy:
            return {Color::Yellow, static_cast<Color>(180), Color::Blue, static_cast<Color>(58)};
        case Biome::Aquatic:
            return {static_cast<Color>(30), static_cast<Color>(24), Color::Blue, static_cast<Color>(24)};
        case Biome::Fungal:
            return {Color::Green, static_cast<Color>(22), Color::Green, static_cast<Color>(22)};
        case Biome::Crystal:
            return {Color::BrightMagenta, Color::Magenta, Color::Magenta, static_cast<Color>(54)};
        case Biome::Corroded:
            return {static_cast<Color>(142), static_cast<Color>(58), static_cast<Color>(148), static_cast<Color>(58)};
        case Biome::Forest:
            return {Color::Green, static_cast<Color>(58), Color::Blue, static_cast<Color>(22)};
        case Biome::Grassland:
            return {Color::DarkGray, Color::Green, Color::Blue, static_cast<Color>(22)};
        case Biome::Jungle:
            return {static_cast<Color>(22), static_cast<Color>(22), static_cast<Color>(30), static_cast<Color>(22)};
    }
    return {Color::White, Color::Default, Color::Blue, Color::Blue};
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
        case Tile::OW_Mountains:   return Color::White;
        case Tile::OW_Crater:      return Color::DarkGray;
        case Tile::OW_IceField:    return Color::Cyan;
        case Tile::OW_LavaFlow:    return Color::Red;
        case Tile::OW_Desert:      return Color::Yellow;
        case Tile::OW_Fungal:      return Color::Green;
        case Tile::OW_Forest:      return Color::Green;
        case Tile::OW_River:       return Color::Blue;
        case Tile::OW_Lake:        return Color::Cyan;
        case Tile::OW_Swamp:       return static_cast<Color>(58);
        case Tile::OW_CaveEntrance:return Color::Magenta;
        case Tile::OW_Ruins:       return Color::BrightMagenta;
        case Tile::OW_Settlement:  return Color::Yellow;
        case Tile::OW_CrashedShip: return Color::Cyan;
        case Tile::OW_Outpost:     return Color::Green;
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
                "\xe2\x99\xa0",  // ♠
                "\xce\xa6",      // Φ
                "\xc6\x92",      // ƒ
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
                "\xcf\x84",      // τ
                "\"",
                ",",
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
            static const char* g[] = {
                "\xcf\x80",      // π
                "\xce\xa9",      // Ω
                "\xc2\xa7",      // §
                "\xce\xa3",      // Σ
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Settlement:  return "\xe2\x99\xa6"; // ♦
        case Tile::OW_CrashedShip: {
            static const char* g[] = {
                "%",
                "\xc2\xa4",      // ¤
            };
            return select_variant(g, seed);
        }
        case Tile::OW_Outpost:     return "+";
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

    struct ScatterSet { int threshold; const char* glyphs; int count; };
    ScatterSet s;
    switch (biome) {
        case Biome::Rocky:    s = {15, ",:`",  3}; break;
        case Biome::Volcanic: s = {20, ",';" , 3}; break;
        case Biome::Ice:      s = {12, "'`,",  3}; break;
        case Biome::Sandy:    s = {20, ",`:",  3}; break;
        case Biome::Aquatic:  s = {10, ",:",   2}; break;
        case Biome::Fungal:   s = {18, "\",'", 3}; break;
        case Biome::Crystal:  s = {15, "*'`",  3}; break;
        case Biome::Corroded: s = {20, ",:;",  3}; break;
        case Biome::Forest:   s = {18, "\",'", 3}; break;
        case Biome::Grassland:s = {15, ",`.",  3}; break;
        case Biome::Jungle:   s = {22, "\",'", 3}; break;
        default: return {'.', nullptr, floor_color, Color::Default};
    }

    if (roll >= s.threshold) {
        return {'.', nullptr, floor_color, Color::Default};
    }
    char scatter = s.glyphs[(seed >> 4) % s.count];
    return {scatter, nullptr, remembered_color, Color::Default}; // dimmer shade for scatter
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

static ResolvedVisual resolve_fixture(uint16_t type_id, uint8_t flags, Biome biome) {
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
    }

    if (remembered) {
        vis.fg = biome_palette(biome).remembered;
    }
    return vis;
}

// ---------------------------------------------------------------------------
// Main resolve — dispatches on RenderCategory
// ---------------------------------------------------------------------------

ResolvedVisual resolve(const RenderDescriptor& desc) {
    if (desc.category == RenderCategory::Fixture) {
        return resolve_fixture(desc.type_id, desc.flags, desc.biome);
    }

    if (desc.category != RenderCategory::Tile) {
        // Stub for non-tile categories
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
        Color c = ow_tile_color(tile, biome);
        const char* utf8 = ow_glyph(tile, seed);
        return {tile_glyph(tile), utf8, c, Color::Default};
    }

    // --- Dungeon / station tiles ---

    if (tile == Tile::StructuralWall) {
        ResolvedVisual vis = resolve_structural_wall(seed);
        if (remembered) vis.fg = bc.remembered;
        return vis;
    }

    if (tile == Tile::Wall) {
        const char* utf8 = wall_glyph_for_biome(biome, seed);
        Color c = remembered ? bc.remembered : bc.wall;
        return {'#', utf8, c, Color::Default};
    }

    if (tile == Tile::Portal) {
        const char* utf8 = "\xe2\x96\xbc";  // ▼
        Color c = remembered ? bc.remembered : Color::Magenta;
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
    (void)type;
    (void)frame_index;
    // Stub — returns bright magenta '*' so unresolved animations are obvious.
    return {'*', nullptr, Color::Magenta, Color::Default};
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
    }
    return '?';
}

} // namespace astra

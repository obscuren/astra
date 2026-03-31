#pragma once

#include "astra/npc.h"       // NpcRole
#include "astra/race.h"      // Race
#include "astra/renderer.h"  // Color
#include "astra/tilemap.h"   // Biome, Tile

namespace astra {

struct RenderDescriptor;
enum class AnimationType : uint8_t;

struct ResolvedVisual {
    char glyph         = '?';
    const char* utf8   = nullptr;  // nullptr = use glyph
    Color fg           = Color::Magenta;
    Color bg           = Color::Default;
};

struct ThemeBiomeColors {
    Color wall;
    Color floor;
    Color water;
    Color remembered;
};

ThemeBiomeColors biome_palette(Biome biome);

// Resolve a game-world descriptor to terminal visuals.
// Returns fallback '?' / Magenta for unhandled categories (stub).
ResolvedVisual resolve(const RenderDescriptor& desc);

// Resolve an animation frame to terminal visuals.
// Returns fallback '*' / Magenta for unhandled types (stub).
ResolvedVisual resolve_animation(AnimationType type, int frame_index);

// Return the ASCII glyph for a fixture type (for UI / editor palette display).
char fixture_glyph(FixtureType type);

// Return the ASCII glyph for an NPC role (for UI display).
char npc_glyph(NpcRole role, Race race = Race::Human);

// Return the resolved visual for an item definition (for inventory/shop UI).
ResolvedVisual item_visual(uint16_t item_def_id);

} // namespace astra

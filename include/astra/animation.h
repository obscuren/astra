#pragma once

#include "astra/renderer.h"
#include "astra/tilemap.h"
#include "astra/visibility_map.h"

#include <chrono>
#include <unordered_map>
#include <vector>

namespace astra {

struct AnimationFrame {
    char glyph = ' ';
    const char* utf8 = nullptr;  // null = use char glyph
    Color color = Color::White;
    int duration_ms = 150;
};

struct AnimationDef {
    std::vector<AnimationFrame> frames;
    bool looping = false;
};

struct ActiveAnimation {
    int x, y;                    // world coordinates
    const AnimationDef* def;
    int current_frame = 0;
    int elapsed_ms = 0;
    int delay_ms = 0;            // countdown before animation starts
    bool finished = false;
};

class AnimationManager {
public:
    // Advance all animations using wall-clock delta. Call every frame.
    void tick();

    // Query animation override at world position. Returns null if none.
    const AnimationFrame* query(int mx, int my) const;

    // Query only effect animations (not fixtures) — for player/NPC override
    const AnimationFrame* query_effect(int mx, int my) const;

    // Spawn a one-shot effect at a position
    void spawn_effect(const AnimationDef& def, int x, int y);

    // Spawn a one-shot effect along a Bresenham line (staggered delays)
    void spawn_effect_line(const AnimationDef& def, int x0, int y0, int x1, int y1);

    // Rebuild fixture animations from visible map tiles
    void spawn_fixture_anims(const TileMap& map, const VisibilityMap& vis);

    // True if any transient effects are active
    bool has_active_effects() const;

    // True if any animations exist (fixtures or effects) — drives timeout in main loop
    bool has_any() const;

    // Clear all animations (call on map transitions)
    void clear();

private:
    std::vector<ActiveAnimation> fixture_anims_;
    std::vector<ActiveAnimation> effect_anims_;
    // Spatial index for O(1) lookup by position
    std::unordered_map<uint64_t, int> fixture_index_;  // pos_key → index in fixture_anims_
    std::unordered_map<uint64_t, int> effect_index_;   // pos_key → index in effect_anims_
    void rebuild_indices();
    static uint64_t pos_key(int x, int y) { return (static_cast<uint64_t>(static_cast<uint32_t>(x)) << 32) | static_cast<uint32_t>(y); }

    std::chrono::steady_clock::time_point last_tick_ = std::chrono::steady_clock::now();

    void advance(ActiveAnimation& a, int delta_ms);
};

// Built-in animation definitions
extern const AnimationDef anim_console_blink;
extern const AnimationDef anim_water_shimmer;
extern const AnimationDef anim_viewport_shimmer;
extern const AnimationDef anim_damage_flash;
extern const AnimationDef anim_heal_pulse;
extern const AnimationDef anim_projectile;
extern const AnimationDef anim_level_up;
extern const AnimationDef anim_torch_flicker;

} // namespace astra

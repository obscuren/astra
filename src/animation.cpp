#include "astra/animation.h"

#include <algorithm>
#include <cmath>

namespace astra {

// ─────────────────────────────────────────────────────────────────
// Static animation definitions
// ─────────────────────────────────────────────────────────────────

const AnimationDef anim_console_blink = {
    AnimationType::ConsoleBlink,
    {{500}, {500}},
    true
};

const AnimationDef anim_water_shimmer = {
    AnimationType::WaterShimmer,
    {{400}, {400}, {400}},
    true
};

const AnimationDef anim_viewport_shimmer = {
    AnimationType::ViewportShimmer,
    {{800}, {800}},
    true
};

const AnimationDef anim_damage_flash = {
    AnimationType::DamageFlash,
    {{100}, {100}},
    false
};

const AnimationDef anim_heal_pulse = {
    AnimationType::HealPulse,
    {{120}, {120}, {120}},
    false
};

const AnimationDef anim_projectile = {
    AnimationType::Projectile,
    {{80}},
    false
};

const AnimationDef anim_torch_flicker = {
    AnimationType::TorchFlicker,
    {{180}, {140}, {200}, {150}, {170}},
    true
};

const AnimationDef anim_level_up = {
    AnimationType::LevelUp,
    {{150}, {150}, {150}},
    false
};

const AnimationDef anim_alien_pulse = {
    AnimationType::AlienPulse,
    {{300}, {300}, {300}, {300}},
    true
};

const AnimationDef anim_scar_smolder = {
    AnimationType::ScarSmolder,
    {{400}, {200}, {400}},
    true
};

const AnimationDef anim_beacon_glow = {
    AnimationType::BeaconGlow,
    {{300}, {300}, {300}, {300}, {300}, {300}},
    true
};

const AnimationDef anim_megastructure_shift = {
    AnimationType::MegastructureShift,
    {{500}, {500}, {500}, {500}},
    true
};

// ─────────────────────────────────────────────────────────────────
// AnimationManager
// ─────────────────────────────────────────────────────────────────

void AnimationManager::advance(ActiveAnimation& a, int delta_ms) {
    if (a.finished) return;
    if (a.def->frames.empty()) { a.finished = true; return; }

    // Handle delay countdown (for staggered projectiles)
    if (a.delay_ms > 0) {
        a.delay_ms -= delta_ms;
        if (a.delay_ms > 0) return;
        delta_ms = -a.delay_ms; // carry over excess
        a.delay_ms = 0;
    }

    a.elapsed_ms += delta_ms;
    int frame_dur = a.def->frames[a.current_frame].duration_ms;

    while (a.elapsed_ms >= frame_dur) {
        a.elapsed_ms -= frame_dur;
        a.current_frame++;

        if (a.current_frame >= static_cast<int>(a.def->frames.size())) {
            if (a.def->looping) {
                a.current_frame = 0;
            } else {
                a.finished = true;
                return;
            }
        }
        frame_dur = a.def->frames[a.current_frame].duration_ms;
    }
}

void AnimationManager::rebuild_indices() {
    effect_index_.clear();
    for (int i = 0; i < static_cast<int>(effect_anims_.size()); ++i) {
        auto& a = effect_anims_[i];
        if (!a.finished && a.delay_ms <= 0)
            effect_index_[pos_key(a.x, a.y)] = i;
    }
    // fixture_index_ is rebuilt in spawn_fixture_anims, not here
}

void AnimationManager::tick() {
    auto now = std::chrono::steady_clock::now();
    int delta_ms = static_cast<int>(
        std::chrono::duration_cast<std::chrono::milliseconds>(now - last_tick_).count());
    last_tick_ = now;

    // Clamp delta to avoid huge jumps after blocking input,
    // but always allow at least one animation frame worth of time
    if (delta_ms > 200) delta_ms = 80;

    for (auto& a : fixture_anims_) advance(a, delta_ms);
    for (auto& a : effect_anims_) advance(a, delta_ms);

    // Remove finished effects and rebuild index
    auto old_size = effect_anims_.size();
    effect_anims_.erase(
        std::remove_if(effect_anims_.begin(), effect_anims_.end(),
                        [](const ActiveAnimation& a) { return a.finished; }),
        effect_anims_.end());
    if (effect_anims_.size() != old_size || !effect_anims_.empty()) {
        rebuild_indices();
    }
}

std::optional<AnimQueryResult> AnimationManager::query(int mx, int my) const {
    uint64_t key = pos_key(mx, my);
    // Effects take priority over fixtures
    auto eit = effect_index_.find(key);
    if (eit != effect_index_.end()) {
        const auto& a = effect_anims_[eit->second];
        if (!a.finished && a.delay_ms <= 0)
            return AnimQueryResult{a.def->type, a.current_frame};
    }
    auto fit = fixture_index_.find(key);
    if (fit != fixture_index_.end()) {
        const auto& a = fixture_anims_[fit->second];
        return AnimQueryResult{a.def->type, a.current_frame};
    }
    return std::nullopt;
}

std::optional<AnimQueryResult> AnimationManager::query_effect(int mx, int my) const {
    auto eit = effect_index_.find(pos_key(mx, my));
    if (eit != effect_index_.end()) {
        const auto& a = effect_anims_[eit->second];
        if (!a.finished && a.delay_ms <= 0)
            return AnimQueryResult{a.def->type, a.current_frame};
    }
    return std::nullopt;
}

void AnimationManager::spawn_effect(const AnimationDef& def, int x, int y) {
    if (effect_anims_.empty()) {
        last_tick_ = std::chrono::steady_clock::now();
    }
    int idx = static_cast<int>(effect_anims_.size());
    effect_anims_.push_back({x, y, &def, 0, 0, 0, false});
    effect_index_[pos_key(x, y)] = idx;
}

void AnimationManager::spawn_effect_line(const AnimationDef& def,
                                          int x0, int y0, int x1, int y1) {
    // Bresenham line — spawn one animation per cell with staggered delay
    int dx = std::abs(x1 - x0);
    int dy = std::abs(y1 - y0);
    int sx = (x0 < x1) ? 1 : -1;
    int sy = (y0 < y1) ? 1 : -1;
    int err = dx - dy;
    int cx = x0, cy = y0;
    int step = 0;

    while (true) {
        // Skip the first cell (shooter position)
        if (step > 0) {
            ActiveAnimation a;
            a.x = cx;
            a.y = cy;
            a.def = &def;
            a.delay_ms = step * 60; // stagger 60ms per cell
            effect_anims_.push_back(a);
        }

        if (cx == x1 && cy == y1) break;

        int e2 = 2 * err;
        if (e2 > -dy) { err -= dy; cx += sx; }
        if (e2 < dx)  { err += dx; cy += sy; }
        ++step;
    }
}

static const AnimationDef* fixture_anim_for(FixtureType type) {
    switch (type) {
        case FixtureType::Console:         return &anim_console_blink;
        case FixtureType::CommandTerminal: return &anim_megastructure_shift;
        case FixtureType::Viewport:        return &anim_viewport_shimmer;
        case FixtureType::Torch:           return &anim_torch_flicker;
        default: return nullptr;
    }
}

void AnimationManager::spawn_fixture_anims(const TileMap& map, const VisibilityMap& vis) {
    // Only add animations for tiles that don't already have one
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            if (vis.get(x, y) == Visibility::Unexplored) continue;
            uint64_t key = pos_key(x, y);
            if (fixture_index_.count(key)) continue;

            const AnimationDef* def = nullptr;

            if (map.get(x, y) == Tile::Fixture) {
                int fid = map.fixture_id(x, y);
                if (fid >= 0) {
                    def = fixture_anim_for(map.fixture(fid).type);
                }
            } else if (map.get(x, y) == Tile::Water ||
                       map.get(x, y) == Tile::OW_River ||
                       map.get(x, y) == Tile::OW_Lake ||
                       map.get(x, y) == Tile::OW_Swamp) {
                def = &anim_water_shimmer;
            } else if (map.get(x, y) == Tile::OW_LavaFlow) {
                def = &anim_torch_flicker;
            } else if (map.get(x, y) == Tile::Portal) {
                def = &anim_beacon_glow;
            }

            if (def) {
                unsigned h = static_cast<unsigned>(x) * 374761393u
                           + static_cast<unsigned>(y) * 668265263u;
                int phase_offset = static_cast<int>(h % 1000);

                int idx = static_cast<int>(fixture_anims_.size());
                ActiveAnimation a;
                a.x = x;
                a.y = y;
                a.def = def;
                a.elapsed_ms = phase_offset;
                fixture_anims_.push_back(a);
                fixture_index_[key] = idx;
            }
        }
    }
}

bool AnimationManager::has_active_effects() const {
    return !effect_anims_.empty();
}

bool AnimationManager::has_any() const {
    return !effect_anims_.empty() || !fixture_anims_.empty();
}

void AnimationManager::clear() {
    fixture_anims_.clear();
    effect_anims_.clear();
    fixture_index_.clear();
    effect_index_.clear();
}

} // namespace astra

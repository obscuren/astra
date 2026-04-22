#pragma once

namespace astra::world {

// --- Overworld dimensions ---
constexpr int overworld_width  = 128;
constexpr int overworld_height = 128;

// --- Detail map dimensions ---
constexpr int detail_map_width  = 80;
constexpr int detail_map_height = 50;

// --- Lore influence tuning ---
constexpr int landmark_zone_radius    = 3;     // 7x7 tile reserved zone
constexpr int min_scar_radius         = 8;
constexpr int max_scar_radius         = 20;
constexpr float terraform_min_coverage = 0.3f;
constexpr float terraform_max_coverage = 0.6f;
constexpr int terraform_min_origins    = 2;
constexpr int terraform_max_origins    = 5;

// --- Influence thresholds ---
constexpr float alien_strength_threshold = 0.15f;  // below this = no alien effect
constexpr float alien_full_replace       = 0.6f;   // above this = full alien tile
constexpr float scar_light_threshold     = 0.1f;   // scarred ground
constexpr float scar_medium_threshold    = 0.4f;   // scorched earth
constexpr float scar_heavy_threshold     = 0.7f;   // glassed/crater

// --- Camp Making ---
constexpr int campfire_lifetime_ticks     = 150;  // world ticks until campfire expires
constexpr int cozy_radius                 = 6;    // Chebyshev tiles from a player campfire
constexpr int camp_making_cooldown_ticks  = 300;  // ability cooldown
constexpr int camp_making_action_cost     = 100;  // action cost to build camp

}  // namespace astra::world

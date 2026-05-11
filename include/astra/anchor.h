#pragma once

#include <cstdint>

namespace astra {

class GridSector;     // forward decl
class WorldManager;   // forward decl

struct Imprint {
    int32_t  id          = -1;     // assigned on spawn (GridSession::next_anchor_id_)
    int      x           = 0;      // Site coords (mirror of NPC's RW position)
    int      y           = 0;
    int      hp          = 0;
    int      max_hp      = 0;
    int      npc_id      = -1;     // index into world's NPC list
    bool     bound       = false;  // true if projected via Bind on a no-Crystal target
    bool     identified  = false;  // becomes true after `look` with Crystal-Decoder unlock
    bool     xp_granted  = false;  // ensures sever-XP is paid only once per Imprint

    bool severed() const { return hp <= 0; }
    float vulnerability_pct() const {
        if (max_hp <= 0) return 0.0f;
        return 1.0f - static_cast<float>(hp) / static_cast<float>(max_hp);
    }
};

// Linear-scaled projection from RW (real-world) tile coords into Site
// (in-Grid) tile coords. Used to mirror NPC positions onto Anchors.
struct ImprintProjection {
    int site_w       = 0;    // Site width in tiles
    int site_h       = 0;    // Site height in tiles
    int rw_origin_x  = 0;    // RW origin (top-left) of the region the LAN covers
    int rw_origin_y  = 0;
    int rw_extent_x  = 60;   // RW range in tiles covered by the projection
    int rw_extent_y  = 50;
};

// Build a projection for the given Site against the world's current map.
// Defaults to using the active map's dimensions for the RW extent.
ImprintProjection make_imprint_projection(const GridSector& sec,
                                        const WorldManager& world);

// Project (rwx, rwy) into (sx, sy) inside the Site, clamped to valid
// range. Mutates sx and sy.
void project_rw_to_site(const ImprintProjection& proj,
                        int rwx, int rwy, int& sx, int& sy);

// If (sx, sy) is unwalkable in `sector`, BFS-Chebyshev to the nearest
// passable cell within `max_radius`. Mutates sx and sy. Returns true if
// (sx, sy) was already passable or a passable neighbour was found; false
// if no walkable cell exists within radius (caller decides what to do).
bool nudge_to_passable(const GridSector& sector, int& sx, int& sy,
                       int max_radius = 4);

}  // namespace astra

#pragma once

#include <cstdint>
#include <string>

namespace astra {

class Game;
struct Player;
struct Npc;
class WorldManager;

enum class TrapKind : uint8_t {
    ProximityMine = 0,
    EmpMine,
    IncendiaryMine,
    DecoyMine,
    Caltrops,
    DungeonGeneric,
};

enum class TrapTrigger : uint8_t {
    NonFriendlyToOwner = 0,
    AnyEntity,
    PlayerOnly,
};

struct Trap {
    TrapKind kind = TrapKind::ProximityMine;
    int x = 0;
    int y = 0;

    // Visibility / detection
    bool hidden = true;
    int reveal_radius = 2;             // Chebyshev
    int detection_dc = 12;
    bool was_in_player_radius = false; // debounce flag for detection roll

    // Trigger logic
    TrapTrigger trigger_mode = TrapTrigger::NonFriendlyToOwner;
    std::string owner_faction;         // "" if player- or dungeon-placed
    bool placer_is_player = false;
    int placer_npc_id = -1;            // for NPC-placed traps

    // State
    int activations_remaining = 1;
    int placed_tick = 0;
};

// Display helpers
const char* trap_kind_name(TrapKind k);
char trap_glyph(TrapKind k);
int  trap_color(TrapKind k);

// Per-kind trap stats — used by both the runtime resolver and UI panels.
struct TrapDef {
    int damage = 0;
    int burst_radius = 0;            // 0 = single tile only
    int status = 0;                  // EffectId cast — 0 means "no status"
    int status_duration = 0;
    int status_tick_damage = 0;
};
const TrapDef& trap_def_for(TrapKind k);

// How far a player-thrown trap of this kind can reach, and the half-width
// of its placement burst reticule (caltrops scatters across a 3x3).
int trap_throw_range(TrapKind k);
int trap_throw_burst_width(TrapKind k);

// Player deploy — spawn-only. Caller decrements the source stack
// (Thrown equipment slot or inventory entry) after the call.
void place_player_trap(Game& game, TrapKind kind, int dest_x, int dest_y);

// Dungeon-placement helper — used by future generators
void place_dungeon_trap(WorldManager& wm, int x, int y, TrapKind kind,
                        TrapTrigger trigger = TrapTrigger::AnyEntity,
                        bool hidden = true,
                        int detection_dc = 14);

// Per-tick / per-event hooks
void on_entity_enters_tile(Game& game, int x, int y, bool is_player, int npc_id);
void update_trap_detection(Game& game);

// Item id ↔ TrapKind mapping (used by use_item dispatch)
TrapKind trap_kind_for_item_id(uint16_t item_id);

} // namespace astra

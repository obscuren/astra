#pragma once

#include "astra/aura.h"
#include "astra/dice.h"
#include "astra/effect.h"
#include "astra/hackable.h"
#include "astra/interaction.h"
#include "astra/race.h"
#include "astra/vulnerability.h"

#include <cstdint>
#include <optional>
#include <random>
#include <string>
#include <vector>

namespace astra {

enum class NpcAi : uint8_t {
    Melee,    // adjacency attacks only (default)
    Turret,   // ranged attack in range+LOS, otherwise hold position
    Kiter,    // reserved: ranged attack in range+LOS, otherwise close the gap
};

// NPC templates define the archetype; the factory fills in name/race.
enum class NpcRole : uint8_t {
    StationKeeper,
    Merchant,
    Drifter,
    Xytomorph,
    FoodMerchant,
    Medic,
    Commander,
    ArmsDealer,
    Astronomer,
    Engineer,
    Nova,
    Civilian,
    Scavenger,
    Prospector,
    ArchonRemnant,
    VoidReaver,
    ArchonSentinel,
    ConclaveSentry,
    HeavyConclaveSentry,
    RustHound,
    SentryDrone,
    ArchonAutomaton,
    ConclaveSentryDrone,
    ArchonSentryDrone,
};

struct Npc {
    int32_t uid = -1;             // stable monotonic ID assigned on spawn (NEVER reused).
                                  // Use this for cross-system linkage (Anchors,
                                  // saved references) instead of the vector index.
    int x = 0;
    int y = 0;
    std::string name;           // personal name, e.g. "Krath"
    std::string role;           // title, e.g. "Station Keeper"
    Race race = Race::Human;
    int hp = 1;
    int max_hp = 1;
    std::string faction;        // faction name (empty = unaligned)
    EffectList effects;
    std::vector<Aura> auras;
    int quickness = 100;
    int energy = 0;
    int level = 1;
    bool elite = false;
    int base_xp = 0;
    int base_damage = 0;
    int dv = 8;
    int av = 0;
    Dice damage_dice;
    DamageType damage_type = DamageType::Kinetic;
    // Ranged attack (empty ranged_damage_dice disables ranged path)
    int attack_range = 1;              // chebyshev tiles; 1 = melee only
    Dice ranged_damage_dice;           // empty by default
    DamageType ranged_damage_type = DamageType::Kinetic;
    NpcAi ai = NpcAi::Melee;
    TypeAffinity type_affinity;
    NpcRole npc_role = NpcRole::Civilian;
    uint64_t flags = 0;         // CreatureFlag bitfield (Mechanical, Biological, ...)
    InteractionData interactions;
    std::optional<Hackable> cyber;       // present iff this NPC is hackable
    std::string pre_hijack_faction;      // restored when Hijacked or TurretAllied effect expires

    // Runtime vulnerability/DoT tracking
    VulnerabilityStack vuln;

    // When displaced by player swap, NPC tries to return here next tick
    int return_x = -1;
    int return_y = -1;

    // Noise-event chase target (e.g. Decoy Mine ping). When ttl > 0 and
    // the NPC has no hostile in sight, it walks toward (move_target_x,
    // move_target_y). Decremented each NPC turn; cleared on arrival.
    int move_target_x = -1;
    int move_target_y = -1;
    int move_target_ttl = 0;

    bool alive() const { return hp > 0; }
    int xp_reward() const { return base_xp * level * (elite ? 3 : 1); }
    int attack_damage() const { return base_damage * level + (elite ? 1 : 0); }
    void scale_to_level(int lvl, bool is_elite);
    std::string label() const;
};

// Create a fully configured NPC. Race is used for name generation.
// For hostile types the race is implicit (e.g. Xytomorph).
Npc create_npc(NpcRole role, Race race, std::mt19937& rng);

// Generate a personal name appropriate for a given race.
std::string generate_name(Race race, std::mt19937& rng);

// Create an NPC from a role name string (e.g. "Xytomorph", "Station Keeper").
Npc create_npc_by_role(const std::string& role_name, std::mt19937& rng);

} // namespace astra

#pragma once

#include "astra/effect.h"
#include "astra/tilemap.h"    // FixtureType, FixtureTag, FixtureData, fixture_has_tag
#include "astra/skill_defs.h" // SkillId

#include <cstdint>
#include <vector>

namespace astra {

// Source of a player/NPC aura. Drives surgical removal when the
// originating system changes state (item unequip, effect expire, etc.).
enum class AuraSource : uint8_t {
    Manual  = 0,  // dev console / scripting; survives save-load
    Item,         // source_id = Item::id
    Effect,       // source_id = static_cast<uint32_t>(EffectId)
    Skill,        // source_id = static_cast<uint32_t>(SkillId)
    Fixture,      // not stored on entities; reserved for clarity
};

// Receiver classes — bitflags; combine with |.
namespace AuraTarget {
    constexpr uint32_t Player      = 1u << 0;
    constexpr uint32_t FriendlyNpc = 1u << 1;
    constexpr uint32_t HostileNpc  = 1u << 2;
    constexpr uint32_t AllNpcs     = FriendlyNpc | HostileNpc;
    constexpr uint32_t Everyone    = Player | AllNpcs;
}

// Runtime aura — pure data. Stored on entities (Player/Npc) and
// materialised for fixtures via auras_for().
struct Aura {
    Effect     template_effect;                       // duration baked in
    int        radius       = 1;                      // Chebyshev
    uint32_t   target_mask  = AuraTarget::Player;
    AuraSource source       = AuraSource::Manual;
    uint32_t   source_id    = 0;
};

// Fixture aura registry — union of tag-derived and type-specific.
std::vector<Aura> auras_for(const FixtureData& fd);

// Skill aura registry — called by rebuild_auras_from_sources.
std::vector<Aura> skill_auras(SkillId id);

struct Player;
struct Npc;

// Wipe all non-Manual entries from entity.auras and re-populate from
// the entity's sources (items / effects / skills). Implementations
// land in later tasks; for now these are stubs.
void rebuild_auras_from_sources(Player& p);
void rebuild_auras_from_sources(Npc& n);

} // namespace astra

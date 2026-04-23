#include "astra/aura.h"

#include "astra/effect.h"
#include "astra/item.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/world_constants.h"

#include <algorithm>

namespace astra {

// ── Fixture aura registries ────────────────────────────────────────

// Tag-driven: any fixture whose `tags` includes this FixtureTag emits
// the listed aura. Keep this list small and generic — prefer type
// auras for anything fixture-specific.
static const std::vector<std::pair<FixtureTag, Aura>>& tag_auras() {
    static const std::vector<std::pair<FixtureTag, Aura>> table = {
        // populated by later tasks
    };
    return table;
}

// Type-specific: exact FixtureType → auras. Use this when a fixture
// emits something its tag class shouldn't universally emit (Cozy is
// campfire-only, not HeatSource-wide).
static const std::vector<std::pair<FixtureType, std::vector<Aura>>>& type_auras() {
    static const std::vector<std::pair<FixtureType, std::vector<Aura>>> table = [] {
        std::vector<std::pair<FixtureType, std::vector<Aura>>> t;

        // Campfire → Cozy. Deliberately fixture-type specific (not
        // HeatSource-wide): only the player's own campfires grant Cozy,
        // not every heat source in the game world.
        Aura cozy;
        cozy.template_effect = make_cozy_ge();   // duration=1 baked in
        cozy.radius          = astra::world::cozy_radius;
        cozy.target_mask     = AuraTarget::Player;
        cozy.source          = AuraSource::Fixture;
        cozy.source_id       = 0;
        t.push_back({FixtureType::Campfire, {cozy}});

        return t;
    }();
    return table;
}

std::vector<Aura> auras_for(const FixtureData& fd) {
    std::vector<Aura> out;
    for (const auto& [tag, aura] : tag_auras()) {
        if (fixture_has_tag(fd, tag)) out.push_back(aura);
    }
    for (const auto& [type, list] : type_auras()) {
        if (fd.type == type) {
            for (const auto& a : list) out.push_back(a);
        }
    }
    return out;
}

// ── Skill aura registry ────────────────────────────────────────────

std::vector<Aura> skill_auras(SkillId /*id*/) {
    // No skill-sourced auras yet.
    return {};
}

// ── Rebuild from sources ───────────────────────────────────────────
// Stubs until Task 6 wires item/effect/skill contributions.

void rebuild_auras_from_sources(Player& /*p*/) {}
void rebuild_auras_from_sources(Npc& /*n*/) {}

} // namespace astra

#include "astra/aura.h"

#include "astra/effect.h"
#include "astra/item.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/world_constants.h"

#include <algorithm>
#include <optional>

namespace astra {

// ── Fixture aura registries ────────────────────────────────────────

// Tag-driven: any fixture whose `tags` includes this FixtureTag emits
// the listed aura. Keep this list small and generic — prefer type
// auras for anything fixture-specific.
static const std::vector<std::pair<FixtureTag, Aura>>& tag_auras() {
    static const std::vector<std::pair<FixtureTag, Aura>> table = [] {
        std::vector<std::pair<FixtureTag, Aura>> t;

        // CookingSource → CookingFireAura. Any fixture tagged
        // CookingSource (Campfire, CampStove, Kitchen, …) lets the
        // player cook when in range.
        Aura cooking;
        cooking.template_effect = make_cooking_fire_aura_ge();
        cooking.radius          = astra::world::cooking_source_radius;
        cooking.target_mask     = AuraTarget::Player;
        cooking.source          = AuraSource::Fixture;
        cooking.source_id       = 0;
        t.push_back({FixtureTag::CookingSource, cooking});

        return t;
    }();
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

namespace {

Aura materialise_grant(const AuraGrant& g, AuraSource source, uint32_t source_id) {
    Aura a;
    a.template_effect = g.make_effect ? g.make_effect() : Effect{};
    a.radius          = g.radius;
    a.target_mask     = g.target_mask;
    a.source          = source;
    a.source_id       = source_id;
    return a;
}

void strip_non_manual(std::vector<Aura>& v) {
    v.erase(std::remove_if(v.begin(), v.end(),
              [](const Aura& a) { return a.source != AuraSource::Manual; }),
            v.end());
}

} // anonymous

void rebuild_auras_from_sources(Player& p) {
    strip_non_manual(p.auras);

    // Items — equipment slots
    auto add_item_grants = [&](const std::optional<Item>& it) {
        if (!it) return;
        for (const auto& g : it->granted_auras) {
            p.auras.push_back(materialise_grant(g, AuraSource::Item, it->id));
        }
    };
    add_item_grants(p.equipment.face);
    add_item_grants(p.equipment.head);
    add_item_grants(p.equipment.body);
    add_item_grants(p.equipment.left_arm);
    add_item_grants(p.equipment.right_arm);
    add_item_grants(p.equipment.left_hand);
    add_item_grants(p.equipment.right_hand);
    add_item_grants(p.equipment.back);
    add_item_grants(p.equipment.feet);
    add_item_grants(p.equipment.thrown);
    add_item_grants(p.equipment.missile);
    add_item_grants(p.equipment.shield);

    // Effects
    for (const auto& e : p.effects) {
        for (const auto& g : e.granted_auras) {
            p.auras.push_back(materialise_grant(g,
                                                AuraSource::Effect,
                                                static_cast<uint32_t>(e.id)));
        }
    }

    // Skills
    for (SkillId sid : p.learned_skills) {
        for (const Aura& sa : skill_auras(sid)) {
            Aura copy = sa;
            copy.source    = AuraSource::Skill;
            copy.source_id = static_cast<uint32_t>(sid);
            p.auras.push_back(std::move(copy));
        }
    }
}

void rebuild_auras_from_sources(Npc& n) {
    strip_non_manual(n.auras);

    // NPCs carry no equipment or skills in the current model; only
    // effect-sourced auras.
    for (const auto& e : n.effects) {
        for (const auto& g : e.granted_auras) {
            n.auras.push_back(materialise_grant(g,
                                                AuraSource::Effect,
                                                static_cast<uint32_t>(e.id)));
        }
    }
}

} // namespace astra

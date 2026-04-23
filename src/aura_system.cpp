#include "astra/aura_system.h"

#include "astra/aura.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/npc.h"
#include "astra/player.h"
#include "astra/tilemap.h"

#include <algorithm>
#include <vector>

namespace astra {

namespace {

bool receiver_matches_mask(uint32_t mask,
                           bool is_player,
                           bool is_hostile) {
    if (is_player) return (mask & AuraTarget::Player) != 0;
    if (is_hostile) return (mask & AuraTarget::HostileNpc) != 0;
    return (mask & AuraTarget::FriendlyNpc) != 0;
}

// Apply all of `auras` emitted from (ex, ey) to in-range receivers.
// `self_npc` (if not null) is skipped to prevent self-application from
// NPC emitters; the player self-exclusion is handled at the call site.
void apply_auras_at(const std::vector<Aura>& auras,
                    int ex, int ey,
                    Game& game,
                    bool emitter_is_player,
                    Npc* self_npc) {
    if (auras.empty()) return;

    auto& player = game.player();
    auto& npcs   = game.world().npcs();

    for (const Aura& a : auras) {
        const int r = a.radius;
        if (r <= 0) continue;

        // Player receiver (skip if emitter is the player)
        if (!emitter_is_player) {
            int dx = std::abs(player.x - ex);
            int dy = std::abs(player.y - ey);
            if (std::max(dx, dy) <= r
                && receiver_matches_mask(a.target_mask, /*is_player*/true, /*hostile*/false)) {
                add_effect(player.effects, a.template_effect);
            }
        }

        // NPC receivers
        for (auto& npc : npcs) {
            if (!npc.alive()) continue;
            if (&npc == self_npc) continue;
            int dx = std::abs(npc.x - ex);
            int dy = std::abs(npc.y - ey);
            if (std::max(dx, dy) > r) continue;
            bool hostile = is_hostile_to_player(npc.faction, player);
            if (!receiver_matches_mask(a.target_mask, /*is_player*/false, hostile)) continue;
            add_effect(npc.effects, a.template_effect);
        }
    }
}

} // anonymous

void AuraSystem::tick(Game& game) {
    auto& map = game.world().map();

    // 1) Fixture emitters — iterate the map once, look up auras per fixture.
    for (int y = 0; y < map.height(); ++y) {
        for (int x = 0; x < map.width(); ++x) {
            int fid = map.fixture_id(x, y);
            if (fid < 0) continue;
            const auto& fd = map.fixture(fid);
            auto auras = auras_for(fd);
            apply_auras_at(auras, x, y, game,
                           /*emitter_is_player*/false, /*self_npc*/nullptr);
        }
    }

    // 2) Player emitter
    auto& player = game.player();
    apply_auras_at(player.auras, player.x, player.y, game,
                   /*emitter_is_player*/true, /*self_npc*/nullptr);

    // 3) NPC emitters
    for (auto& npc : game.world().npcs()) {
        if (!npc.alive()) continue;
        apply_auras_at(npc.auras, npc.x, npc.y, game,
                       /*emitter_is_player*/false, /*self_npc*/&npc);
    }
}

} // namespace astra

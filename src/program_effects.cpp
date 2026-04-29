#include "astra/program_effects.h"

#include "astra/effect.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/npc.h"
#include "astra/world_manager.h"

#include <string>

namespace astra {

namespace {

void apply_reboot_optics(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    target.state = HackState::Compromised;
    target.state_ticks_left = 4;
    game.log("The " + std::string(device_kind_name(target.device_kind)) +
             " judders and flickers offline.");
}

void apply_friendly_fire(Game& game, Hackable& target, int tx, int ty) {
    if (target.device_kind != DeviceKind::Turret) {
        game.log("Friendly Fire only works on turrets.");
        return;
    }
    target.state = HackState::Compromised;
    target.state_ticks_left = 2;
    for (auto& npc : game.world().npcs()) {
        if (npc.x == tx && npc.y == ty && npc.alive()) {
            if (npc.pre_hijack_faction.empty()) {
                npc.pre_hijack_faction = npc.faction;
            }
            npc.faction = "Hijacked";
            add_effect(npc.effects, make_hijacked_ge(2));
            game.log("The turret rotates onto its allies.");
            return;
        }
    }
    game.log("The turret's targeting matrix scrambles, but no allies are in range.");
}

void apply_data_leech(Game& game, Hackable& target, int /*tx*/, int /*ty*/) {
    target.state = HackState::Compromised;
    target.state_ticks_left = 1;
    int credits = 5 + (target.security_tier * 5);
    game.player().money += credits;
    game.log("Data leeched: +" + std::to_string(credits) +
             " credits skimmed off the bus.");
}

} // namespace

void apply_program_effect(ProgramId id, Game& game, Hackable& target, int tx, int ty) {
    switch (id) {
        case ProgramId::RebootOptics: apply_reboot_optics(game, target, tx, ty); break;
        case ProgramId::FriendlyFire: apply_friendly_fire(game, target, tx, ty); break;
        case ProgramId::DataLeech:    apply_data_leech(game, target, tx, ty); break;
        default: break;
    }
}

} // namespace astra

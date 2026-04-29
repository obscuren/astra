#include "astra/hacking_system.h"

#include "astra/cyberdeck.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/item.h"
#include "astra/program.h"
#include "astra/program_effects.h"   // Task 9 will populate; Task 7 ships a stub
#include "astra/world_manager.h"

#include <algorithm>

namespace astra {

namespace {
constexpr int kDetectionDecayInterval = 5;   // tick every N world steps, -1 to value
constexpr int kDetectionMax = 100;
constexpr int kDetectionMin = 0;
} // namespace

void HackingSystem::add_detection(int delta) {
    detection_.value = std::clamp(detection_.value + delta, kDetectionMin, kDetectionMax);
}

void HackingSystem::reset_zone() {
    detection_.value = 0;
    detection_.decay_acc = 0;
}

uint64_t HackingSystem::compute_zone_signature(const Game& game) {
    // Hash navigation state + zone coords. The exact composition is internal
    // — we only need a stable uint64_t that changes when the player enters a
    // distinct zone.
    const auto& nav = game.world().navigation();
    uint64_t s = nav.current_system_id;
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_body_index + 2);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_moon_index + 2);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.at_station ? 1 : 0);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.on_ship ? 1 : 0);
    s = (s * 31u) ^ static_cast<uint64_t>(nav.current_depth + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(game.world().zone_x() + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(game.world().zone_y() + 1);
    s = (s * 31u) ^ static_cast<uint64_t>(static_cast<int>(game.world().surface_mode()));
    return s;
}

void HackingSystem::tick(Game& game) {
    uint64_t sig = compute_zone_signature(game);
    if (sig != last_zone_signature_) {
        last_zone_signature_ = sig;
        reset_zone();
        return;
    }
    if (detection_.value <= kDetectionMin) return;
    if (++detection_.decay_acc >= kDetectionDecayInterval) {
        detection_.decay_acc = 0;
        detection_.value = std::max(kDetectionMin, detection_.value - 1);
    }
}

void HackingSystem::begin_quickhack_targeting(Game& /*game*/) {
    // Full implementation lands in Task 8.
    targeting_ = true;
    blink_phase_ = 0;
}

void HackingSystem::handle_targeting_input(int /*key*/, Game& /*game*/) {
    // Implemented in Task 8.
}

std::string HackingSystem::execute_quickhack(Game& game, const Item& program,
                                             Hackable& target, int tx, int ty) {
    if (!program.program) return "Not a program.";
    const ProgramDef* def = find_program(program.program->id);
    if (!def) return "Unknown program.";
    if (def->kind != ProgramKind::Qh)
        return "Only .qh programs can be fired in the real world.";

    bool ok = std::any_of(def->target_filter.begin(), def->target_filter.end(),
                          [&](DeviceKind k){ return k == target.device_kind; });
    if (!ok) {
        return std::string("Program rejects ") + device_kind_name(target.device_kind) + ".";
    }

    auto& deck_slot = game.player().equipment.cyberdeck;
    if (!deck_slot || !deck_slot->deck) return "No cyberdeck equipped.";
    auto& deck = *deck_slot->deck;
    if (deck.ram_current < def->ram_cost) {
        return "Not enough RAM (" + std::to_string(deck.ram_current) + "/" +
               std::to_string(def->ram_cost) + ").";
    }
    deck.ram_current -= def->ram_cost;

    add_detection(def->detection_cost);

    apply_program_effect(def->id, game, target, tx, ty);

    target.state = HackState::Compromised;
    return std::string(def->name) + " executed.";
}

} // namespace astra

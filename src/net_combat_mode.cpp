#include "astra/net_combat_mode.h"

#include "astra/net_ice_telegraph.h"
#include "astra/net_pipe_path.h"
#include "astra/net_voice.h"

#include <algorithm>
#include <string>

namespace astra {

namespace {

bool ice_is_threat(const Ice& ic) {
    return ic.hp > 0
        && ic.charmed_turns_left == 0
        && ic.color != IceColor::White;
}

bool in_room(const NetRoom& r, int x, int y) {
    return x >= r.x && x < r.x + r.w && y >= r.y && y < r.y + r.h;
}

}  // namespace

bool combat_should_lock(const NetSession& s) {
    // S7c.1 followup: avatar must be STRICTLY inside a room (not in a
    // pipe / on a wall) for combat-lock to engage. Mirrors the
    // engagement gate in ice_cast_tick.
    if (room_index_at_strict(s.netspace,
                             s.avatar_x, s.avatar_y) < 0) return false;
    auto conn = connected_pipe_indices(s.netspace, s.avatar_x, s.avatar_y);
    for (int idx : conn) {
        auto path = pipe_path_cells(s.netspace, idx,
                                    s.avatar_x, s.avatar_y);
        if (path.empty()) continue;
        int ri = room_index_at(s.netspace,
                               path.back().first, path.back().second);
        if (ri < 0 || ri >= static_cast<int>(s.netspace.rooms.size()))
            continue;
        const NetRoom& far = s.netspace.rooms[static_cast<size_t>(ri)];
        for (const auto& ic : s.ice)
            if (ice_is_threat(ic) && in_room(far, ic.x, ic.y))
                return true;
    }
    return false;
}

bool update_combat_lock(NetSession& s) {
    const bool want = s.combat_manual || combat_should_lock(s);
    const auto prev = s.combat_mode;
    s.combat_mode = want ? NetSession::NetCombatMode::Combat
                         : NetSession::NetCombatMode::Normal;
    if (s.combat_mode == prev) return false;
    if (s.combat_mode == NetSession::NetCombatMode::Combat) {
        s.field_caption = "\xe2\x9a\xa0 COMBAT \xe2\x80\x94 node locked";
        s.push_log(astra::net_voice::cmd("combat lock engaged."));
    } else {
        s.field_caption.clear();
        s.push_log(astra::net_voice::cmd("combat clear."));
    }
    return true;
}

void core_action_perform(NetSession& s, int idx) {
    if (idx < 0 || idx > 3) return;
    switch (s.core_actions[static_cast<size_t>(idx)]) {
        case NetCoreAction::Sniff: {
            // Sticky ladder: ++sniff_level capped at kSniffMax.
            const int prev = s.sniff_level;
            if (s.sniff_level < kSniffMax) ++s.sniff_level;
            const int now = s.sniff_level;
            if (now > prev) {
                // Per-tier reveal log line so the player learns the
                // ladder by playing it.
                std::string newly;
                if (now == 1)
                    newly = "payload dmg + cast bars + Black proximity exposed.";
                else if (now == 2)
                    newly = "ICE names + HP + precise Black ETA exposed.";
                s.push_log(astra::net_voice::cmd(
                    "sniff: tier " + std::to_string(now) + " -- " + newly));
            } else {
                s.push_log(astra::net_voice::cmd(
                    "sniff: max tier (already wide-open)."));
            }
            break;
        }
        case NetCoreAction::Channel:
            s.ram = std::min(s.ram_max, s.ram + 2);
            s.push_log(astra::net_voice::cmd("channel: +RAM."));
            break;
        case NetCoreAction::Brace:
            s.brace_turns = 1;
            s.push_log(astra::net_voice::cmd("brace."));
            break;
        case NetCoreAction::Run:
            // RUN is Game-touching and dispatched separately via
            // core_action_run() from net_input.cpp. This branch should
            // not be reached -- assert-style no-op log if it ever is.
            s.push_log(astra::net_voice::cmd(
                "autopilot: routed (RUN dispatch sentinel)."));
            break;
        default: break;
    }
}

}  // namespace astra

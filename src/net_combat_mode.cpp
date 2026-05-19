#include "astra/net_combat_mode.h"

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
            int live = 0;
            for (auto& ic : s.ice) if (ic.hp > 0) ++live;
            s.push_log(astra::net_voice::cmd(
                "sniff: " + std::to_string(live) + " ICE, "
                + std::to_string(static_cast<int>(s.in_flight.size()))
                + " in flight."));
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
            s.push_log(astra::net_voice::cmd("autopilot."));
            break;
        default: break;
    }
}

}  // namespace astra

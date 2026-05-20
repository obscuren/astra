#include "astra/net_ice_telegraph.h"

#include "astra/net_combat.h"        // pipe_graph_next_hop
#include "astra/net_ice.h"
#include "astra/net_pipe_path.h"     // room_index_at
#include "astra/net_session.h"

namespace astra {

bool sniff_show(IceTelegraphTier tier, int sniff_level, RevealKind k) {
    // Future tiers (Elite/Boss/Blackwall) plug in via additional cases.
    // S6 ships only Watchdog; the others hide everything as a defined
    // seam (renderer asks, gets a stable no, no UB).
    if (tier != IceTelegraphTier::Watchdog) return false;

    switch (k) {
        // T1 (sniff_level >= 1) opens:
        case RevealKind::PayloadDmg:      return sniff_level >= 1;
        case RevealKind::IceCastBar:      return sniff_level >= 1;
        case RevealKind::BlackEtaCoarse:  return sniff_level >= 1;
        // T2 (sniff_level >= 2) adds:
        case RevealKind::IceCastName:     return sniff_level >= 2;
        case RevealKind::IceHp:           return sniff_level >= 2;
        case RevealKind::BlackEtaPrecise: return sniff_level >= 2;
    }
    return false;
}

namespace {
// Given a pipe index and a room, return the OTHER room the pipe
// connects. -1 if pipe doesn't touch room.
int other_room_of_pipe(const Netspace& ns, int pipe_idx, int room) {
    if (pipe_idx < 0 ||
        pipe_idx >= static_cast<int>(ns.pipes.size())) return -1;
    const auto& p = ns.pipes[static_cast<std::size_t>(pipe_idx)];
    int ra = room_index_at(ns, p.x0, p.y0);
    int rb = room_index_at(ns, p.x1, p.y1);
    if (ra == room) return rb;
    if (rb == room) return ra;
    return -1;
}
}  // namespace

int black_eta_beats(const NetSession& s, const Ice& blk) {
    if (blk.color != IceColor::Black) return -1;
    if (blk.hp <= 0 || blk.charmed_turns_left != 0) return -1;

    const int avatar_room =
        room_index_at(s.netspace, s.avatar_x, s.avatar_y);
    if (avatar_room < 0) return -1;

    int eta = 0;
    int cur_room;

    if (blk.walk_pipe_index >= 0) {
        // In transit: cells remaining on current pipe.
        const int remaining =
            static_cast<int>(blk.walk_path.size()) - blk.walk_seg;
        if (remaining < 0) return -1;
        eta += remaining;
        // After this pipe, Black is in the room at walk_path.back().
        if (blk.walk_path.empty()) return -1;
        const auto& last = blk.walk_path.back();
        cur_room = room_index_at(s.netspace, last.first, last.second);
    } else {
        cur_room = room_index_at(s.netspace, blk.x, blk.y);
    }
    if (cur_room < 0) return -1;
    if (cur_room == avatar_room) return eta;

    // BFS-walk the remaining hops, summing each pipe's cell count.
    // Cap iterations at rooms.size() to guarantee termination on any
    // path topology (BFS shortest-path can't exceed total rooms).
    const int max_hops = static_cast<int>(s.netspace.rooms.size());
    for (int hop = 0; hop < max_hops; ++hop) {
        if (cur_room == avatar_room) return eta;
        int next_pipe =
            pipe_graph_next_hop(s.netspace, cur_room, avatar_room);
        if (next_pipe < 0) return -1;       // no path
        const auto& p =
            s.netspace.pipes[static_cast<std::size_t>(next_pipe)];
        eta += static_cast<int>(p.cells.size());
        int next_room = other_room_of_pipe(s.netspace, next_pipe, cur_room);
        if (next_room < 0) return -1;
        cur_room = next_room;
    }
    return (cur_room == avatar_room) ? eta : -1;
}

}  // namespace astra

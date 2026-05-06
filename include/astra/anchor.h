#pragma once

#include <cstdint>

namespace astra {

struct Anchor {
    int32_t  id          = -1;     // assigned on spawn (GridSession::next_anchor_id_)
    int      x           = 0;      // Site coords (mirror of NPC's RW position)
    int      y           = 0;
    int      hp          = 0;
    int      max_hp      = 0;
    int      npc_id      = -1;     // index into world's NPC list
    bool     bound       = false;  // true if projected via Bind on a no-Crystal target
    bool     identified  = false;  // becomes true after `look` with Crystal-Decoder unlock
    bool     xp_granted  = false;  // ensures sever-XP is paid only once per Anchor

    bool severed() const { return hp <= 0; }
    float vulnerability_pct() const {
        if (max_hp <= 0) return 0.0f;
        return 1.0f - static_cast<float>(hp) / static_cast<float>(max_hp);
    }
};

}  // namespace astra

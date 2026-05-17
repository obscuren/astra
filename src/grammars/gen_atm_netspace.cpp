#include "astra/grammars/gen_atm_netspace.h"

#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

// Canvas dimensions — sized to the §ATM sample.
// $ ring occupies rows kRingY..kRingY+kRingH-1 and
// columns kRingX..kRingX+kRingW-1.
constexpr int kCanvasW  = 48;
constexpr int kCanvasH  = 17;

// Outer $ ring bounds (inner edge of the ring row/col).
constexpr int kRingX    = 3;
constexpr int kRingY    = 2;
constexpr int kRingW    = 42;   // kRingX + kRingW - 1 = 44
constexpr int kRingH    = 13;   // kRingY + kRingH - 1 = 14

// AUTH — 11w × 5h, upper-left inside the ring.
constexpr int kAuthX    = 5;
constexpr int kAuthY    = 3;
constexpr int kAuthW    = 11;
constexpr int kAuthH    = 5;

// VAULT — 10w × 7h, upper-right inside the ring.
constexpr int kVaultX   = 33;
constexpr int kVaultY   = 3;
constexpr int kVaultW   = 10;
constexpr int kVaultH   = 7;

// BALANCE — 11w × 4h, center-bottom inside the ring.
constexpr int kBalanceX = 18;
constexpr int kBalanceY = 8;
constexpr int kBalanceW = 11;
constexpr int kBalanceH = 4;

// "ATM #NNN" id from the seed — four decimal digits per the design sample.
std::string atm_id(uint32_t seed) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "%04u", static_cast<unsigned>(seed % 10000u));
    return buf;
}

}  // namespace

Netspace gen_atm_netspace(const TargetDescriptor& desc) {
    NetspaceBuilder b(kCanvasW, kCanvasH, NetTile::Void);
    b.set_target(desc);

    // Title — bank brand + unit id + urgency tag.
    char title[64];
    std::snprintf(title, sizeof title, "NCBANK :: ATM #%s :: TRACE PRIMED",
                  atm_id(desc.seed).c_str());
    b.set_title(title);

    // Four rooms: AUTH + VAULT + BALANCE + EXIT node (tiny 3×3).
    // Reserve to keep NetRoom references stable across add_room calls.
    b.ns.rooms.reserve(static_cast<size_t>(4));

    // ── AUTH ───────────────────────────────────────────────────────────
    NetRoom& auth = b.add_room(kAuthX, kAuthY, kAuthW, kAuthH, "AUTH",
                               NetRoom::Border::Thin);
    auth.label_color    = net_theme::box_thin_color;
    auth.top_content    = "\xe2\x96\x91 @ \xe2\x96\x91";  // ░ @ ░
    auth.top_color      = net_theme::wall_light;
    auth.bottom_content = "\xe2\x97\x8a\xe2\x97\x8a\xe2\x97\x8a";  // ◊◊◊
    auth.bottom_color   = net_theme::data_node;
    b.set_jack_in(auth);

    // ── BALANCE ────────────────────────────────────────────────────────
    NetRoom& balance = b.add_room(kBalanceX, kBalanceY, kBalanceW, kBalanceH,
                                  "BALANCE", NetRoom::Border::Thin);
    balance.label_color    = net_theme::box_thin_color;
    balance.bottom_content = "$$$$$";
    balance.bottom_color   = net_theme::data_node;

    // ── VAULT ──────────────────────────────────────────────────────────
    NetRoom& vault = b.add_room(kVaultX, kVaultY, kVaultW, kVaultH, "VAULT",
                                NetRoom::Border::Thin);
    vault.label_color    = net_theme::box_thin_color;
    vault.top_content    = "\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93\xe2\x96\x93";  // ▓▓▓▓▓▓
    vault.top_color      = net_theme::wall_heavy;
    vault.bottom_content = "";   // breakwall gate occupies the bottom interior row
    vault.bottom_color   = net_theme::data_node;

    // ── Connections ── hand-stamped, attaching at EDGE-CENTRES with
    // make_passable ports so the avatar can actually traverse
    // AUTH → BALANCE → VAULT (the auto-router attached at corners, which
    // are impassable border tiles → no walkable path).
    auto hrun = [&](int y, int x0, int x1) {
        for (int x = x0; x <= x1; ++x) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeV ? NetTile::PipeJunc
                                                 : NetTile::PipeH);
        }
    };
    auto vrun = [&](int x, int y0, int y1) {
        for (int y = y0; y <= y1; ++y) {
            NetTile cur = b.ns.at(x, y);
            b.ns.set(x, y, cur == NetTile::PipeH ? NetTile::PipeJunc
                                                 : NetTile::PipeV);
        }
    };

    // AUTH right-centre → BALANCE top-centre.
    const int auth_port_x = kAuthX + kAuthW - 1;
    const int auth_port_y = kAuthY + kAuthH / 2;
    const int bal_cx_     = kBalanceX + kBalanceW / 2;
    b.make_passable(auth_port_x, auth_port_y);
    hrun(auth_port_y, auth_port_x + 1, bal_cx_);
    vrun(bal_cx_, auth_port_y + 1, kBalanceY - 1);
    b.ns.set(bal_cx_, auth_port_y, NetTile::PipeJunc);  // bend → corner glyph
    b.make_passable(bal_cx_, kBalanceY);

    // BALANCE right-centre → VAULT bottom-centre (gated by a breakwall on
    // VAULT's bottom interior row — Breach to enter).
    const int bal_port_x = kBalanceX + kBalanceW - 1;
    const int bal_port_y = kBalanceY + kBalanceH / 2;
    const int vault_cx   = kVaultX + kVaultW / 2;
    const int vault_bot  = kVaultY + kVaultH - 1;
    b.make_passable(bal_port_x, bal_port_y);
    hrun(bal_port_y, bal_port_x + 1, vault_cx);
    vrun(vault_cx, vault_bot + 1 <= bal_port_y ? vault_bot + 1 : bal_port_y,
                   bal_port_y);
    b.make_passable(vault_cx, vault_bot);
    // Breakwall across VAULT's bottom interior row — the entry gate.
    b.add_breakwall_row(kVaultX + 1, kVaultX + kVaultW - 2,
                        kVaultY + kVaultH - 2, 4);

    // ── EXIT (greedy / post-VAULT) ─────────────────────────────────────
    // Place exit tile one cell right of VAULT's right border so it is
    // reachable after cracking the VAULT. Open a passable port through
    // VAULT's right wall at the avatar row and lay a short H-pipe.
    const int vault_right  = kVaultX + kVaultW - 1;
    const int vault_mid_y  = kVaultY + kVaultH / 2;
    const int exit_x       = vault_right + 2;
    const int exit_y       = vault_mid_y;
    b.make_passable(vault_right, vault_mid_y);
    b.ns.set(vault_right + 1, vault_mid_y, NetTile::PipeH);
    b.ns.set(exit_x, exit_y, NetTile::Exit);
    b.ns.exit_x = exit_x;
    b.ns.exit_y = exit_y;

    // ── EXIT (safe / BALANCE branch) ───────────────────────────────────
    // A second ungated Exit reachable from AUTH without touching the VAULT
    // breakwall.  Spec: BALANCE is the safe lesser payout — grab it and
    // jack out cleanly without cracking VAULT.
    //
    // Placement: one cell outside AUTH's LEFT wall at the jack-in (focus)
    // row.  AUTH's right wall at y=kAuthY+1=4 is where the AUTH→BALANCE
    // pipe exits; the left wall at y=kAuthY+kAuthH-2=6 (bottom interior
    // row / jack-in row) is entirely unconnected to the pipe path, so
    // stepping through it is a deliberate side-step, not an incidental
    // crossing.  The cell at (kAuthX-1, 6) is Void at gen time and lies
    // outside the $ ring (left ring column is at x=kRingX=3; kAuthX-1=4).
    //
    // press_luck_step==0 for ATM ⟹ no trace cost on this jack-out.
    // Canonical exit_x/exit_y remain the post-VAULT cell above.
    {
        const int safe_exit_y = kAuthY + kAuthH - 2;  // = 6 — AUTH bottom interior / jack-in row
        const int auth_left   = kAuthX;               // = 5 — AUTH left border
        const int safe_exit_x = kAuthX - 1;           // = 4 — one cell outside left border
        b.make_passable(auth_left, safe_exit_y);
        // Clear any $ ring glyph that may have landed on this cell
        // (ring left column is at x=3; x=4 is outside it, but guard anyway).
        b.ns.glyph_overrides.erase({safe_exit_x, safe_exit_y});
        b.ns.set(safe_exit_x, safe_exit_y, NetTile::Exit);
    }

    // ── $ border ring ──────────────────────────────────────────────────
    // Stamp outer ring just inside canvas edge. Every $ is both decorative
    // and a spawn-pool source (the TraceAtLeast trigger uses empty cells
    // from glyph_overrides when NetSpawnSpec::cells is empty).
    auto put_dollar = [&](int x, int y) {
        if (!b.ns.in_bounds(x, y)) return;
        b.ns.set(x, y, NetTile::Glyph);
        b.ns.glyph_overrides[{x, y}] = "$";
    };
    // Top and bottom rows of the ring.
    for (int x = kRingX; x < kRingX + kRingW; ++x) {
        put_dollar(x, kRingY);
        put_dollar(x, kRingY + kRingH - 1);
    }
    // Left and right columns (interior rows only, skip corners).
    for (int y = kRingY + 1; y < kRingY + kRingH - 1; ++y) {
        put_dollar(kRingX,              y);
        put_dollar(kRingX + kRingW - 1, y);
    }

    // Re-assert the Exit tile last — the $ ring loop may have stamped a
    // decorative Glyph over the exit cell (right column). Clearing the
    // override also keeps the trace>=100 packet wave from spawning here.
    b.ns.set(exit_x, exit_y, NetTile::Exit);
    b.ns.glyph_overrides.erase({exit_x, exit_y});

    // ── Trace hint ─────────────────────────────────────────────────────
    b.ns.trace_tick_hint = 2;   // bank ICE is FAST

    // ── Action nodes ───────────────────────────────────────────────────
    // BALANCE interior content cell — one row above bottom border.
    // add_room fills interior; bottom_content row is at kBalanceY + kBalanceH - 2.
    const int bal_cx = kBalanceX + kBalanceW / 2;
    const int bal_cy = kBalanceY + kBalanceH - 2;

    NetNode bal;
    bal.kind    = NetNodeKind::Stash;
    bal.label   = "$$$$";
    bal.payload = desc.seed & 0xFFFFu;
    bal.x       = bal_cx;
    bal.y       = bal_cy;
    b.ns.action_nodes.push_back(bal);

    // VAULT interior content cell — above the bottom-row breakwall gate
    // (reachable only after Breach-ing in from the bottom port).
    const int vg_cx = kVaultX + kVaultW / 2;
    const int vg_cy = kVaultY + 2;

    NetNode vg;
    vg.kind    = NetNodeKind::VaultGrab;
    vg.label   = "$$$$$$";
    vg.payload = 300u + (static_cast<uint32_t>(desc.tier) * 200u);
    vg.x       = vg_cx;
    vg.y       = vg_cy;
    b.ns.action_nodes.push_back(vg);

    // ── Triggers ───────────────────────────────────────────────────────
    // FRAUD enforcer spawns after K net turns (near AUTH interior).
    NetTrigger fraud;
    fraud.cond      = NetTriggerCond::TurnCountAtLeast;
    fraud.threshold = 6 - (desc.tier < 3 ? desc.tier : 3);
    fraud.spawn.color = IceColor::Gray;
    fraud.spawn.hp    = 2;
    fraud.spawn.count = 1;
    // Deterministic floor cell just inside AUTH.
    fraud.spawn.cells = { { kAuthX + 1, kAuthY + 1 } };
    b.ns.triggers.push_back(fraud);

    // $ → hostile packet wave at 100% trace (empty cells ⇒ from $-border).
    NetTrigger packets;
    packets.cond      = NetTriggerCond::TraceAtLeast;
    packets.threshold = 100;
    packets.spawn.color = IceColor::Gray;
    packets.spawn.hp    = 1;
    packets.spawn.count = 4 + (desc.tier >= 3 ? 2 : 0);
    // cells intentionally empty — tick_grid converts random $ glyph_overrides.
    b.ns.triggers.push_back(packets);

    return b.finalize();
}

}  // namespace astra

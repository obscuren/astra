#include "astra/soul_mirror.h"

#include "astra/consciousness_save.h"
#include "astra/game.h"
#include "astra/hackable.h"
#include "astra/player.h"
#include "astra/renderer.h"
#include "astra/ui.h"
#include "astra/world_manager.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <random>

namespace astra::soul_mirror {

namespace {

bool player_on_console_tile(Game& game, const Hackable& console) {
    // Hackable position comes from the fixture that carries it; the caller
    // sets s.console only for PrecursorConsole fixtures, which store their
    // tile coordinates via the interaction site passed at begin_*.
    // console_x/console_y are exposed on HackableChannelPos stored in Game.
    return game.soul_mirror_state().console_x == game.player().x &&
           game.soul_mirror_state().console_y == game.player().y;
}

void commit_fragment(Game& game, Hackable& console) {
    for (auto& f : console.lore_fragments) {
        if (f.committed) continue;
        f.committed = true;

        ConsciousnessSave cs;
        read_consciousness(cs);   // may return false; we still write fresh

        LoreFragmentRef ref;
        ref.archive_id         = f.archive_id;
        ref.galaxy_seed_origin = game.world().seed();
        ref.world_tick_origin  = static_cast<int32_t>(game.world().world_tick());
        cs.lore_archive.push_back(std::move(ref));

        if (cs.consciousness_id == 0) {
            std::random_device rd;
            cs.consciousness_id =
                (static_cast<uint64_t>(rd()) << 32) | static_cast<uint64_t>(rd());
        }
        write_consciousness(cs);
        game.log("Lore fragment committed: " + f.archive_id);
        return;
    }
    game.log("Console fully synced.");
}

} // namespace

// ---------------------------------------------------------------------------

void begin_active(Game& game, Hackable& console) {
    auto& s = game.soul_mirror_state();
    s.active    = true;
    s.passive   = false;
    s.console   = &console;
    s.console_x = game.player().x;
    s.console_y = game.player().y;
    game.log("Sync Soul: channel begun.");
}

void begin_passive(Game& game, Hackable& console) {
    auto& s = game.soul_mirror_state();
    s.active    = true;
    s.passive   = true;
    s.console   = &console;
    s.console_x = game.player().x;
    s.console_y = game.player().y;
    // No log — passive is invisible.
}

void tick(Game& game) {
    auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;

    if (!player_on_console_tile(game, *s.console)) {
        s.active = false;   // pause — do not reset progress
        return;
    }

    s.console->soul_mirror_progress += kProgressPerTurn;

    if (!s.passive) {
        int& ep = game.player().energy;
        if (ep > 0) ep = std::max(0, ep - kEpCostPerTurn);
    }

    if (s.console->soul_mirror_progress >= kCommitThreshold) {
        s.console->soul_mirror_progress = 0;
        commit_fragment(game, *s.console);
    }
}

void on_player_damaged(Game& game) {
    auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;
    s.active = false;   // pause — no reset
    game.hacking().add_detection(kDamageDetectionBurst);
}

bool is_active(const Game& game) {
    return game.soul_mirror_state().active;
}

void render_hud_strip(Game& game, Renderer& r) {
    const auto& s = game.soul_mirror_state();
    if (!s.active || !s.console) return;

    int prog   = s.console->soul_mirror_progress;
    int ep_cur = game.player().energy;
    int ep_max = std::max(1, game.player().effective_willpower() * 3);

    // Centre, two rows below the XP bar, half the screen wide.
    Rect xp     = game.xp_bar_rect();
    int  scr_w  = r.get_width();
    int  width  = std::max(40, scr_w / 2);
    int  x      = (scr_w - width) / 2;
    int  y      = xp.y + 2;
    Rect rect{x, y, width, 1};
    UIContext ctx(&r, rect);

    // Bar geometry: label + bar + value lines fit in `width` columns.
    int    pct       = std::min(100, (prog * 100) / kCommitThreshold);
    char   tail[64];
    std::snprintf(tail, sizeof(tail),
                  "  %d/%d next fragment   EP %d/%d",
                  prog, kCommitThreshold, ep_cur, ep_max);

    const char* lead = "SYNC IN PROGRESS  ";
    int  lead_w = static_cast<int>(std::strlen(lead));
    int  tail_w = static_cast<int>(std::strlen(tail));
    int  bar_w  = std::max(8, width - lead_w - tail_w);

    ctx.text(0, 0, lead, Color::BrightWhite);
    ctx.bar(lead_w, 0, bar_w, prog, kCommitThreshold,
            Color::Cyan, Color::DarkGray, '#', '-');
    ctx.text(lead_w + bar_w, 0, tail, Color::Cyan);
    (void)pct;   // pct is reflected in the filled bar segments
}

} // namespace astra::soul_mirror

#include "astra/rebirth_sequence.h"

#include "astra/consciousness_save.h"
#include "astra/game.h"
#include "astra/game_state.h"
#include "astra/renderer.h"

#include <cstdio>
#include <cstring>

namespace astra {

namespace {

constexpr const char* kCinematicLines[] = {
    "The body unmakes itself at the event horizon.",
    "Spacetime curves into a single bright point.",
    "...consciousness uploads...",
    "...the galaxy collapses behind you...",
    "...knowledge persists through the singularity...",
    "                  [Press any key]",
};
constexpr int kCinematicCount =
    static_cast<int>(sizeof(kCinematicLines) / sizeof(*kCinematicLines));

void draw_centered(Renderer& r, int row, const char* s, Color c) {
    int len = static_cast<int>(std::strlen(s));
    int x = std::max(0, 80 / 2 - len / 2);   // good enough — terminal is wide
    r.draw_glyph(x, row, " ", Color::White);  // ensure row drawn
    for (int i = 0; i < len; ++i) {
        char buf[2] = {s[i], 0};
        r.draw_glyph(x + i, row, buf, c);
    }
}

} // namespace

void RebirthSequence::begin() {
    phase_         = Phase::Confirm;
    cinematic_idx_ = 0;
    survives_.clear();

    ConsciousnessSave cs;
    bool have = read_consciousness(cs);
    if (have && cs.consciousness_id != 0) {
        char buf[96];
        std::snprintf(buf, sizeof(buf),
                      "consciousness id  %016llx",
                      static_cast<unsigned long long>(cs.consciousness_id));
        survives_.emplace_back(buf);
        std::snprintf(buf, sizeof(buf),
                      "rebirth count     %u  -> %u",
                      cs.rebirth_count, cs.rebirth_count + 1);
        survives_.emplace_back(buf);
    } else {
        survives_.emplace_back("first rebirth — consciousness will be assigned");
    }
    if (have && cs.deep_grid_base.w > 0) {
        survives_.emplace_back("deep-Grid base  Your.Anchor");
    }
    if (have && !cs.lore_archive.empty()) {
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "lore fragments   %zu", cs.lore_archive.size());
        survives_.emplace_back(buf);
    }
    if (have && cs.grid_currency > 0) {
        char buf[64];
        std::snprintf(buf, sizeof(buf),
                      "grid currency    %d", cs.grid_currency);
        survives_.emplace_back(buf);
    }
}

void RebirthSequence::apply(Game& game) {
    ConsciousnessSave cs;
    read_consciousness(cs);                 // ok if missing — defaults
    cs.rebirth_count++;
    cs.seen_first_rebirth = true;
    write_consciousness(cs);

    // Galaxy-save deletion + auto-reseed are deferred — the player returns to
    // the main menu and starts a fresh game. The new galaxy will pick up the
    // updated consciousness.dat on first interaction.
    game.set_state(GameState::MainMenu);
    game.log("You wake. The galaxy is new. Memory persists.");
}

bool RebirthSequence::handle_key(Game& game, int key) {
    if (phase_ == Phase::Inactive) return false;

    if (phase_ == Phase::Confirm) {
        if (key == '\n' || key == '\r') {
            ConsciousnessSave cs;
            read_consciousness(cs);
            if (!cs.seen_first_rebirth) {
                phase_         = Phase::Cinematic;
                cinematic_idx_ = 0;
            } else {
                apply(game);
                phase_ = Phase::Inactive;
            }
            return true;
        }
        if (key == 27) {                    // Esc — cancel
            phase_ = Phase::Inactive;
            return true;
        }
        return true;                        // swallow other keys
    }

    if (phase_ == Phase::Cinematic) {
        if (cinematic_idx_ < kCinematicCount - 1) {
            ++cinematic_idx_;
        } else {
            apply(game);
            phase_ = Phase::Inactive;
        }
        return true;
    }
    return false;
}

void RebirthSequence::render(const Game& /*game*/, Renderer& r) const {
    if (phase_ == Phase::Inactive) return;

    if (phase_ == Phase::Confirm) {
        int row = 6;
        draw_centered(r, row++, "── Sgr A* — REBIRTH ──", Color::BrightYellow);
        ++row;
        draw_centered(r, row++, "Crossing the event horizon will end this galaxy.",
                      Color::White);
        draw_centered(r, row++, "These will survive your rebirth:", Color::White);
        ++row;
        for (const auto& s : survives_) {
            draw_centered(r, row++, s.c_str(), Color::Cyan);
        }
        ++row;
        draw_centered(r, row++, "[Enter] cross   [Esc] back", Color::DarkGray);
        return;
    }

    if (phase_ == Phase::Cinematic) {
        int reveal = std::min(cinematic_idx_ + 1, kCinematicCount);
        int row    = 8;
        for (int i = 0; i < reveal; ++i) {
            Color c = (i == reveal - 1) ? Color::BrightWhite : Color::Cyan;
            draw_centered(r, row + i * 2, kCinematicLines[i], c);
        }
    }
}

} // namespace astra

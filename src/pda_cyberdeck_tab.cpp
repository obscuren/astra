#include "astra/pda_screen.h"

#include "astra/cyberdeck.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/program_compiler.h"

#include <string>

namespace astra {

namespace {

void draw_deck_subscreen(PdaScreen& self, UIContext& ctx) {
    auto* deck_slot = self.player().equipment.equipped_cyberdeck();
    if (!deck_slot || !*deck_slot || !(*deck_slot)->deck) {
        int cy = ctx.height() / 2 - 2;
        ctx.text(ctx.width() / 2 - 13, cy,
                 "-- NO CYBERDECK EQUIPPED --", Color::DarkGray);
        ctx.text(4, cy + 2,
                 "Equip a cyberdeck in a Utility slot.", Color::DarkGray);
        return;
    }
    auto& deck      = *(*deck_slot)->deck;
    auto  deck_name = (*deck_slot)->name;

    // ── Left pane: deck stats + slot list ────────────────────────────────
    ctx.text(2, 1, deck_name, Color::White);
    ctx.text(2, 2, "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                   "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                   "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80",
             Color::DarkGray);

    int y = 3;
    auto stat = [&](const std::string& label, const std::string& value) {
        ctx.text(2, y, label, Color::DarkGray);
        ctx.text(2 + static_cast<int>(label.size()), y, value, Color::Default);
        ++y;
    };
    stat("RAM      ", std::to_string(deck.ram_current) + "/" + std::to_string(deck.stats.ram_max));
    stat("HEAT     ", std::to_string(deck.heat_current) + "/" + std::to_string(deck.stats.heat_cap));
    stat("COOLING  ", std::to_string(deck.stats.cooling_rate) + "/turn");
    stat("STEALTH  ", "+" + std::to_string(deck.stats.stealth));

    ++y;
    ctx.text(2, y++, "PROGRAM SLOTS", Color::White);
    for (int i = 0; i < deck.stats.slots; ++i) {
        const auto& sl = deck.loaded[i];
        if (sl.program_def_id == 0) {
            ctx.text(2, y++, "  " + std::to_string(i + 1) + " \xe2\x96\xa2 (empty)",
                     Color::DarkGray);
        } else {
            Item probe = build_by_def_id(sl.program_def_id);
            ctx.text(2, y++, "  " + std::to_string(i + 1) + " \xe2\x96\xa3 " + probe.name,
                     Color::Default);
        }
    }

    // ── Right pane: compiled-program inventory list ───────────────────────
    int half = ctx.width() / 2;
    ctx.text(half + 2, 1, "COMPILED PROGRAMS", Color::White);
    ctx.text(half + 2, 2,
             "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
             "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
             "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
             "\xe2\x94\x80\xe2\x94\x80",
             Color::DarkGray);
    int yr = 3;
    for (const auto& it : self.player().inventory.items) {
        if (!it.compiled_program.has_value()) continue;
        ctx.text(half + 2, yr++, "  " + it.name, Color::Default);
    }
    if (yr == 3) {
        ctx.text(half + 2, yr, "  (none)", Color::DarkGray);
    }
}

void draw_compiler_subscreen(PdaScreen& /*self*/, UIContext& ctx) {
    int cy = ctx.height() / 2 - 2;
    ctx.text(ctx.width() / 2 - 12, cy,
             "-- COMPILER (TASK 12) --", Color::DarkGray);
    ctx.text(4, cy + 2,
             "Compiler workspace lands in the next task.", Color::DarkGray);
}

void draw_patterns_overlay(PdaScreen& /*self*/, UIContext& ctx) {
    int cy = ctx.height() / 2 - 2;
    ctx.text(ctx.width() / 2 - 12, cy,
             "-- PATTERNS (TASK 13) --", Color::DarkGray);
}

}  // namespace

void PdaScreen::draw_cyberdeck(UIContext& ctx) {
    if (cyberdeck_show_patterns_overlay_) {
        draw_patterns_overlay(*this, ctx);
        return;
    }
    switch (cyberdeck_subscreen_) {
        case CyberdeckSubscreen::Deck:     draw_deck_subscreen(*this, ctx);     break;
        case CyberdeckSubscreen::Compiler: draw_compiler_subscreen(*this, ctx); break;
    }
}

void PdaScreen::handle_cyberdeck_key(int key) {
    if (key == '\t') {
        cyberdeck_subscreen_ = (cyberdeck_subscreen_ == CyberdeckSubscreen::Deck)
                             ? CyberdeckSubscreen::Compiler
                             : CyberdeckSubscreen::Deck;
        return;
    }
    if (key == 'p') {
        cyberdeck_show_patterns_overlay_ = !cyberdeck_show_patterns_overlay_;
        return;
    }
    // Subscreen-specific keys land in Tasks 12 + 13.
}

}  // namespace astra

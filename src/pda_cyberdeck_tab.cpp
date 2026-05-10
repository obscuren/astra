#include "astra/pda_screen.h"

#include "astra/cyberdeck.h"
#include "astra/fragment.h"
#include "astra/game.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/program_compiler.h"
#include "astra/program_pattern.h"
#include "astra/skill_defs.h"

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

// ── Compiler helpers ──────────────────────────────────────────────────────

// Render a fragment chain recursively. Returns the next free y row.
int render_chain(UIContext& ctx, int x, int y,
                 const std::vector<ProgramNode>& chain,
                 const std::vector<int>& cursor_path,
                 int /*depth*/) {
    for (size_t i = 0; i < chain.size(); ++i) {
        const auto& n   = chain[i];
        const FragmentDef* def = find_fragment(n.fragment);
        bool sel = !cursor_path.empty() &&
                   cursor_path.front() == static_cast<int>(i);

        if (def && def->kind == FragmentKind::Container) {
            // Header
            std::string header = std::string(BoxDraw::TL) + "\xe2\x94\x80 "
                               + def->display
                               + "(" + std::to_string(n.param) + ") \xe2\x94\x80";
            ctx.text(x, y, header, sel ? Color::Cyan : Color::White);
            ++y;

            // Body — build sub-path
            std::vector<int> sub_path;
            if (sel && cursor_path.size() > 1)
                sub_path.assign(cursor_path.begin() + 1, cursor_path.end());
            y = render_chain(ctx, x + 2, y, n.body, sub_path, 1);

            // Footer
            ctx.text(x, y, std::string(BoxDraw::BL) + "\xe2\x94\x80\xe2\x94\x80", Color::White);
            ++y;
        } else {
            std::string label;
            if (def) {
                if (def->takes_param)
                    label = "[" + std::string(def->display) + "(" + std::to_string(n.param) + ")]";
                else
                    label = "[" + std::string(def->display) + "]";
            } else {
                label = "[???]";
            }
            ctx.text(x, y, label, sel ? Color::Cyan : Color::Default);
            ++y;
        }
    }
    if (chain.empty()) {
        ctx.text(x, y, "(empty \xe2\x80\x94 pick a fragment from the palette)", Color::DarkGray);
        ++y;
    }
    return y;
}

void draw_compiler_subscreen(PdaScreen& self, UIContext& ctx) {
    // ── Header ────────────────────────────────────────────────────────────
    int disks = 0;
    for (const auto& it : self.player().inventory.items) {
        if (it.item_def_id == ITEM_PROGRAM_DISK) disks += it.stack_count;
    }
    int ceiling = max_program_fragments(self.player());
    std::string header = "Program Disks: " + std::to_string(disks)
                       + "    |    Programming ceiling: "
                       + std::to_string(ceiling) + " fragments";
    ctx.text(2, 0, header, Color::White);

    int third  = ctx.width() / 3;
    int col_p  = 0;
    int col_b  = third;
    int col_v  = 2 * third;

    // ── Left pane: fragment palette ───────────────────────────────────────
    ctx.text(col_p + 2, 2, "FRAGMENTS", Color::White);

    int yp = 4;
    FragmentKind last_kind = FragmentKind::Container;  // sentinel ≠ first real kind
    bool first = true;
    const auto& catalog = fragment_catalog();
    for (size_t i = 0; i < catalog.size(); ++i) {
        const auto& def = catalog[i];
        if (def.id == FragmentId::None) continue;

        // Print kind header when kind changes
        if (first || def.kind != last_kind) {
            const char* kind_label =
                (def.kind == FragmentKind::Producer)    ? "PRODUCERS"   :
                (def.kind == FragmentKind::Transformer) ? "OPERATORS"   :
                                                          "CONTAINERS";
            ctx.text(col_p, yp++, kind_label, Color::Yellow);
            last_kind = def.kind;
            first = false;
        }

        bool known = false;
        for (auto fid : self.player().learned_fragments)
            if (fid == def.id) { known = true; break; }

        bool sel = static_cast<int>(i) == self.compiler_palette_cursor();
        std::string line = " " + std::string(def.display) + "  "
                         + std::to_string(def.exec_cost) + "/"
                         + std::to_string(def.heat_cost);
        Color color = !known ? Color::DarkGray
                    : sel    ? Color::Cyan
                             : Color::Default;
        ctx.text(col_p, yp++, line, color);
    }

    // Hint
    ctx.text(col_p, yp + 1, " \xe2\x86\x91\xe2\x86\x93 navigate  Enter append", Color::DarkGray);
    ctx.text(col_p, yp + 2, " b/B enter/exit body  c compile", Color::DarkGray);

    // ── Middle pane: build ────────────────────────────────────────────────
    ctx.text(col_b + 2, 2,
             "BUILD (max " + std::to_string(ceiling) + " fragments)",
             Color::White);
    render_chain(ctx, col_b, 4,
                 self.compiler_build(),
                 self.compiler_cursor_path(), 0);

    // ── Right pane: live preview ─────────────────────────────────────────
    auto cp = compile_program(self.compiler_build(), "");
    ctx.text(col_v + 2, 2, "PREVIEW", Color::White);
    int yv = 4;
    ctx.text(col_v, yv++, " Effect:", Color::Default);
    if (!cp.resolved.named_pattern.empty()) {
        ctx.text(col_v, yv++, "  \xe2\x96\xba " + cp.resolved.named_pattern, Color::Green);
    }
    ctx.text(col_v, yv++, "  damage " + std::to_string(cp.resolved.damage), Color::DarkGray);
    ctx.text(col_v, yv++, "  radius " + std::to_string(cp.resolved.radius), Color::DarkGray);
    ctx.text(col_v, yv++, "  ticks  " + std::to_string(cp.resolved.tick_count), Color::DarkGray);
    ++yv;
    ctx.text(col_v, yv++, " Costs:", Color::Default);
    ctx.text(col_v, yv++, "  exec " + std::to_string(cp.exec_cost), Color::DarkGray);
    ctx.text(col_v, yv++, "  heat " + std::to_string(cp.heat_cost), Color::DarkGray);
    ctx.text(col_v, yv++, "  ram  " + std::to_string(cp.ram_held), Color::DarkGray);
    if (!cp.patterns_lit.empty()) {
        ++yv;
        ctx.text(col_v, yv++, " Patterns lit:", Color::Default);
        for (const auto& pat : cp.patterns_lit)
            ctx.text(col_v, yv++, "  \xe2\x96\xba " + pat, Color::Green);
    }
}

// ── Compiler key-handling helpers ──────────────────────────────────────────

// Return pointer to the node at the cursor (last node in the current chain).
ProgramNode* node_at_cursor(PdaScreen& self) {
    const auto& path = self.compiler_cursor_path();
    if (path.empty()) {
        auto& chain = self.compiler_build_mut();
        if (chain.empty()) return nullptr;
        return &chain.back();
    }
    auto* chain = &self.compiler_build_mut();
    for (size_t i = 0; i + 1 < path.size(); ++i) {
        if (path[i] < 0 || path[i] >= static_cast<int>(chain->size())) return nullptr;
        chain = &(*chain)[path[i]].body;
    }
    int last = path.back();
    if (last < 0 || last >= static_cast<int>(chain->size())) return nullptr;
    return &(*chain)[last];
}

void append_fragment(PdaScreen& self, FragmentId id) {
    int ceiling = max_program_fragments(self.player());
    if (ceiling == 0) return;

    const auto& path = self.compiler_cursor_path();
    auto* chain = &self.compiler_build_mut();
    for (int idx : path) {
        if (idx < 0 || idx >= static_cast<int>(chain->size())) return;
        chain = &(*chain)[idx].body;
    }
    if (path.empty() && static_cast<int>(chain->size()) >= ceiling) return;

    const FragmentDef* def = find_fragment(id);
    if (!def) return;
    ProgramNode n;
    n.fragment = id;
    n.param    = def->takes_param ? def->default_n : 0;
    chain->push_back(n);
}

void enter_body(PdaScreen& self) {
    auto& path  = self.compiler_cursor_path_mut();
    auto* chain = &self.compiler_build_mut();
    for (int idx : path) {
        if (idx < 0 || idx >= static_cast<int>(chain->size())) return;
        chain = &(*chain)[idx].body;
    }
    if (chain->empty()) return;
    int last = static_cast<int>(chain->size()) - 1;
    const FragmentDef* def = find_fragment((*chain)[last].fragment);
    if (!def || def->kind != FragmentKind::Container) return;
    path.push_back(last);
}

void exit_body(PdaScreen& self) {
    auto& path = self.compiler_cursor_path_mut();
    if (!path.empty()) path.pop_back();
}

void compile_action(PdaScreen& self, Game& game) {
    if (self.compiler_build().empty()) return;

    // Find a program disk in inventory
    int disk_idx = -1;
    auto& inv = self.player().inventory.items;
    for (size_t i = 0; i < inv.size(); ++i) {
        if (inv[i].item_def_id == ITEM_PROGRAM_DISK) {
            disk_idx = static_cast<int>(i);
            break;
        }
    }
    if (disk_idx < 0) {
        game.log("No Program Disk in inventory.");
        return;
    }

    // Consume one disk
    auto& disk = inv[disk_idx];
    if (disk.stack_count > 1)
        disk.stack_count -= 1;
    else
        inv.erase(inv.begin() + disk_idx);

    // Compile
    auto cp = compile_program(self.compiler_build(), "");

    // Build an Item holding the compiled program
    Item out;
    out.item_def_id = 0;
    out.id          = 9500;
    out.name        = cp.name;
    out.description = "Player-compiled program ("
                    + std::to_string(cp.chain.size()) + " fragments).";
    out.type        = ItemType::Special;
    out.rarity      = Rarity::Common;
    out.weight      = 1;
    out.stackable   = false;
    out.compiled_program = std::move(cp);
    inv.push_back(std::move(out));

    const auto& saved = inv.back();
    game.log("Compiled: " + saved.name + ".");

    // Pattern discovery
    if (saved.compiled_program.has_value()) {
        for (const auto& pat : saved.compiled_program->patterns_lit) {
            bool already = false;
            for (const auto& d : self.player().discovered_patterns)
                if (d == pat) { already = true; break; }
            if (!already) {
                self.player().discovered_patterns.push_back(pat);
                game.log("Pattern discovered: " + pat + "!");
            }
        }
    }

    // Reset workbench
    self.compiler_build_mut().clear();
    self.compiler_cursor_path_mut().clear();
}

void draw_patterns_overlay(PdaScreen& self, UIContext& ctx) {
    int total = static_cast<int>(pattern_catalog().size());
    int discovered = static_cast<int>(self.player().discovered_patterns.size());

    std::string header = "PATTERNS  DISCOVERED " + std::to_string(discovered)
                       + " / " + std::to_string(total);
    ctx.text(2, 1, header, Color::White);
    ctx.text(2, 2, "──────────────────────────────────────", Color::DarkGray);

    int y = 4;
    for (const auto& p : pattern_catalog()) {
        bool known = false;
        for (const auto& d : self.player().discovered_patterns) {
            if (d == p.name) { known = true; break; }
        }
        if (known) {
            std::string seq;
            for (auto fid : p.fragment_seq) {
                const FragmentDef* def = find_fragment(fid);
                if (!seq.empty()) seq += ", ";
                seq += def ? def->display : "?";
            }
            ctx.text(2, y++, "► " + p.name + "    [" + seq + "]", Color::Green);
            ctx.text(4, y++, p.description, Color::DarkGray);
        } else {
            ctx.text(2, y++, "  ??????????    [???, ???]", Color::DarkGray);
        }
        y += 1;
    }

    ctx.text(2, ctx.height() - 1, "[p / Esc] Back", Color::DarkGray);
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
    if (cyberdeck_subscreen_ != CyberdeckSubscreen::Compiler) return;
    if (cyberdeck_show_patterns_overlay_) return;

    const auto& palette = fragment_catalog();
    switch (key) {
        case KEY_UP:
            if (compiler_palette_cursor_ > 0) --compiler_palette_cursor_;
            break;
        case KEY_DOWN:
            if (compiler_palette_cursor_ < static_cast<int>(palette.size()) - 1)
                ++compiler_palette_cursor_;
            break;
        case '\n': case '\r': {
            if (compiler_palette_cursor_ >= 0 &&
                compiler_palette_cursor_ < static_cast<int>(palette.size())) {
                FragmentId id = palette[compiler_palette_cursor_].id;
                if (id != FragmentId::None)
                    append_fragment(*this, id);
            }
            break;
        }
        case '\b': case 127: {
            // Remove last node in current chain
            auto* chain = &compiler_build_;
            for (int idx : compiler_cursor_path_) chain = &(*chain)[idx].body;
            if (!chain->empty()) chain->pop_back();
            // If we just emptied a body we were inside, pop back out
            if (chain->empty() && !compiler_cursor_path_.empty())
                compiler_cursor_path_.pop_back();
            break;
        }
        case 'b':
            enter_body(*this);
            break;
        case 'B':
            exit_body(*this);
            break;
        case '+':
        case '=': {
            ProgramNode* n = node_at_cursor(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && def->takes_param && n->param < def->max_n)
                    ++n->param;
            }
            break;
        }
        case '-':
        case '_': {
            ProgramNode* n = node_at_cursor(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && def->takes_param && n->param > def->min_n)
                    --n->param;
            }
            break;
        }
        case 'c':
            if (game_) compile_action(*this, *game_);
            break;
        default: break;
    }
}

}  // namespace astra

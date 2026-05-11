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

    // ── Top header: ──┤ CYBERDECK ├────────────────── (full width)
    self.draw_section_header(ctx, 0, "CYBERDECK", 1, ctx.width() - 1);

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

    int half = ctx.width() / 2;

    // Vertical divider between left/right panes
    for (int r = 2; r < ctx.height() - 1; ++r) {
        ctx.text(half - 1, r, "\xe2\x94\x82", Color::DarkGray);
    }

    // ── Left pane header: ──┤ DECK ├──
    self.draw_section_header(ctx, 2, "DECK", 1, half - 1);

    ctx.text(2, 3, deck_name, Color::White);

    int y = 5;
    auto stat = [&](const std::string& label, const std::string& value) {
        ctx.text(2, y, label, Color::DarkGray);
        ctx.text(2 + static_cast<int>(label.size()), y, value, Color::Default);
        ++y;
    };
    stat("RAM      ", std::to_string(deck.ram_current) + "/" + std::to_string(deck.stats.ram_max));
    stat("HEAT     ", std::to_string(deck.heat_current) + "/" + std::to_string(deck.stats.heat_cap));
    stat("COOLING  ", std::to_string(deck.stats.cooling_rate) + "/turn");
    stat("STEALTH  ", "+" + std::to_string(deck.stats.stealth));

    // ── Sub-header: SLOTS ──┤
    ++y;
    self.draw_section_header(ctx, y++, "SLOTS", 1, half - 1);
    int slot_cursor = self.cyberdeck_slot_cursor();
    for (int i = 0; i < deck.stats.slots; ++i) {
        const auto& sl = deck.loaded[i];
        bool sel = (i == slot_cursor);
        std::string marker = sel ? "\xe2\x96\xb8 " : "  ";  // ▸
        std::string name;
        if (sl.compiled.has_value()) {
            name = sl.compiled->name;
        } else if (sl.program_def_id != 0) {
            Item probe = build_by_def_id(sl.program_def_id);
            name = probe.name;
        }
        if (name.empty()) {
            ctx.text(2, y++, marker + std::to_string(i + 1) + " \xe2\x96\xa2 (empty)",
                     sel ? Color::Cyan : Color::DarkGray);
        } else {
            ctx.text(2, y++, marker + std::to_string(i + 1) + " \xe2\x96\xa3 " + name,
                     sel ? Color::Cyan : Color::Default);
        }
    }

    ctx.text(2, ctx.height() - 2,
             " \xe2\x86\x91\xe2\x86\x93 slot   Space: load program",
             Color::DarkGray);

    // ── Right pane header: ──┤ COMPILED PROGRAMS ├──
    self.draw_section_header(ctx, 2, "COMPILED PROGRAMS", half + 1, ctx.width() - 1);
    int yr = 4;
    int found = 0;
    for (const auto& it : self.player().inventory.items) {
        if (!it.compiled_program.has_value()) continue;
        ctx.text(half + 2, yr++, "  " + it.name, Color::Default);
        ++found;
    }
    if (found == 0) {
        ctx.text(half + 2, yr, "  (none — compile some in the Compiler)", Color::DarkGray);
    }
}

// ── Compiler helpers ──────────────────────────────────────────────────────

// Render a fragment chain recursively. Returns the next free y row.
// `cursor_path` describes the insertion path from the OUTERMOST chain inward;
// when it's empty, this chain is where the next append lands and we draw a
// ▸ insertion marker at the end so the user can see the cursor.
int render_chain(UIContext& ctx, int x, int y,
                 const std::vector<ProgramNode>& chain,
                 const std::vector<int>& cursor_path,
                 int /*depth*/) {
    bool is_active_chain = cursor_path.empty();

    for (size_t i = 0; i < chain.size(); ++i) {
        const auto& n   = chain[i];
        const FragmentDef* def = find_fragment(n.fragment);
        bool on_path = !cursor_path.empty() &&
                       cursor_path.front() == static_cast<int>(i);

        // Inter-node flow arrow (between any two peer nodes — visualises pipeline)
        if (i > 0) {
            ctx.text(x + 1, y++, "\xe2\x86\x93", Color::DarkGray);  // ↓
        }

        if (def && def->kind == FragmentKind::Container) {
            std::string header = std::string(BoxDraw::TL) + "\xe2\x94\x80 "
                               + def->display
                               + "(" + std::to_string(n.param) + ") \xe2\x94\x80";
            ctx.text(x, y, header, on_path ? Color::Cyan : Color::White);
            ++y;

            std::vector<int> sub_path;
            if (on_path && cursor_path.size() > 1)
                sub_path.assign(cursor_path.begin() + 1, cursor_path.end());
            y = render_chain(ctx, x + 2, y, n.body,
                             on_path ? sub_path : std::vector<int>{-1}, 1);

            ctx.text(x, y, std::string(BoxDraw::BL) + "\xe2\x94\x80\xe2\x94\x80",
                     on_path ? Color::Cyan : Color::White);
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
            ctx.text(x, y, label, on_path ? Color::Cyan : Color::Default);
            ++y;
        }
    }

    if (chain.empty()) {
        if (is_active_chain) {
            ctx.text(x, y, "\xe2\x96\xb8", Color::Cyan);  // ▸ standalone insertion marker
        } else {
            ctx.text(x, y, "(empty)", Color::DarkGray);
        }
        ++y;
    } else if (is_active_chain) {
        ctx.text(x + 1, y++, "\xe2\x86\x93", Color::DarkGray);   // ↓ flow into next
        ctx.text(x, y, "\xe2\x96\xb8", Color::Cyan);              // ▸ cursor
        ++y;
    }
    return y;
}

void draw_compiler_subscreen(PdaScreen& self, UIContext& ctx) {
    int disks = 0;
    for (const auto& it : self.player().inventory.items) {
        if (it.item_def_id == ITEM_PROGRAM_DISK) disks += it.stack_count;
    }
    int ceiling = max_program_fragments(self.player());

    // ── Top header (full width) ──┤ COMPILER ├───────────────────────────
    self.draw_section_header(ctx, 0, "COMPILER", 1, ctx.width() - 1);

    std::string stats = "Program Disks: " + std::to_string(disks)
                      + "    |    Programming ceiling: "
                      + std::to_string(ceiling) + " fragments";
    ctx.text(2, 1, stats, Color::DarkGray);

    // Layout: narrow side panes (1/6 each = 1/3 combined), wide BUILD in
    // the middle (2/3) so the program tree has room to breathe.
    int sixth  = ctx.width() / 6;
    int col_p  = 0;
    int col_b  = sixth;
    int col_v  = ctx.width() - sixth;

    // Vertical dividers between the three panes
    for (int r = 3; r < ctx.height() - 1; ++r) {
        ctx.text(col_b - 1, r, "\xe2\x94\x82", Color::DarkGray);
        ctx.text(col_v - 1, r, "\xe2\x94\x82", Color::DarkGray);
    }

    // ── Pane sub-headers: ──┤ FRAGMENTS ├── / ──┤ BUILD ├── / ──┤ PREVIEW ├──
    self.draw_section_header(ctx, 3, "FRAGMENTS", col_p + 1, col_b - 1);
    self.draw_section_header(ctx, 3, "BUILD",     col_b + 1, col_v - 1);
    self.draw_section_header(ctx, 3, "PREVIEW",   col_v + 1, ctx.width() - 1);

    // ── Left pane: fragment palette ───────────────────────────────────────
    int yp = 5;
    FragmentKind last_kind = FragmentKind::Container;  // sentinel ≠ first real kind
    bool first = true;
    const auto& catalog = fragment_catalog();
    for (size_t i = 0; i < catalog.size(); ++i) {
        const auto& def = catalog[i];
        if (def.id == FragmentId::None) continue;

        if (first || def.kind != last_kind) {
            const char* kind_label =
                (def.kind == FragmentKind::Producer)    ? "PRODUCERS"   :
                (def.kind == FragmentKind::Transformer) ? "OPERATORS"   :
                                                          "CONTAINERS";
            ctx.text(col_p + 1, yp++, kind_label, Color::Yellow);
            last_kind = def.kind;
            first = false;
        }

        bool known = false;
        for (auto fid : self.player().learned_fragments)
            if (fid == def.id) { known = true; break; }

        bool sel = static_cast<int>(i) == self.compiler_palette_cursor();
        std::string line = (sel ? "\xe2\x96\xb8 " : "  ")
                         + std::string(def.display) + "  "
                         + std::to_string(def.exec_cost) + "/"
                         + std::to_string(def.heat_cost);
        Color color = !known ? Color::DarkGray
                    : sel    ? Color::Cyan
                             : Color::Default;
        ctx.text(col_p + 1, yp++, line, color);
    }

    ctx.text(col_p + 1, ctx.height() - 4, " \xe2\x86\x91\xe2\x86\x93 navigate   Enter: append",  Color::DarkGray);
    ctx.text(col_p + 1, ctx.height() - 3, " \xe2\x86\x90\xe2\x86\x92 +/- N    b/B enter/exit body", Color::DarkGray);
    ctx.text(col_p + 1, ctx.height() - 2, " c compile",                                          Color::DarkGray);

    // ── Middle pane: build ────────────────────────────────────────────────
    render_chain(ctx, col_b + 1, 5,
                 self.compiler_build(),
                 self.compiler_cursor_path(), 0);

    // ── Right pane: live preview ─────────────────────────────────────────
    auto cp = compile_program(self.compiler_build(), "");
    int yv = 5;
    ctx.text(col_v + 1, yv++, " Effect:", Color::Default);
    if (!cp.resolved.named_pattern.empty()) {
        ctx.text(col_v + 1, yv++, "  \xe2\x96\xba " + cp.resolved.named_pattern, Color::Green);
    }
    ctx.text(col_v + 1, yv++, "  damage " + std::to_string(cp.resolved.damage), Color::DarkGray);
    ctx.text(col_v + 1, yv++, "  radius " + std::to_string(cp.resolved.radius), Color::DarkGray);
    ctx.text(col_v + 1, yv++, "  ticks  " + std::to_string(cp.resolved.tick_count), Color::DarkGray);
    ++yv;
    ctx.text(col_v + 1, yv++, " Costs:", Color::Default);
    ctx.text(col_v + 1, yv++, "  exec " + std::to_string(cp.exec_cost), Color::DarkGray);
    ctx.text(col_v + 1, yv++, "  heat " + std::to_string(cp.heat_cost), Color::DarkGray);
    ctx.text(col_v + 1, yv++, "  ram  " + std::to_string(cp.ram_held), Color::DarkGray);
    if (!cp.patterns_lit.empty()) {
        ++yv;
        ctx.text(col_v + 1, yv++, " Patterns lit:", Color::Default);
        for (const auto& pat : cp.patterns_lit)
            ctx.text(col_v + 1, yv++, "  \xe2\x96\xba " + pat, Color::Green);
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
    if (self.compiler_build().empty()) {
        self.set_context_message("Compile: empty build.", 3);
        return;
    }

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
        self.set_context_message("Compile failed: no Program Disk.", 3);
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

    // Build a status line that includes the pattern (if any), so the on-tab
    // result is informative without forcing the player to read the world log.
    std::string status = "Compiled: " + saved.name;
    bool first_pattern = true;

    // Pattern discovery
    if (saved.compiled_program.has_value()) {
        for (const auto& pat : saved.compiled_program->patterns_lit) {
            bool already = false;
            for (const auto& d : self.player().discovered_patterns)
                if (d == pat) { already = true; break; }
            if (!already) {
                self.player().discovered_patterns.push_back(pat);
                game.log("Pattern discovered: " + pat + "!");
                status += first_pattern ? "  [Discovered: " : ", ";
                status += pat;
                first_pattern = false;
            }
        }
        if (!first_pattern) status += "]";
    }

    self.set_context_message(status + ".", 4);

    // Reset workbench
    self.compiler_build_mut().clear();
    self.compiler_cursor_path_mut().clear();
}

// Build a list of (inventory_index, item_pointer) pairs for compiled programs.
std::vector<std::pair<int, const Item*>> compiled_programs_in_inventory(PdaScreen& self) {
    std::vector<std::pair<int, const Item*>> out;
    const auto& inv = self.player().inventory.items;
    for (size_t i = 0; i < inv.size(); ++i) {
        if (inv[i].compiled_program.has_value())
            out.emplace_back(static_cast<int>(i), &inv[i]);
    }
    return out;
}

void draw_load_popup(PdaScreen& self, UIContext& ctx) {
    auto progs = compiled_programs_in_inventory(self);
    int w = 50;
    int h = std::max(8, static_cast<int>(progs.size()) + 6);
    int x = ctx.width() / 2 - w / 2;
    int y = ctx.height() / 2 - h / 2;

    // Clear background of the popup region with spaces
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            ctx.text(x + i, y + j, " ", Color::Default);
        }
    }

    // Border (top/bottom only — keep it minimal)
    self.draw_section_header(ctx, y, "LOAD PROGRAM", x + 1, x + w - 1);

    int slot = self.cyberdeck_slot_cursor();
    ctx.text(x + 2, y + 2, "Loading into slot " + std::to_string(slot + 1) + ".", Color::DarkGray);

    int cur = self.cyberdeck_load_popup_cursor();
    if (progs.empty()) {
        ctx.text(x + 2, y + 4, "  (no compiled programs in inventory)", Color::DarkGray);
    } else {
        for (int i = 0; i < static_cast<int>(progs.size()); ++i) {
            bool sel = (i == cur);
            std::string line = (sel ? "\xe2\x96\xb8 " : "  ") + progs[i].second->name;
            ctx.text(x + 2, y + 4 + i, line, sel ? Color::Cyan : Color::Default);
        }
    }

    ctx.text(x + 2, y + h - 2, " \xe2\x86\x91\xe2\x86\x93 select   Enter load   Esc cancel", Color::DarkGray);
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
    // Popup renders ON TOP of the Deck sub-screen.
    if (cyberdeck_load_popup_ && cyberdeck_subscreen_ == CyberdeckSubscreen::Deck) {
        draw_load_popup(*this, ctx);
    }
}

void PdaScreen::handle_cyberdeck_key(int key) {
    // Load-program popup intercepts everything while it's open.
    if (cyberdeck_load_popup_) {
        auto progs = compiled_programs_in_inventory(*this);
        if (key == KEY_UP) {
            if (cyberdeck_load_popup_cursor_ > 0) --cyberdeck_load_popup_cursor_;
            return;
        }
        if (key == KEY_DOWN) {
            if (cyberdeck_load_popup_cursor_ < static_cast<int>(progs.size()) - 1)
                ++cyberdeck_load_popup_cursor_;
            return;
        }
        if (key == '\n' || key == '\r') {
            // Load the selected compiled program into the cursor's slot.
            auto* deck_slot = player_->equipment.equipped_cyberdeck();
            if (deck_slot && *deck_slot && (*deck_slot)->deck &&
                cyberdeck_load_popup_cursor_ < static_cast<int>(progs.size())) {
                auto& deck = *(*deck_slot)->deck;
                if (cyberdeck_slot_cursor_ < deck.stats.slots) {
                    const Item* prog_item = progs[cyberdeck_load_popup_cursor_].second;
                    auto& sl = deck.loaded[cyberdeck_slot_cursor_];
                    // Carry both: def_id (for legacy items) and a copy of the
                    // compiled payload (drives render + fire regardless of
                    // whether the source item ever changes/leaves inventory).
                    sl.program_def_id = prog_item->item_def_id;
                    if (prog_item->compiled_program.has_value()) {
                        sl.compiled = *prog_item->compiled_program;
                    } else {
                        sl.compiled.reset();
                    }
                    std::string msg = "Loaded " + prog_item->name + " into slot "
                                    + std::to_string(cyberdeck_slot_cursor_ + 1) + ".";
                    if (game_) game_->log(msg);
                    set_context_message(msg, 3);
                }
            }
            cyberdeck_load_popup_close();
            return;
        }
        if (key == 27) {  // Esc
            cyberdeck_load_popup_close();
            return;
        }
        return;
    }

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

    // Deck sub-screen — slot navigation + Space to open load popup
    if (cyberdeck_subscreen_ == CyberdeckSubscreen::Deck) {
        auto* deck_slot = player_->equipment.equipped_cyberdeck();
        int max_slot = 0;
        if (deck_slot && *deck_slot && (*deck_slot)->deck)
            max_slot = (*deck_slot)->deck->stats.slots;
        switch (key) {
            case KEY_UP:
                if (cyberdeck_slot_cursor_ > 0) --cyberdeck_slot_cursor_;
                break;
            case KEY_DOWN:
                if (cyberdeck_slot_cursor_ < max_slot - 1) ++cyberdeck_slot_cursor_;
                break;
            case ' ':
                if (max_slot > 0) cyberdeck_load_popup_open();
                break;
            case 'u':
                // Unload the program in the current slot.
                if (deck_slot && *deck_slot && (*deck_slot)->deck &&
                    cyberdeck_slot_cursor_ < max_slot) {
                    auto& sl = (*deck_slot)->deck->loaded[cyberdeck_slot_cursor_];
                    if (!slot_is_empty(sl)) {
                        sl.program_def_id = 0;
                        sl.compiled.reset();
                        set_context_message(
                            "Unloaded slot " + std::to_string(cyberdeck_slot_cursor_ + 1) + ".", 3);
                    }
                }
                break;
            default: break;
        }
        return;
    }

    if (cyberdeck_show_patterns_overlay_) return;

    const auto& palette = fragment_catalog();
    // Helpers — palette cursor must always sit on a real fragment (skip None).
    auto palette_step = [&](int dir) {
        int n = static_cast<int>(palette.size());
        for (int steps = 0; steps < n; ++steps) {
            int next = compiler_palette_cursor_ + dir;
            if (next < 0 || next >= n) return;
            compiler_palette_cursor_ = next;
            if (palette[compiler_palette_cursor_].id != FragmentId::None) return;
        }
    };
    switch (key) {
        case KEY_UP:
            palette_step(-1);
            break;
        case KEY_DOWN:
            palette_step(+1);
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
        case '=':
        case KEY_RIGHT: {
            ProgramNode* n = node_at_cursor(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && def->takes_param && n->param < def->max_n)
                    ++n->param;
            }
            break;
        }
        case '-':
        case '_':
        case KEY_LEFT: {
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

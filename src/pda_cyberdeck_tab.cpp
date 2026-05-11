#include "astra/pda_screen.h"

#include "astra/ability_bar.h"
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

// Stylized "chipset" ornament. Verbatim template from /tmp/cyberdeck.
// 17 wide × 9 tall. Cogs render in magenta with the centre cog in gold;
// solid block tabs render in cyan; bullet dots in gold; frame in magenta.
//
//   ╔───────█───█───╗
//   │ ┌─•─┘ │ ┌─│─┐ │
//   ▉───⚙─┐ ⚙─┐ ⚙ • │
//   │ └─┌─+─┘ +─┘ └─│
//   ▉───⚙─┐ ⚙ └─⚙───▉
//   │─┐ ┌─+ ┌─+─┘─┐ │
//   │ • ⚙ └─⚙ └─⚙───▉
//   │ └─│─┘ │ ┌─•─┘ │
//   ╚───█───█───────╝
void draw_chip_ornament(UIContext& ctx, int x0, int y0) {
    static const std::vector<std::string> rows = {
        "\xe2\x95\x94\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x88\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x88\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\x97",
        "\xe2\x94\x82 \xe2\x94\x8c\xe2\x94\x80\xe2\x80\xa2\xe2\x94\x80\xe2\x94\x98 \xe2\x94\x82 \xe2\x94\x8c\xe2\x94\x80\xe2\x94\x82\xe2\x94\x80\xe2\x94\x90 \xe2\x94\x82",
        "\xe2\x96\x89\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x9a\x99\xe2\x94\x80\xe2\x94\x90 \xe2\x9a\x99\xe2\x94\x80\xe2\x94\x90 \xe2\x9a\x99 \xe2\x80\xa2 \xe2\x94\x82",
        "\xe2\x94\x82 \xe2\x94\x94\xe2\x94\x80\xe2\x94\x8c\xe2\x94\x80+\xe2\x94\x80\xe2\x94\x98 +\xe2\x94\x80\xe2\x94\x98 \xe2\x94\x94\xe2\x94\x80\xe2\x94\x82",
        "\xe2\x96\x89\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x9a\x99\xe2\x94\x80\xe2\x94\x90 \xe2\x9a\x99 \xe2\x94\x94\xe2\x94\x80\xe2\x9a\x99\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x89",
        "\xe2\x94\x82\xe2\x94\x80\xe2\x94\x90 \xe2\x94\x8c\xe2\x94\x80+ \xe2\x94\x8c\xe2\x94\x80+\xe2\x94\x80\xe2\x94\x98\xe2\x94\x80\xe2\x94\x90 \xe2\x94\x82",
        "\xe2\x94\x82 \xe2\x80\xa2 \xe2\x9a\x99 \xe2\x94\x94\xe2\x94\x80\xe2\x9a\x99 \xe2\x94\x94\xe2\x94\x80\xe2\x9a\x99\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x89",
        "\xe2\x94\x82 \xe2\x94\x94\xe2\x94\x80\xe2\x94\x82\xe2\x94\x80\xe2\x94\x98 \xe2\x94\x82 \xe2\x94\x8c\xe2\x94\x80\xe2\x80\xa2\xe2\x94\x80\xe2\x94\x98 \xe2\x94\x82",
        "\xe2\x95\x9a\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x88\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x96\x88\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x95\x9d",
    };

    const Color frame  = Color::Magenta;
    const Color cog    = Color::Magenta;
    const Color center = Color::BrightYellow;
    const Color block  = Color::Cyan;
    const Color dot    = Color::BrightYellow;

    int cog_count = 0;
    for (int row = 0; row < static_cast<int>(rows.size()); ++row) {
        const std::string& line = rows[row];
        int col = 0;
        for (size_t i = 0; i < line.size(); ) {
            unsigned char c0 = static_cast<unsigned char>(line[i]);
            int len = 1;
            uint32_t cp = c0;
            if      ((c0 & 0x80) == 0x00) { len = 1; cp = c0; }
            else if ((c0 & 0xE0) == 0xC0) { len = 2; cp = c0 & 0x1F; }
            else if ((c0 & 0xF0) == 0xE0) { len = 3; cp = c0 & 0x0F; }
            else if ((c0 & 0xF8) == 0xF0) { len = 4; cp = c0 & 0x07; }
            for (int k = 1; k < len; ++k) {
                cp = (cp << 6) | (static_cast<unsigned char>(line[i + k]) & 0x3F);
            }

            // Spaces leave the background untouched.
            if (cp == ' ') { ++col; i += len; continue; }

            Color color = frame;
            if (cp == 0x2699) {                          // ⚙
                color = (cog_count == 4) ? center : cog;
                ++cog_count;
            } else if (cp == 0x2588 || cp == 0x2589) {   // █ ▉
                color = block;
            } else if (cp == 0x2022) {                   // •
                color = dot;
            }

            ctx.text(x0 + col, y0 + row, line.substr(i, len), color);
            i += len;
            ++col;
        }
    }
}

// One cartridge "card" for a cyberdeck slot. 11 wide × 3 tall frame plus a
// 1-row name label and 1-row cost subline below. Border colour signals state:
// yellow when selected, cyan when loaded, dark-gray when empty.
void draw_slot_card(UIContext& ctx, int x, int y, int idx,
                    bool loaded, const std::string& name,
                    int heat_cost, bool selected) {
    constexpr int w = 11;
    Color border = selected ? Color::Yellow
                 : loaded   ? Color::Cyan
                            : Color::DarkGray;

    // Top border — ┌──┤ N ├──┐ with the slot number embedded.
    ctx.text(x,     y, BoxDraw::TL,                     border);
    ctx.text(x + 1, y, "\xe2\x94\x80\xe2\x94\x80",      border);   // ──
    ctx.text(x + 3, y, "\xe2\x94\xa4",                  border);   // ┤
    ctx.text(x + 4, y, " " + std::to_string(idx + 1) + " ",
             selected ? Color::Yellow : Color::White);
    ctx.text(x + 7, y, "\xe2\x94\x9c",                  border);   // ├
    ctx.text(x + 8, y, "\xe2\x94\x80\xe2\x94\x80",      border);
    ctx.text(x + w - 1, y, BoxDraw::TR,                 border);

    // Content row — side rails + centered glyph.
    ctx.text(x,         y + 1, BoxDraw::V, border);
    int mid = x + w / 2;
    if (loaded) {
        ctx.text(mid, y + 1, "\xe2\x96\xa3", Color::Cyan);          // ▣
    } else {
        ctx.text(mid, y + 1, "\xc2\xb7", Color::DarkGray);          // ·
    }
    ctx.text(x + w - 1, y + 1, BoxDraw::V, border);

    // Bottom border.
    ctx.text(x, y + 2, BoxDraw::BL, border);
    for (int i = 1; i < w - 1; ++i)
        ctx.text(x + i, y + 2, BoxDraw::H, border);
    ctx.text(x + w - 1, y + 2, BoxDraw::BR, border);

    // Name label — truncate to fit the card width.
    std::string label = loaded ? name : std::string("(empty)");
    constexpr int max_label = w;
    if (static_cast<int>(label.size()) > max_label) {
        label = label.substr(0, max_label - 2) + "..";
    }
    int label_x = x + (w - static_cast<int>(label.size())) / 2;
    ctx.text(label_x, y + 3, label, loaded ? Color::Default : Color::DarkGray);

    // Heat-cost subline — only meaningful for loaded programs with a known cost.
    if (loaded && heat_cost > 0) {
        std::string cost = std::to_string(heat_cost) + " heat";
        int cost_x = x + (w - static_cast<int>(cost.size())) / 2;
        ctx.text(cost_x, y + 4, cost, Color::Yellow);
    }
}

// Render a horizontal bar like ▮▮▮▮▯▯▯▯▯▯▯▯ <cur>/<max>.
// Filled cells use `fill_color`, empty cells stay DarkGray; the numeric
// "cur/max" tail right-pads `cur` so the slash always lines up.
void draw_bar(UIContext& ctx, int x, int y, int cur, int max,
              int width, Color fill_color) {
    int filled = 0;
    if (max > 0) {
        filled = (cur * width + max / 2) / max;
        if (filled < 0) filled = 0;
        if (filled > width) filled = width;
    }
    int cx = x;
    for (int i = 0; i < width; ++i) {
        const char* glyph = (i < filled) ? "\xe2\x96\xae"   // ▮
                                         : "\xe2\x96\xaf";  // ▯
        ctx.text(cx, y, glyph, (i < filled) ? fill_color : Color::DarkGray);
        cx += 1;
    }
    // Right-align cur within max's digit width so the slash aligns visually
    // across consecutive rows of bars.
    auto num_digits = [](int v) {
        int d = 1;
        for (int n = v / 10; n > 0; n /= 10) ++d;
        return d;
    };
    int max_w = num_digits(std::max(1, max));
    std::string cur_s = std::to_string(cur);
    while (static_cast<int>(cur_s.size()) < max_w) cur_s.insert(0, " ");
    ctx.text(cx + 1, y, cur_s + "/" + std::to_string(max), Color::Default);
}

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

    // ── Header: ──┤ DECK ├── (spans full width now)
    self.draw_section_header(ctx, 1, "DECK", 1, ctx.width() - 1);

    ctx.text(2, 2, deck_name, Color::White);

    // Stat rows — RAM/HEAT get bars, COOLING/STEALTH stay textual.
    int y = 4;
    const int label_w = 8;          // "RAM     " / "HEAT    " / "COOLING " / "STEALTH "
    const int bar_w   = 12;         // bar cells
    const int bar_x   = 2 + label_w;

    auto label = [&](const char* text) {
        ctx.text(2, y, text, Color::DarkGray);
    };

    label("RAM");
    draw_bar(ctx, bar_x, y, deck.ram_current, deck.stats.ram_max,
             bar_w, Color::Cyan);
    ++y;

    label("HEAT");
    draw_bar(ctx, bar_x, y, deck.heat_current, deck.stats.heat_cap,
             bar_w, Color::Red);
    ++y;

    label("COOLING");
    ctx.text(bar_x, y, std::to_string(deck.stats.cooling_rate) + "/turn",
             Color::Default);
    ++y;

    label("STEALTH");
    ctx.text(bar_x, y, "+" + std::to_string(deck.stats.stealth),
             Color::Default);
    ++y;

    // ── Sub-header: SLOTS ──┤
    self.draw_section_header(ctx, y++, "SLOTS", 1, ctx.width() - 1);

    // ── Decorative chipset ornament, centered just above the slot row ───
    constexpr int ornament_w = 17;
    constexpr int ornament_h = 9;
    int ornament_x = (ctx.width() - ornament_w) / 2;
    if (ornament_x < 0) ornament_x = 0;
    draw_chip_ornament(ctx, ornament_x, y);
    y += ornament_h;

    // Horizontal cartridge row, centered on the panel.
    constexpr int card_w = 11;
    constexpr int card_gap = 2;
    const int n_slots = deck.stats.slots;
    const int total_w = n_slots * card_w + std::max(0, n_slots - 1) * card_gap;
    const int row_x  = (ctx.width() - total_w) / 2;
    const int row_y  = y + 1;

    int slot_cursor = self.cyberdeck_slot_cursor();
    for (int i = 0; i < n_slots; ++i) {
        const auto& sl = deck.loaded[i];
        std::string name;
        int heat_cost = 0;
        if (sl.compiled.has_value()) {
            name = sl.compiled->name;
            heat_cost = sl.compiled->heat_cost;
        } else if (sl.program_def_id != 0) {
            Item probe = build_by_def_id(sl.program_def_id);
            name = probe.name;
        }
        bool loaded = !name.empty();
        int card_x = row_x + i * (card_w + card_gap);
        draw_slot_card(ctx, card_x, row_y, i,
                       loaded, name, heat_cost,
                       i == slot_cursor);
    }
}

// ── Compiler helpers ──────────────────────────────────────────────────────

// One edit position in the build tree — either a gap (blue insertion line) or
// on a node (delete/edit target). Linear ordering matches depth-first walk.
struct EditPos {
    std::vector<int> path;   // path to the chain (body) this position lives in
    int slot = 0;            // 0..2*chain.size(); even = gap, odd = on-node
};

bool pos_is_gap(int slot) { return (slot % 2) == 0; }
int  pos_node_index(int slot) { return (slot - 1) / 2; }
int  pos_gap_index(int slot) { return slot / 2; }

// Flatten the tree into the depth-first ordered list of edit positions.
void flatten_positions(const std::vector<ProgramNode>& chain,
                       std::vector<int> path,
                       std::vector<EditPos>& out) {
    int n = static_cast<int>(chain.size());
    // gap 0 (above first node)
    out.push_back({path, 0});
    for (int i = 0; i < n; ++i) {
        // on node
        out.push_back({path, 2 * i + 1});
        // descend into container body
        const FragmentDef* def = find_fragment(chain[i].fragment);
        if (def && def->kind == FragmentKind::Container) {
            auto child = path;
            child.push_back(i);
            flatten_positions(chain[i].body, child, out);
        }
        // gap after this node
        out.push_back({path, 2 * (i + 1)});
    }
}

// Walk the tree following `path` and return a mutable reference to the chain.
std::vector<ProgramNode>* chain_at_path(std::vector<ProgramNode>& root,
                                        const std::vector<int>& path) {
    std::vector<ProgramNode>* chain = &root;
    for (int idx : path) {
        if (idx < 0 || idx >= static_cast<int>(chain->size())) return nullptr;
        chain = &(*chain)[idx].body;
    }
    return chain;
}

const std::vector<ProgramNode>* chain_at_path(const std::vector<ProgramNode>& root,
                                              const std::vector<int>& path) {
    const std::vector<ProgramNode>* chain = &root;
    for (int idx : path) {
        if (idx < 0 || idx >= static_cast<int>(chain->size())) return nullptr;
        chain = &(*chain)[idx].body;
    }
    return chain;
}

// Find current cursor index in the flattened list (0 if no match).
int locate_cursor(const std::vector<EditPos>& flat,
                  const std::vector<int>& path, int slot) {
    for (int i = 0; i < static_cast<int>(flat.size()); ++i) {
        if (flat[i].path == path && flat[i].slot == slot) return i;
    }
    return 0;
}

// Render a chain. Rendering rules (per UX spec):
//   - Non-cursor inter-node gaps (between two nodes): ↓ arrow.
//   - Non-cursor head/tail gaps (above first / below last): nothing.
//   - Cursor at a gap, editor focus: "▸ ───────" in cyan.
//   - Cursor at a gap, palette focus: "▸" alone in cyan (no line).
//   - Cursor on a node: "▸ [NAME]" in cyan (editor focus). Outside editor
//     focus the cursor is normally snapped to the trailing gap by the
//     mode toggle, so on-node-in-palette-mode is rare; render same way.
int render_chain_edit(UIContext& ctx, int x, int y,
                      const std::vector<ProgramNode>& chain,
                      const std::vector<int>& self_path,
                      const std::vector<int>& cursor_path,
                      int cursor_slot,
                      bool build_focus) {
    bool active = (self_path == cursor_path);
    int n = static_cast<int>(chain.size());

    auto draw_gap = [&](int gap_index) {
        bool is_cursor = active && (cursor_slot == 2 * gap_index);
        bool is_inter_node = gap_index > 0 && gap_index < n;
        if (is_cursor) {
            if (build_focus) {
                // ▸ ──────── (cyan)
                ctx.text(x, y,
                         "\xe2\x96\xb8 \xe2\x94\x80\xe2\x94\x80\xe2\x94\x80"
                         "\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80\xe2\x94\x80",
                         Color::Cyan);
            } else {
                ctx.text(x, y, "\xe2\x96\xb8", Color::Cyan);
            }
            ++y;
        } else if (is_inter_node) {
            ctx.text(x + 4, y, "\xe2\x86\x93", Color::DarkGray);   // ↓
            ++y;
        }
        // else: head/tail gap, not cursor → don't render anything (saves a row)
    };

    // Head gap (above first node)
    draw_gap(0);

    for (int i = 0; i < n; ++i) {
        const auto& node = chain[i];
        const FragmentDef* def = find_fragment(node.fragment);
        bool on_node = active && (cursor_slot == 2 * i + 1);
        std::string marker = on_node ? "\xe2\x96\xb8 " : "  ";  // ▸
        Color highlight = on_node ? Color::Cyan : Color::Default;

        if (def && def->kind == FragmentKind::Container) {
            // Split-render the header so the box-draw pipes are purple while
            // the body text uses the on/off highlight color.
            //   marker + ┌─ + " TICK(N) " + ─
            const Color pipe_color = Color::Magenta;
            int cx = x;
            ctx.text(cx, y, marker, highlight);
            // Advance by the marker's CELL width, not its byte length —
            // "▸ " is 4 bytes but still 2 cells. Using marker.size() shifted
            // the ┌ (and the │ closer underneath) right by 2 cells whenever
            // the cursor was on the container header.
            cx += 2;
            const int pipe_col = cx;   // column of ┌ / │ / └
            std::string pipe_open = std::string(BoxDraw::TL) + "\xe2\x94\x80";  // ┌─
            ctx.text(cx, y, pipe_open, pipe_color);
            cx += 2;
            std::string label = std::string(" ") + def->display
                              + "(" + std::to_string(node.param) + ") ";
            ctx.text(cx, y, label, on_node ? Color::Cyan : Color::White);
            cx += static_cast<int>(label.size());
            ctx.text(cx, y, "\xe2\x94\x80", pipe_color);   // ─
            ++y;

            // Recurse with +4 instead of +2 so the body's leftmost column
            // stays free for the container's vertical │ closer.
            int body_start_y = y;
            std::vector<int> child = self_path;
            child.push_back(i);
            y = render_chain_edit(ctx, x + 4, y, node.body,
                                  child, cursor_path, cursor_slot, build_focus);

            // Draw left-side │ between header and footer (extends the box
            // drawing on the LEFT all the way from ┌ down to └).
            for (int vy = body_start_y; vy < y; ++vy) {
                ctx.text(pipe_col, vy, "\xe2\x94\x82", pipe_color);   // │
            }

            std::string footer = "  " + std::string(BoxDraw::BL) + "\xe2\x94\x80\xe2\x94\x80";
            ctx.text(x, y++, footer, pipe_color);
        } else {
            std::string label;
            if (def) {
                if (def->takes_param)
                    label = marker + "[" + std::string(def->display)
                          + "(" + std::to_string(node.param) + ")]";
                else
                    label = marker + "[" + std::string(def->display) + "]";
            } else {
                label = marker + "[???]";
            }
            ctx.text(x, y++, label, highlight);
        }

        // Gap below this node — either inter-node ↓ or tail gap
        draw_gap(i + 1);
    }

    return y;
}

void draw_compiler_subscreen(PdaScreen& self, UIContext& ctx) {
    int ceiling = max_program_fragments(self.player());

    // Locked-out splash: the Compiler is gated behind Programming I. Until
    // the player learns that skill the workspace is empty by design.
    if (ceiling == 0) {
        int cy = ctx.height() / 2 - 2;
        const char* title = "-- COMPILER LOCKED --";
        int title_cols = 0;
        for (const char* p = title; *p; ++p) ++title_cols;
        ctx.text(ctx.width() / 2 - title_cols / 2, cy, title, Color::DarkGray);
        ctx.text(4, cy + 2,
                 "Learn the Programming I skill in the Skills tab to unlock the Cyberdeck Compiler.",
                 Color::DarkGray);
        ctx.text(4, cy + 4,
                 "Programming I costs 0 SP and is available the moment you learn Cat_Hacking.",
                 Color::DarkGray);
        return;
    }

    // Layout: narrow side panes (1/6 each = 1/3 combined), wide BUILD in
    // the middle (2/3) so the program tree has room to breathe.
    int sixth  = ctx.width() / 6;
    int col_p  = 0;
    int col_b  = sixth;
    int col_v  = ctx.width() - sixth;

    // Vertical dividers between the three panes
    for (int r = 1; r < ctx.height() - 1; ++r) {
        ctx.text(col_b - 1, r, "\xe2\x94\x82", Color::DarkGray);
        ctx.text(col_v - 1, r, "\xe2\x94\x82", Color::DarkGray);
    }

    // ── Pane sub-headers: ──┤ FRAGMENTS ├── / ──┤ BUILD ├── / ──┤ PREVIEW ├──
    const int header_y = 1;
    self.draw_section_header(ctx, header_y, "FRAGMENTS", col_p + 1, col_b - 1);
    self.draw_section_header(ctx, header_y, "BUILD",     col_b + 1, col_v - 1);
    self.draw_section_header(ctx, header_y, "PREVIEW",   col_v + 1, ctx.width() - 1);

    // Right-aligned ┤ Max Fragments: N ├ overlay on the BUILD header.
    {
        std::string lbl  = " Max Fragments: ";
        std::string val  = std::to_string(ceiling);
        std::string tail = " ";
        int content_cells = static_cast<int>(lbl.size() + val.size() + tail.size());
        // ┤ + content + ├ takes (content_cells + 2) cells. Place flush with
        // the pane's right edge (col_v - 1).
        int label_x = (col_v - 1) - 1 - content_cells;
        ctx.text(label_x, header_y, BoxDraw::RT, Color::DarkGray);
        ctx.text(label_x + 1, header_y, lbl, Color::White);
        ctx.text(label_x + 1 + static_cast<int>(lbl.size()), header_y, val, Color::Green);
        ctx.text(label_x + 1 + static_cast<int>(lbl.size() + val.size()),
                 header_y, tail, Color::Default);
        ctx.text(label_x + 1 + content_cells, header_y, BoxDraw::LT, Color::DarkGray);
    }

    // ── Left pane: fragment palette ───────────────────────────────────────
    int yp = 3;
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

        // Split-render arrow / name / costs so each can highlight independently.
        // Arrow + name pop in BrightYellow when selected; costs render in
        // standard Yellow so the name remains the prominent element.
        const char* marker = sel ? "\xe2\x96\xb8 " : "  ";   // ▸
        ctx.text(col_p + 1, yp, marker,
                 sel ? Color::BrightYellow : Color::Default);

        std::string name  = std::string(def.display);
        std::string costs = "  " + std::to_string(def.exec_cost) + "/"
                          + std::to_string(def.heat_cost);

        Color name_color  = !known ? Color::DarkGray
                          : sel    ? Color::BrightYellow
                                   : Color::Default;
        Color costs_color = !known ? Color::DarkGray
                          : sel    ? Color::Yellow
                                   : Color::Default;

        int name_x = col_p + 3;
        ctx.text(name_x, yp, name, name_color);
        ctx.text(name_x + static_cast<int>(name.size()), yp, costs, costs_color);
        ++yp;
    }

    // ── Middle pane: build ────────────────────────────────────────────────
    bool build_focus = self.compiler_focus() == PdaScreen::CompilerFocus::Build;
    render_chain_edit(ctx, col_b + 1, 3,
                      self.compiler_build(),
                      /*self_path=*/{},
                      self.build_cursor_path(),
                      self.build_cursor_slot(),
                      build_focus);


    // ── Right pane: live preview ─────────────────────────────────────────
    auto cp = compile_program(self.compiler_build(), "");
    int yv = 3;
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

// Return pointer to the node the cursor sits ON (or nullptr if cursor is on a gap).
ProgramNode* cursor_on_node(PdaScreen& self) {
    if (pos_is_gap(self.build_cursor_slot())) return nullptr;
    auto* chain = chain_at_path(self.compiler_build_mut(), self.build_cursor_path());
    if (!chain) return nullptr;
    int idx = pos_node_index(self.build_cursor_slot());
    if (idx < 0 || idx >= static_cast<int>(chain->size())) return nullptr;
    return &(*chain)[idx];
}

// Resolve which parameterized node the [+/-] keys should adjust:
//   1) Cursor sits ON a parameterized node → that node.
//   2) Otherwise → walk up the path to the innermost enclosing parameterized
//      container (TICK / LOOP). This lets +/- adjust the container's N even
//      when the cursor is currently inside the container body.
ProgramNode* cursor_param_target(PdaScreen& self) {
    if (ProgramNode* n = cursor_on_node(self)) {
        const FragmentDef* def = find_fragment(n->fragment);
        if (def && def->takes_param) return n;
    }
    auto path = self.build_cursor_path();
    while (!path.empty()) {
        std::vector<int> parent_path(path.begin(), path.end() - 1);
        auto* parent = chain_at_path(self.compiler_build_mut(), parent_path);
        if (parent) {
            int idx = path.back();
            if (idx >= 0 && idx < static_cast<int>(parent->size())) {
                ProgramNode& candidate = (*parent)[idx];
                const FragmentDef* def = find_fragment(candidate.fragment);
                if (def && def->takes_param) return &candidate;
            }
        }
        path.pop_back();
    }
    return nullptr;
}

// Insert a fragment at the cursor. Gap → insert at gap_index; on-node →
// insert AFTER the current node. Cursor advances to the gap below the new
// node so the user can keep "typing" forward — UNLESS the new node is a
// container, in which case the cursor descends into its (empty) body so
// the next append lands inside the loop/tick.
void insert_at_cursor(PdaScreen& self, FragmentId id) {
    int ceiling = max_program_fragments(self.player());
    if (ceiling == 0) return;

    auto* chain = chain_at_path(self.compiler_build_mut(), self.build_cursor_path());
    if (!chain) return;

    // Top-level ceiling check (limits fragments per top-level program).
    if (self.build_cursor_path().empty()
        && static_cast<int>(chain->size()) >= ceiling) {
        self.set_context_message("Program ceiling reached.", 3);
        return;
    }

    const FragmentDef* def = find_fragment(id);
    if (!def) return;
    ProgramNode n;
    n.fragment = id;
    n.param    = def->takes_param ? def->default_n : 0;

    int insert_at;
    if (pos_is_gap(self.build_cursor_slot())) {
        insert_at = pos_gap_index(self.build_cursor_slot());
    } else {
        insert_at = pos_node_index(self.build_cursor_slot()) + 1;
    }
    chain->insert(chain->begin() + insert_at, std::move(n));

    if (def->kind == FragmentKind::Container) {
        // Descend into the new container's empty body so subsequent appends
        // land INSIDE it (TICK / LOOP).
        self.build_cursor_path_mut().push_back(insert_at);
        self.build_cursor_slot_mut() = 0;
    } else {
        // Advance cursor to the gap BELOW the new node in this chain.
        self.build_cursor_slot_mut() = 2 * (insert_at + 1);
    }
}

// Delete according to cursor position.
//   Gap K > 0  → delete node at K-1 in the chain at path; cursor stays at gap K-1.
//   Gap K == 0 → no-op (no node above).
//   On-node K  → delete node at K; cursor moves to gap K.
void delete_at_cursor(PdaScreen& self) {
    auto* chain = chain_at_path(self.compiler_build_mut(), self.build_cursor_path());
    if (!chain) return;

    if (pos_is_gap(self.build_cursor_slot())) {
        int g = pos_gap_index(self.build_cursor_slot());
        if (g == 0) return;
        chain->erase(chain->begin() + (g - 1));
        self.build_cursor_slot_mut() = 2 * (g - 1);
    } else {
        int n = pos_node_index(self.build_cursor_slot());
        if (n < 0 || n >= static_cast<int>(chain->size())) return;
        chain->erase(chain->begin() + n);
        self.build_cursor_slot_mut() = 2 * n;
    }
}

// Clamp the cursor into the tree after structural mutation.
void clamp_cursor(PdaScreen& self) {
    auto& path = self.build_cursor_path_mut();
    while (!path.empty()) {
        auto* parent = chain_at_path(self.compiler_build_mut(),
                                     std::vector<int>(path.begin(), path.end() - 1));
        if (!parent || path.back() < 0
            || path.back() >= static_cast<int>(parent->size())) {
            path.pop_back();
            continue;
        }
        break;
    }
    auto* chain = chain_at_path(self.compiler_build_mut(), path);
    if (!chain) {
        self.build_cursor_slot_mut() = 0;
        return;
    }
    int max_slot = 2 * static_cast<int>(chain->size());
    if (self.build_cursor_slot() < 0)         self.build_cursor_slot_mut() = 0;
    if (self.build_cursor_slot() > max_slot)  self.build_cursor_slot_mut() = max_slot;
}

// Begin the compile flow: pre-flight checks, then open the name-prompt
// popup with an auto-generated default. Actual disk consume + item
// creation happens in confirm_compile() when the user hits Enter.
void begin_compile_prompt(PdaScreen& self, Game& game) {
    if (self.compiler_build().empty()) {
        self.set_context_message("Compile: empty build.", 3);
        return;
    }
    // Disk check up front — don't open the prompt if compile would fail.
    int disks = 0;
    for (const auto& it : self.player().inventory.items) {
        if (it.item_def_id == ITEM_PROGRAM_DISK) disks += it.stack_count;
    }
    if (disks <= 0) {
        self.set_context_message("Compile failed: no Cipher Disk.", 3);
        game.log("No Cipher Disk in inventory.");
        return;
    }
    self.cyberdeck_compile_prompt_open(auto_name(self.compiler_build()));
}

void confirm_compile(PdaScreen& self, Game& game, const std::string& name) {
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
        self.set_context_message("Compile failed: no Cipher Disk.", 3);
        game.log("No Cipher Disk in inventory.");
        return;
    }

    // Consume one disk
    auto& disk = inv[disk_idx];
    if (disk.stack_count > 1)
        disk.stack_count -= 1;
    else
        inv.erase(inv.begin() + disk_idx);

    // Compile — use the player-supplied name (falls back to auto_name if blank).
    std::string final_name = name.empty() ? auto_name(self.compiler_build()) : name;
    auto cp = compile_program(self.compiler_build(), final_name);

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
    self.build_cursor_path_mut().clear();
    self.build_cursor_slot_mut() = 0;
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

    // Full pipe-style border using the same box-draw glyphs as the section
    // headers. Top row uses the section-header glyphs so the title reads
    // ──┤ LOAD PROGRAM ├──; the rest of the frame uses BoxDraw chars.
    self.draw_section_header(ctx, y, "LOAD PROGRAM", x, x + w);
    // Replace the section-header's leading "── " with a real top-left corner
    // and the trailing tail with a real top-right corner.
    ctx.text(x,         y, BoxDraw::TL, Color::DarkGray);   // ┌
    ctx.text(x + w - 1, y, BoxDraw::TR, Color::DarkGray);   // ┐
    // Side rails
    for (int j = 1; j < h - 1; ++j) {
        ctx.text(x,         y + j, BoxDraw::V, Color::DarkGray);   // │
        ctx.text(x + w - 1, y + j, BoxDraw::V, Color::DarkGray);
    }
    // Bottom
    ctx.text(x, y + h - 1, BoxDraw::BL, Color::DarkGray);   // └
    for (int i = 1; i < w - 1; ++i) {
        ctx.text(x + i, y + h - 1, BoxDraw::H, Color::DarkGray);   // ─
    }
    ctx.text(x + w - 1, y + h - 1, BoxDraw::BR, Color::DarkGray);  // ┘

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

void draw_compile_prompt(PdaScreen& self, UIContext& ctx) {
    int w = 60;
    int h = 8;
    int x = ctx.width() / 2 - w / 2;
    int y = ctx.height() / 2 - h / 2;

    // Background
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            ctx.text(x + i, y + j, " ", Color::Default);
        }
    }

    // Full pipe-style border (same shape as the LOAD PROGRAM popup).
    self.draw_section_header(ctx, y, "NAME PROGRAM", x, x + w);
    ctx.text(x,         y, BoxDraw::TL, Color::DarkGray);
    ctx.text(x + w - 1, y, BoxDraw::TR, Color::DarkGray);
    for (int j = 1; j < h - 1; ++j) {
        ctx.text(x,         y + j, BoxDraw::V, Color::DarkGray);
        ctx.text(x + w - 1, y + j, BoxDraw::V, Color::DarkGray);
    }
    ctx.text(x, y + h - 1, BoxDraw::BL, Color::DarkGray);
    for (int i = 1; i < w - 1; ++i) {
        ctx.text(x + i, y + h - 1, BoxDraw::H, Color::DarkGray);
    }
    ctx.text(x + w - 1, y + h - 1, BoxDraw::BR, Color::DarkGray);

    ctx.text(x + 2, y + 2, "Name your compiled program:", Color::DarkGray);

    // Input field with a trailing █ caret so the user sees where typing goes.
    std::string field = self.cyberdeck_compile_name() + "\xe2\x96\x88";
    ctx.text(x + 2, y + 4, " " + field, Color::Cyan);

    ctx.text(x + 2, y + h - 2, " Enter: compile   Backspace: erase   Esc: cancel", Color::DarkGray);
}

// Color a fragment's kind label / accent color, matching the palette legend.
Color color_for_kind(FragmentKind k) {
    switch (k) {
        case FragmentKind::Producer:    return Color::Yellow;
        case FragmentKind::Transformer: return Color::Cyan;
        case FragmentKind::Container:   return Color::Magenta;
    }
    return Color::Default;
}

const char* kind_name(FragmentKind k) {
    switch (k) {
        case FragmentKind::Producer:    return "PRODUCER";
        case FragmentKind::Transformer: return "OPERATOR";
        case FragmentKind::Container:   return "CONTAINER";
    }
    return "";
}

void draw_fragment_help_popup(PdaScreen& self, UIContext& ctx) {
    const FragmentDef* def = find_fragment(self.cyberdeck_help_fragment());
    if (!def) return;

    int w = 56;
    int h = def->takes_param ? 13 : 11;
    int x = ctx.width()  / 2 - w / 2;
    int y = ctx.height() / 2 - h / 2;

    // Clear background of the popup region.
    for (int j = 0; j < h; ++j) {
        for (int i = 0; i < w; ++i) {
            ctx.text(x + i, y + j, " ", Color::Default);
        }
    }

    // Bordered frame with a titled top edge.
    self.draw_section_header(ctx, y, "FRAGMENT", x, x + w);
    ctx.text(x,         y, BoxDraw::TL, Color::DarkGray);
    ctx.text(x + w - 1, y, BoxDraw::TR, Color::DarkGray);
    for (int j = 1; j < h - 1; ++j) {
        ctx.text(x,         y + j, BoxDraw::V, Color::DarkGray);
        ctx.text(x + w - 1, y + j, BoxDraw::V, Color::DarkGray);
    }
    ctx.text(x, y + h - 1, BoxDraw::BL, Color::DarkGray);
    for (int i = 1; i < w - 1; ++i) {
        ctx.text(x + i, y + h - 1, BoxDraw::H, Color::DarkGray);
    }
    ctx.text(x + w - 1, y + h - 1, BoxDraw::BR, Color::DarkGray);

    int px = x + 2;
    int py = y + 2;

    // Title line: [DISPLAY]   PRODUCER/OPERATOR/CONTAINER
    Color accent = color_for_kind(def->kind);
    std::string title = std::string("[") + def->display + "]";
    ctx.text(px, py, title, accent);
    const char* kn = kind_name(def->kind);
    int kw = 0;
    for (const char* p = kn; *p; ++p) ++kw;
    ctx.text(x + w - 2 - kw, py, kn, Color::DarkGray);
    py += 2;

    // Costs row — exec / heat in yellow against gray labels.
    ctx.text(px, py, "Exec:", Color::DarkGray);
    ctx.text(px + 6, py, std::to_string(def->exec_cost), Color::Yellow);
    ctx.text(px + 14, py, "Heat:", Color::DarkGray);
    ctx.text(px + 20, py, std::to_string(def->heat_cost), Color::Yellow);
    if (def->kind == FragmentKind::Container && def->ram_per_n > 0) {
        // LOOP: ram = ram_per_n * N + ram_base
        std::string ram = std::to_string(def->ram_per_n) + "\xc2\xb7N"
                        + (def->ram_base > 0
                              ? std::string("+") + std::to_string(def->ram_base)
                              : std::string());
        ctx.text(px + 28, py, "RAM:", Color::DarkGray);
        ctx.text(px + 33, py, ram, Color::Yellow);
    }
    py += 2;

    // Param row — only for parameterized containers.
    if (def->takes_param) {
        ctx.text(px, py, "Param N:", Color::DarkGray);
        std::string range = std::to_string(def->min_n) + ".."
                          + std::to_string(def->max_n)
                          + "  (default " + std::to_string(def->default_n) + ")";
        ctx.text(px + 9, py, range, Color::Default);
        py += 2;
    }

    // Description, wrapped to the popup width.
    int wrap = w - 4;
    std::string desc = def->description ? def->description : "";
    while (!desc.empty()) {
        if (static_cast<int>(desc.size()) <= wrap) {
            ctx.text(px, py++, desc, Color::Default);
            break;
        }
        int cut = wrap;
        while (cut > 0 && desc[cut] != ' ') --cut;
        if (cut == 0) cut = wrap;
        ctx.text(px, py++, desc.substr(0, cut), Color::Default);
        // skip the space at the break
        if (cut < static_cast<int>(desc.size()) && desc[cut] == ' ') ++cut;
        desc = desc.substr(cut);
    }

    // Footer hint.
    ctx.text(x + 2, y + h - 2, " [?/Esc] Close", Color::DarkGray);
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
    // Compile name-prompt renders ON TOP of the Compiler sub-screen.
    if (cyberdeck_compile_prompt_ && cyberdeck_subscreen_ == CyberdeckSubscreen::Compiler) {
        draw_compile_prompt(*this, ctx);
    }
    // Fragment-info popup renders ON TOP of everything else in the Compiler.
    if (cyberdeck_fragment_help_ && cyberdeck_subscreen_ == CyberdeckSubscreen::Compiler) {
        draw_fragment_help_popup(*this, ctx);
    }
}

void PdaScreen::handle_cyberdeck_key(int key) {
    // Fragment-info popup intercepts everything while open. '?' toggles closed.
    if (cyberdeck_fragment_help_) {
        if (key == 27 || key == '?') {
            cyberdeck_fragment_help_close();
        }
        return;
    }

    // Compile-name prompt intercepts everything while open.
    if (cyberdeck_compile_prompt_) {
        if (key == 27) {                            // Esc → cancel
            cyberdeck_compile_prompt_close();
            return;
        }
        if (key == '\n' || key == '\r') {           // Enter → confirm
            std::string name = cyberdeck_compile_name_;
            cyberdeck_compile_prompt_close();
            if (game_) confirm_compile(*this, *game_, name);
            return;
        }
        if (key == '\b' || key == 127) {            // Backspace
            if (!cyberdeck_compile_name_.empty())
                cyberdeck_compile_name_.pop_back();
            return;
        }
        // Printable ASCII → append. Cap length so the field doesn't overflow.
        if (key >= ' ' && key < 127
            && cyberdeck_compile_name_.size() < 40) {
            cyberdeck_compile_name_.push_back(static_cast<char>(key));
            return;
        }
        return;
    }

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
        if (key == '\n' || key == '\r' || key == ' ') {
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
                    // Auto-bind to the abilities bar (append to first free slot).
                    ability_bar::assign_on_learn(
                        *player_,
                        cyberdeck_slot_skill_id(cyberdeck_slot_cursor_));
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
            case KEY_LEFT:
                if (cyberdeck_slot_cursor_ > 0) --cyberdeck_slot_cursor_;
                break;
            case KEY_RIGHT:
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
                        // Drop the ability-bar binding too.
                        ability_bar::remove_and_compact(
                            *player_,
                            cyberdeck_slot_skill_id(cyberdeck_slot_cursor_));
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

    // ── Compiler input ──────────────────────────────────────────────────
    const auto& palette = fragment_catalog();
    auto palette_step = [&](int dir) {
        int n = static_cast<int>(palette.size());
        for (int steps = 0; steps < n; ++steps) {
            int next = compiler_palette_cursor_ + dir;
            if (next < 0 || next >= n) return;
            compiler_palette_cursor_ = next;
            if (palette[compiler_palette_cursor_].id != FragmentId::None) return;
        }
    };

    // Build a flattened position list for build-cursor navigation.
    auto build_step = [&](int dir) {
        std::vector<EditPos> flat;
        flatten_positions(compiler_build_, {}, flat);
        int idx = locate_cursor(flat, build_cursor_path_, build_cursor_slot_);
        int next = idx + dir;
        if (next < 0) next = 0;
        if (next >= static_cast<int>(flat.size())) next = static_cast<int>(flat.size()) - 1;
        build_cursor_path_ = flat[next].path;
        build_cursor_slot_ = flat[next].slot;
    };

    // ← / → switch focus directionally: Left = Fragments (left pane),
    // Right = Build (middle pane). Build cursor is STATEFUL across the
    // toggle — palette-mode inserts land at the cursor's current position.
    if (key == KEY_LEFT) {
        compiler_focus_ = CompilerFocus::Palette;
        return;
    }
    if (key == KEY_RIGHT) {
        compiler_focus_ = CompilerFocus::Build;
        return;
    }

    switch (key) {
        case KEY_UP:
            if (compiler_focus_ == CompilerFocus::Palette) palette_step(-1);
            else                                            build_step(-1);
            break;
        case KEY_DOWN:
            if (compiler_focus_ == CompilerFocus::Palette) palette_step(+1);
            else                                            build_step(+1);
            break;
        case '\n': case '\r': case ' ': {
            // Insert palette-selected fragment at the build cursor.
            if (compiler_palette_cursor_ >= 0 &&
                compiler_palette_cursor_ < static_cast<int>(palette.size())) {
                FragmentId id = palette[compiler_palette_cursor_].id;
                if (id != FragmentId::None) {
                    insert_at_cursor(*this, id);
                    clamp_cursor(*this);
                }
            }
            break;
        }
        case '\b': case 127:
            delete_at_cursor(*this);
            clamp_cursor(*this);
            break;
        case '+':
        case '=': {
            ProgramNode* n = cursor_param_target(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && n->param < def->max_n) ++n->param;
            }
            break;
        }
        case '-':
        case '_': {
            ProgramNode* n = cursor_param_target(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && n->param > def->min_n) --n->param;
            }
            break;
        }
        case 'c':
            if (game_) begin_compile_prompt(*this, *game_);
            break;
        case '?': {
            // Resolve which fragment to describe:
            //   - Build focus + cursor on a node → that node's fragment
            //   - Otherwise → palette cursor's fragment
            FragmentId id = FragmentId::None;
            if (compiler_focus_ == CompilerFocus::Build) {
                if (ProgramNode* n = cursor_on_node(*this)) id = n->fragment;
            }
            if (id == FragmentId::None
                && compiler_palette_cursor_ >= 0
                && compiler_palette_cursor_ < static_cast<int>(palette.size())) {
                id = palette[compiler_palette_cursor_].id;
            }
            if (id != FragmentId::None) cyberdeck_fragment_help_open(id);
            break;
        }
        default: break;
    }
}

}  // namespace astra

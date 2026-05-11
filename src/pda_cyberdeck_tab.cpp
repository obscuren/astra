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

    int half = ctx.width() / 2;

    // Vertical divider between left/right panes
    for (int r = 1; r < ctx.height() - 1; ++r) {
        ctx.text(half - 1, r, "\xe2\x94\x82", Color::DarkGray);
    }

    // ── Left pane header: ──┤ DECK ├──
    self.draw_section_header(ctx, 1, "DECK", 1, half - 1);

    ctx.text(2, 2, deck_name, Color::White);

    int y = 4;
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
    self.draw_section_header(ctx, 1, "COMPILED PROGRAMS", half + 1, ctx.width() - 1);
    int yr = 3;
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
            ctx.text(x + 2, y, "\xe2\x86\x93", Color::DarkGray);   // ↓
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
            std::string header = marker + std::string(BoxDraw::TL)
                               + "\xe2\x94\x80 " + def->display
                               + "(" + std::to_string(node.param) + ") \xe2\x94\x80";
            ctx.text(x, y++, header, on_node ? Color::Cyan : Color::White);

            std::vector<int> child = self_path;
            child.push_back(i);
            y = render_chain_edit(ctx, x + 2, y, node.body,
                                  child, cursor_path, cursor_slot, build_focus);

            std::string footer = "  " + std::string(BoxDraw::BL) + "\xe2\x94\x80\xe2\x94\x80";
            ctx.text(x, y++, footer, Color::White);
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
        std::string line = (sel ? "\xe2\x96\xb8 " : "  ")
                         + std::string(def.display) + "  "
                         + std::to_string(def.exec_cost) + "/"
                         + std::to_string(def.heat_cost);
        Color color = !known ? Color::DarkGray
                    : sel    ? Color::Cyan
                             : Color::Default;
        ctx.text(col_p + 1, yp++, line, color);
    }

    // ── Middle pane: build ────────────────────────────────────────────────
    bool build_focus = self.compiler_focus() == PdaScreen::CompilerFocus::Build;
    render_chain_edit(ctx, col_b + 1, 3,
                      self.compiler_build(),
                      /*self_path=*/{},
                      self.build_cursor_path(),
                      self.build_cursor_slot(),
                      build_focus);

    // ── Footer hint: focus + compile ─────────────────────────────────────
    const char* focus_label = build_focus ? "Editor" : "Fragment";
    std::string footer = " [\xe2\x86\x90 Fragment] [Editor \xe2\x86\x92]   focus: "
                       + std::string(focus_label)
                       + "    [c] compile";
    ctx.text(2, ctx.height() - 2, footer, Color::DarkGray);

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
            ProgramNode* n = cursor_on_node(*this);
            if (n) {
                const FragmentDef* def = find_fragment(n->fragment);
                if (def && def->takes_param && n->param < def->max_n)
                    ++n->param;
            }
            break;
        }
        case '-':
        case '_': {
            ProgramNode* n = cursor_on_node(*this);
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

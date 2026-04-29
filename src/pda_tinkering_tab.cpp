#include "astra/pda_screen.h"
#include "astra/display_name.h"
#include "astra/skill_defs.h"
#include "astra/tinkering.h"
#include "terminal_theme.h"

#include <algorithm>
#include <string>
#include <vector>

namespace astra {

void PdaScreen::draw_tinkering(UIContext& ctx) {
    // Gate: require Tinkering category unlocked
    if (!player_has_skill(*player_, SkillId::Cat_Tinkering)) {
        draw_stub(ctx, "Tinkering workbench unavailable.");
        ctx.text({.x = ctx.width() / 2 - 23, .y = ctx.height() / 2 + 1,
                  .content = "Learn the Tinkering skill to use this station.", .tag = UITag::TextDim});
        return;
    }

    int w = ctx.width();
    int half = w / 2;

    // Section header
    draw_section_header(ctx, 0, "WORKBENCH");

    // Workbench box (centered in left half, 28 wide × 3 tall)
    int wb_w = 28;
    int wb_x = (half - wb_w) / 2;
    int wb_y = 2;
    bool wb_sel = (tinker_focus_ == TinkerFocus::Workbench);
    Color wb_border = wb_sel ? Color::Yellow : Color::DarkGray;

    // Draw workbench box
    ctx.put(wb_x, wb_y, BoxDraw::TL, wb_border);
    for (int i = 1; i < wb_w - 1; ++i) ctx.put(wb_x + i, wb_y, BoxDraw::H, wb_border);
    ctx.put(wb_x + wb_w - 1, wb_y, BoxDraw::TR, wb_border);

    ctx.put(wb_x, wb_y + 1, BoxDraw::V, wb_border);
    ctx.put(wb_x + wb_w - 1, wb_y + 1, BoxDraw::V, wb_border);

    ctx.put(wb_x, wb_y + 2, BoxDraw::BL, wb_border);
    for (int i = 1; i < wb_w - 1; ++i) ctx.put(wb_x + i, wb_y + 2, BoxDraw::H, wb_border);
    ctx.put(wb_x + wb_w - 1, wb_y + 2, BoxDraw::BR, wb_border);

    // Workbench content
    if (workbench_item_) {
        auto wb_vis = item_visual(workbench_item_->item_def_id);
        std::string display = std::string(1, wb_vis.glyph) + " " + workbench_item_->name;
        if (static_cast<int>(display.size()) > wb_w - 4) display = display.substr(0, wb_w - 4);
        int nx = wb_x + (wb_w - static_cast<int>(display.size())) / 2;
        ctx.put(nx, wb_y + 1, wb_vis.glyph, rarity_color(workbench_item_->rarity));
        ctx.text({.x = nx + 2, .y = wb_y + 1, .content = workbench_item_->name,
                  .tag = rarity_tag(workbench_item_->rarity)});
    } else {
    {
        std::string empty_msg = "Empty, no item";
        int emx = wb_x + (wb_w - static_cast<int>(empty_msg.size())) / 2;
        ctx.text({.x = emx, .y = wb_y + 1, .content = empty_msg, .tag = UITag::TextDim});
    }
    }

    // Connector line from workbench to slots
    int conn_x = wb_x + wb_w / 2;
    ctx.put(conn_x, wb_y + 3, BoxDraw::V, Color::DarkGray);
    ctx.put(conn_x, wb_y + 4, BoxDraw::V, Color::DarkGray);

    // Enhancement slots (3 boxes, 9 wide × 3 tall each)
    int slot_w = 11;
    int slot_gap = 2;
    // Center slots on the workbench center (conn_x is mid of slot 2)
    int slot_start_x = conn_x - (slot_w + slot_gap) - slot_w / 2;
    int slot_y = wb_y + 5;

    // Connector lines from workbench to slots:
    //   │ (vertical from workbench)
    //   ┌────┬────┐ (horizontal with corners at ends, T at center)
    //   │    │    │ (vertical into each slot)
    int slot1_cx = slot_start_x + slot_w / 2;
    int slot3_cx = slot_start_x + 2 * (slot_w + slot_gap) + slot_w / 2;
    int hy = slot_y - 1; // horizontal line row

    // Horizontal line
    for (int x = slot1_cx; x <= slot3_cx; ++x)
        ctx.put(x, hy, BoxDraw::H, Color::DarkGray);

    // Junctions: corners at ends, T at center where vertical comes from above
    ctx.put(slot1_cx, hy, BoxDraw::TL, Color::DarkGray);   // ┌ left end
    ctx.put(conn_x,   hy, BoxDraw::CROSS, Color::DarkGray); // ┼ center (vertical crosses horizontal)
    ctx.put(slot3_cx, hy, BoxDraw::TR, Color::DarkGray);   // ┐ right end

    int max_slots = workbench_item_ ? workbench_item_->enhancement_slots : 0;

    for (int si = 0; si < 3; ++si) {
        int sx = slot_start_x + si * (slot_w + slot_gap);
        int sy = slot_y;
        bool locked = (si >= max_slots);
        bool selected = (tinker_focus_ == TinkerFocus::Slots && tinker_slot_cursor_ == si);
        Color border = selected ? Color::Yellow : Color::DarkGray;

        // Box
        ctx.put(sx, sy, BoxDraw::TL, border);
        for (int i = 1; i < slot_w - 1; ++i) ctx.put(sx + i, sy, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, sy, BoxDraw::TR, border);

        ctx.put(sx, sy + 1, BoxDraw::V, border);
        ctx.put(sx + slot_w - 1, sy + 1, BoxDraw::V, border);

        ctx.put(sx, sy + 2, BoxDraw::BL, border);
        for (int i = 1; i < slot_w - 1; ++i) ctx.put(sx + i, sy + 2, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, sy + 2, BoxDraw::BR, border);

        // Slot label
        std::string label = "SLOT " + std::to_string(si + 1);
        int lx = sx + (slot_w - static_cast<int>(label.size())) / 2;
        ctx.text({.x = lx, .y = sy + 3, .content = label,
                  .tag = selected ? UITag::TextWarning : UITag::TextDim});

        // Content
        if (locked) {
        {
            int lpad = (slot_w - 2 - 6) / 2;
            ctx.text({.x = sx + 1 + lpad, .y = sy + 1, .content = "locked", .tag = UITag::TextDim});
        }
        } else if (workbench_item_ && si < static_cast<int>(workbench_item_->enhancements.size())
                   && workbench_item_->enhancements[si].filled) {
            const auto& enh = workbench_item_->enhancements[si];
            std::string bonus;
            if (enh.stat_bonus.av) bonus = "+" + std::to_string(enh.stat_bonus.av) + "AV";
            else if (enh.stat_bonus.dv) bonus = "+" + std::to_string(enh.stat_bonus.dv) + "DV";
            else if (enh.stat_bonus.view_radius) bonus = "+" + std::to_string(enh.stat_bonus.view_radius) + "VIS";
            UITag enh_tag = enh.committed ? UITag::TextSuccess : UITag::TextWarning;
        {
            int bpad = (slot_w - 2 - static_cast<int>(bonus.size())) / 2;
            ctx.text({.x = sx + 1 + bpad, .y = sy + 1, .content = bonus, .tag = enh_tag});
        }
        } else {
        {
            int epad = (slot_w - 2 - 5) / 2;
            ctx.text({.x = sx + 1 + epad, .y = sy + 1, .content = "empty", .tag = UITag::TextDim});
        }
        }
    }

    // Synthesizer section
    int synth_y = slot_y + 5;
    draw_section_header(ctx, synth_y, "SYNTHESIZER");
    synth_y += 2;

    bool has_synthesize = player_has_skill(*player_, SkillId::Synthesize);
    if (!has_synthesize) {
        ctx.text({.x = 3, .y = synth_y, .content = "Requires Synthesize skill to use.", .tag = UITag::TextDim});
        synth_y += 2;
    } else if (player_->learned_blueprints.size() >= 2) {
        // Two blueprint boxes side by side
        int bp_w = 16;
        int bp_gap = 3;
        int bp_total = bp_w * 2 + bp_gap;
        int bp_start = (half - bp_total) / 2;

        for (int bi = 0; bi < 2; ++bi) {
            int bx = bp_start + bi * (bp_w + bp_gap);
            int by = synth_y;
            bool selected = (tinker_focus_ == TinkerFocus::Synthesizer && synth_bp_cursor_ == bi);
            Color border = selected ? Color::Yellow : Color::DarkGray;

            ctx.put(bx, by, BoxDraw::TL, border);
            for (int i = 1; i < bp_w - 1; ++i) ctx.put(bx + i, by, BoxDraw::H, border);
            ctx.put(bx + bp_w - 1, by, BoxDraw::TR, border);

            ctx.put(bx, by + 1, BoxDraw::V, border);
            ctx.put(bx + bp_w - 1, by + 1, BoxDraw::V, border);

            ctx.put(bx, by + 2, BoxDraw::BL, border);
            for (int i = 1; i < bp_w - 1; ++i) ctx.put(bx + i, by + 2, BoxDraw::H, border);
            ctx.put(bx + bp_w - 1, by + 2, BoxDraw::BR, border);

            int bp_idx = (bi == 0) ? synth_bp1_ : synth_bp2_;
            if (bp_idx >= 0 && bp_idx < static_cast<int>(player_->learned_blueprints.size())) {
                std::string name = player_->learned_blueprints[bp_idx].name;
                if (static_cast<int>(name.size()) > bp_w - 2) name = name.substr(0, bp_w - 2);
                int nx = bx + (bp_w - static_cast<int>(name.size())) / 2;
                ctx.text({.x = nx, .y = by + 1, .content = name, .tag = UITag::TextAccent});
            } else {
            {
                std::string placeholder = (bi == 0) ? "Blueprint 1" : "Blueprint 2";
                int px = bx + (bp_w - static_cast<int>(placeholder.size())) / 2;
                ctx.text({.x = px, .y = by + 1, .content = placeholder, .tag = UITag::TextDim});
            }
            }
        }

        // "+" between boxes
        ctx.put(bp_start + bp_w + bp_gap / 2, synth_y + 1, '+', Color::White);

        // Recipe preview
        synth_y += 4;
        if (synth_bp1_ >= 0 && synth_bp2_ >= 0) {
            const auto& bp1 = player_->learned_blueprints[synth_bp1_].name;
            const auto& bp2 = player_->learned_blueprints[synth_bp2_].name;
            const auto* recipe = find_recipe(bp1, bp2);
            if (recipe) {
                ctx.label_value({.x = 3, .y = synth_y, .label = "Result: ", .label_tag = UITag::TextDim,
                                 .value = recipe->result_name, .value_tag = UITag::TextSuccess});
                synth_y++;

                // Show cost
                std::string cost;
                for (const auto& req : recipe->material_costs) {
                    if (!cost.empty()) cost += ", ";
                    const MaterialDef* def = find_material(req.material_id);
                    cost += std::to_string(req.count) + "x " + (def ? def->name : "?");
                }
                ctx.label_value({.x = 3, .y = synth_y, .label = "Cost: ", .label_tag = UITag::TextDim,
                                 .value = cost, .value_tag = UITag::TextDim});
                synth_y++;
                ctx.styled_text({.x = 3, .y = synth_y, .segments = key_action_segments("y", "Synthesize")});
            } else {
                ctx.text({.x = 3, .y = synth_y, .content = "No known recipe.", .tag = UITag::TextDim});
            }
        }
        synth_y += 2;
    } else {
        ctx.text({.x = 3, .y = synth_y, .content = "Learn 2+ blueprints to synthesize.", .tag = UITag::TextDim});
        synth_y += 2;
    }

    // Materials section
    int mat_y = synth_y;
    draw_section_header(ctx, mat_y, "MATERIALS");
    mat_y += 2;
    int mx = 3;
    for (const auto& item : player_->inventory.items) {
        if (item.type == ItemType::CraftingMaterial) {
            ctx.text_rich(mx, mat_y, display_name(item));
            mat_y++;
        }
    }
    if (mat_y == slot_y + 7) {
        ctx.text({.x = 3, .y = mat_y, .content = "No crafting materials.", .tag = UITag::TextDim});
    }

    // Right panel — upper (compact detail) + lower (blueprint catalog).
    int rx = half + 3;
    int rw_avail = w - half - 4;
    int right_edge = w - 1;  // end-of-screen for section-header line

    // --- Upper pane: item detail only (no action list — hotkeys live in the footer bar) ---
    int ry = 1;

    if (workbench_item_) {
        const auto& item = *workbench_item_;
        ctx.text_rich(rx, ry, display_name(item));
        ry++;
        ctx.text({.x = rx, .y = ry, .content = std::string(rarity_name(item.rarity)),
                  .tag = rarity_tag(item.rarity)});
        ry += 2;

        if (item.modifiers.av)
            ctx.label_value({.x = rx, .y = ry++, .label = "AV: +", .label_tag = UITag::StatAttack,
                             .value = std::to_string(item.modifiers.av), .value_tag = UITag::StatAttack});
        if (item.modifiers.dv)
            ctx.label_value({.x = rx, .y = ry++, .label = "DV: +", .label_tag = UITag::StatDefense,
                             .value = std::to_string(item.modifiers.dv), .value_tag = UITag::StatDefense});

        if (item.max_durability > 0) {
            ctx.text({.x = rx, .y = ry, .content = "Durabl: ", .tag = UITag::TextDim});
            int bar_w = std::min(14, rw_avail - 14);
            if (bar_w > 0) {
                ctx.progress_bar({.x = rx + 8, .y = ry, .width = bar_w,
                                  .value = item.durability, .max = item.max_durability,
                                  .tag = UITag::DurabilityBar});
            }
            std::string dur = std::to_string(item.durability) + "/" + std::to_string(item.max_durability);
            ctx.text({.x = rx + 8 + bar_w + 1, .y = ry, .content = dur, .tag = UITag::TextSuccess});
            ry++;
        }

        // Enhancement slot details (compact)
        ry++;
        for (int si = 0; si < 3; ++si) {
            bool locked = (si >= item.enhancement_slots);
            std::string slot_label = "[" + std::to_string(si + 1) + "] ";
            ctx.text({.x = rx, .y = ry, .content = slot_label, .tag = UITag::TextBright});
            if (locked) ctx.text({.x = rx + 4, .y = ry, .content = "locked", .tag = UITag::TextDim});
            else if (si < static_cast<int>(item.enhancements.size()) && item.enhancements[si].filled)
            {
                const auto& enh = item.enhancements[si];
                std::string label = enh.material_name;
                if (!enh.committed) label += " (pending)";
                ctx.text({.x = rx + 4, .y = ry, .content = label,
                          .tag = enh.committed ? UITag::TextSuccess : UITag::TextWarning});
            }
            else ctx.text({.x = rx + 4, .y = ry, .content = "empty", .tag = UITag::TextDim});
            ry++;
        }
    } else {
        ctx.text({.x = rx, .y = 1, .content = "Place an item on the", .tag = UITag::TextDim});
        ctx.text({.x = rx, .y = 2, .content = "workbench to begin.", .tag = UITag::TextDim});
        ctx.text({.x = rx, .y = 4, .content = "1. Select workbench", .tag = UITag::TextDim});
        ctx.text({.x = rx, .y = 5, .content = "2. Press [Space] to place", .tag = UITag::TextDim});
        ctx.text({.x = rx, .y = 6, .content = "3. Use [r] [a] [s] actions", .tag = UITag::TextDim});
        ctx.text({.x = rx, .y = 7, .content = "4. Select slots to enhance", .tag = UITag::TextDim});
        ry = 9;
    }

    // --- Catalog header — left/right arrows switch between two tabs ---
    int cat_hdr_y = ry + 1;
    const char* tab_title = (catalog_tab_ == CatalogTab::Blueprints)
        ? "\xe2\x97\x82 BLUEPRINT CATALOG \xe2\x96\xb8"
        : "\xe2\x97\x82 SCHEMATICS \xe2\x96\xb8";
    draw_section_header(ctx, cat_hdr_y, tab_title, half + 1, right_edge);
    int cy = cat_hdr_y + 2;
    int cy_max = ctx.height() - 1;

    // Collect recipes the player has unlocked (at least one ingredient known).
    auto player_knows = [&](const char* bp_name) {
        for (const auto& bp : player_->learned_blueprints)
            if (bp.name == bp_name) return true;
        return false;
    };
    struct KnownRecipe {
        const SynthesisRecipe* rec;
        bool has_bp1;
        bool has_bp2;
    };
    std::vector<KnownRecipe> known;
    for (const auto& r : synthesis_recipes()) {
        bool h1 = player_knows(r.blueprint_1);
        bool h2 = player_knows(r.blueprint_2);
        if (h1 || h2) known.push_back({&r, h1, h2});
    }

    int schem_count = static_cast<int>(player_->learned_schematics.size());
    bool show_blueprints = (catalog_tab_ == CatalogTab::Blueprints);
    bool show_schematics = (catalog_tab_ == CatalogTab::Schematics);
    bool empty = (show_blueprints && known.empty()) ||
                 (show_schematics && schem_count == 0);
    if (empty) {
        const char* msg = show_blueprints
            ? "No blueprints learned."
            : "No schematics learned. Find one and read it.";
        ctx.text({.x = rx, .y = cy, .content = msg, .tag = UITag::TextDim});
    } else {
        // Look up "do we have at least this much" for a material id.
        // Recipe material_id can be either Item::id (T2 convention) or
        // Item::item_def_id (junk reagents like Scrap), so match either.
        auto have_count = [&](uint32_t mid) -> int {
            int n = 0;
            for (const auto& it : player_->inventory.items) {
                if (it.id == mid || it.item_def_id == mid)
                    n += it.stack_count;
            }
            return n;
        };

        // Flattened display lines. is_header → clickable category bar (keyed by result).
        // is_cost → special styled row, pulls segments from recipe.
        struct Line {
            bool is_header = false;
            bool is_cost = false;
            bool is_schematic = false;  // true: use schem_idx; false: use recipe_idx
            int recipe_idx = 0;          // index into `known` (synthesis)
            int schem_idx = 0;           // index into player_->learned_schematics
            std::string text;
            UITag tag = UITag::TextDim;
        };
        std::vector<Line> lines;

        int panel_w = (w - (half + 1)) - 2; // usable text width inside right panel
        if (panel_w < 10) panel_w = 10;

        auto wrap = [](const std::string& s, int width) {
            std::vector<std::string> out;
            std::string cur;
            size_t i = 0;
            while (i < s.size()) {
                size_t j = s.find(' ', i);
                std::string word = (j == std::string::npos) ? s.substr(i) : s.substr(i, j - i);
                if (cur.empty()) cur = word;
                else if (static_cast<int>(cur.size() + 1 + word.size()) <= width) cur += " " + word;
                else { out.push_back(cur); cur = word; }
                if (j == std::string::npos) break;
                i = j + 1;
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        };

        if (show_blueprints) {
            for (int ri = 0; ri < static_cast<int>(known.size()); ++ri) {
                const auto& kr = known[ri];
                const auto& r = *kr.rec;
                bool collapsed = catalog_collapsed_.count(r.result_name) > 0;

                Line hdr; hdr.is_header = true; hdr.recipe_idx = ri;
                hdr.text = r.result_name; hdr.tag = UITag::TextAccent;
                lines.push_back(hdr);

                if (!collapsed) {
                    Line bp1; bp1.recipe_idx = ri;
                    bp1.text = std::string("  + ") + r.blueprint_1;
                    bp1.tag = kr.has_bp1 ? UITag::TextSuccess : UITag::TextDim;
                    lines.push_back(bp1);

                    Line bp2; bp2.recipe_idx = ri;
                    bp2.text = std::string("  + ") + r.blueprint_2;
                    bp2.tag = kr.has_bp2 ? UITag::TextSuccess : UITag::TextDim;
                    lines.push_back(bp2);

                    Line cost; cost.is_cost = true; cost.recipe_idx = ri;
                    lines.push_back(cost);

                    if (r.result_desc && *r.result_desc) {
                        lines.push_back({});
                        for (const auto& wl : wrap(r.result_desc, panel_w - 2)) {
                            Line d; d.recipe_idx = ri; d.text = "  " + wl; d.tag = UITag::TextDim;
                            lines.push_back(d);
                        }
                    }
                    lines.push_back({});
                }
            }
        } else {
            // Schematics tab — single-recipe rows, no blueprint sub-lines.
            for (int si = 0; si < schem_count; ++si) {
                const auto& ls = player_->learned_schematics[si];
                const SchematicRecipe* rec = find_schematic_recipe(ls.schematic_id);
                std::string out_name = rec ? rec->output_name : ls.name;
                bool collapsed = catalog_collapsed_.count(out_name) > 0;

                Line hdr; hdr.is_header = true; hdr.is_schematic = true; hdr.schem_idx = si;
                hdr.text = out_name; hdr.tag = UITag::TextAccent;
                lines.push_back(hdr);

                if (!collapsed && rec) {
                    Line cost; cost.is_cost = true; cost.is_schematic = true; cost.schem_idx = si;
                    lines.push_back(cost);
                    if (rec->output_desc && *rec->output_desc) {
                        lines.push_back({});
                        for (const auto& wl : wrap(rec->output_desc, panel_w - 2)) {
                            Line d; d.is_schematic = true; d.schem_idx = si;
                            d.text = "  " + wl; d.tag = UITag::TextDim;
                            lines.push_back(d);
                        }
                    }
                    lines.push_back({});
                }
            }
        }

        // Cursor index runs over the visible tab's contents only.
        int recipe_count = show_blueprints
            ? static_cast<int>(known.size())
            : schem_count;
        if (catalog_cursor_ >= recipe_count) catalog_cursor_ = recipe_count - 1;
        if (catalog_cursor_ < 0) catalog_cursor_ = 0;

        int cursor_line_idx = 0;
        for (int li = 0; li < static_cast<int>(lines.size()); ++li) {
            if (!lines[li].is_header) continue;
            int idx = lines[li].is_schematic ? lines[li].schem_idx : lines[li].recipe_idx;
            if (idx == catalog_cursor_) { cursor_line_idx = li; break; }
        }

        int visible_rows = cy_max - cy;
        if (visible_rows < 1) visible_rows = 1;
        if (cursor_line_idx < catalog_scroll_) catalog_scroll_ = cursor_line_idx;
        if (cursor_line_idx >= catalog_scroll_ + visible_rows)
            catalog_scroll_ = cursor_line_idx - visible_rows + 1;
        if (catalog_scroll_ < 0) catalog_scroll_ = 0;

        int total_lines = static_cast<int>(lines.size());
        int end_line = std::min(total_lines, catalog_scroll_ + visible_rows);

        int bar_x0 = half + 1;
        int bar_x1 = w;
        for (int li = catalog_scroll_; li < end_line; ++li) {
            const auto& L = lines[li];
            int y = cy + (li - catalog_scroll_);

            if (L.is_header) {
                int idx = L.is_schematic ? L.schem_idx : L.recipe_idx;
                bool cursor_here = (tinker_focus_ == TinkerFocus::Catalog
                                    && idx == catalog_cursor_);
                std::string name;
                if (L.is_schematic) {
                    const auto& ls = player_->learned_schematics[L.schem_idx];
                    const SchematicRecipe* rec = find_schematic_recipe(ls.schematic_id);
                    name = rec ? rec->output_name : ls.name;
                } else {
                    name = known[L.recipe_idx].rec->result_name;
                }
                bool collapsed = catalog_collapsed_.count(name) > 0;

                Color bar_bg = cursor_here ? static_cast<Color>(235) : static_cast<Color>(233);
                for (int fx = bar_x0; fx < bar_x1; ++fx)
                    ctx.put(fx, y, ' ', bar_bg, bar_bg);

                const char* tri = collapsed ? "\xe2\x96\xb8" : "\xe2\x96\xbe";
                ctx.put(bar_x0 + 1, y, tri, Color::DarkGray);
                ctx.put(bar_x0 + 3, y, BoxDraw::V, Color::Black);

                Color name_fg = cursor_here ? Color::Yellow : Color::White;
                int lx = bar_x0 + 5;
                for (char c : name) {
                    if (lx >= bar_x1 - 1) break;
                    ctx.put(lx++, y, c, name_fg, bar_bg);
                }
            } else if (L.is_cost) {
                std::vector<astra::TextSegment> segs;
                segs.push_back({"  Cost: ", UITag::TextDim});
                const auto& reqs = L.is_schematic
                    ? find_schematic_recipe(player_->learned_schematics[L.schem_idx].schematic_id)->material_costs
                    : known[L.recipe_idx].rec->material_costs;
                bool any = false;
                for (const auto& req : reqs) {
                    if (any) segs.push_back({", ", UITag::TextDim});
                    any = true;
                    bool enough = have_count(req.material_id) >= req.count;
                    UITag tag = enough ? UITag::TextSuccess : UITag::TextDim;
                    const MaterialDef* def = find_material(req.material_id);
                    std::string mname = def ? def->name : "?";
                    segs.push_back({std::to_string(req.count) + "x " + mname, tag});
                }
                if (!any) segs.push_back({"none", UITag::TextDim});
                ctx.styled_text({.x = rx, .y = y, .segments = segs});
            } else if (!L.text.empty()) {
                ctx.text({.x = rx, .y = y, .content = L.text, .tag = L.tag});
            }
        }

        if (catalog_scroll_ > 0)
            ctx.put(right_edge - 1, cy, '^', Color::DarkGray);
        if (end_line < total_lines)
            ctx.put(right_edge - 1, cy_max - 1, 'v', Color::DarkGray);
    }
}

} // namespace astra

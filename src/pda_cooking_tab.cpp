#include "astra/pda_screen.h"
#include "astra/item.h"
#include "astra/item_defs.h"
#include "astra/item_ids.h"
#include "astra/recipe.h"
#include "astra/effect.h"
#include "astra/skill_defs.h"

#include <algorithm>
#include <string>
#include <vector>

namespace astra {

// ─────────────────────────────────────────────────────────────────
// Cooking tab
// ─────────────────────────────────────────────────────────────────

namespace {

std::string cooking_item_name_for_def(uint16_t def_id) {
    Item it = build_by_def_id(def_id);
    return it.name.empty() ? std::string{"?"} : it.name;
}

std::string cooking_short_name(uint16_t def_id, int max_len) {
    std::string name = cooking_item_name_for_def(def_id);
    if (static_cast<int>(name.size()) > max_len && max_len > 1) {
        name = name.substr(0, max_len - 1) + ".";
    }
    return name;
}

int cooking_inventory_qty_for_def(const Player& p, uint16_t def_id) {
    int total = 0;
    for (const auto& it : p.inventory.items)
        if (it.item_def_id == def_id) total += it.stack_count;
    return total;
}

} // anonymous

void PdaScreen::draw_cooking(UIContext& ctx) {
    if (!player_has_skill(*player_, SkillId::Cat_Cooking)) {
        draw_stub(ctx, "Kitchen unavailable.");
        ctx.text({.x = ctx.width() / 2 - 22, .y = ctx.height() / 2 + 1,
                  .content = "Learn the Cooking skill to use the kitchen.",
                  .tag = UITag::TextDim});
        return;
    }

    int w = ctx.width();
    int half = w / 2;

    // ── LEFT HALF ──
    draw_section_header(ctx, 0, "COOKING POT");

    // Three pot slots centered in the left half.
    int slot_w = 13;
    int slot_gap = 2;
    int slots_total = slot_w * 3 + slot_gap * 2;
    int slots_start_x = (half - slots_total) / 2;
    int slots_y = 2;

    for (int i = 0; i < 3; ++i) {
        int sx = slots_start_x + i * (slot_w + slot_gap);
        bool selected = (cooking_focus_ == CookingFocus::Slots && cooking_slot_cursor_ == i);
        Color border = selected ? Color::Yellow : Color::DarkGray;

        ctx.put(sx, slots_y, BoxDraw::TL, border);
        for (int k = 1; k < slot_w - 1; ++k) ctx.put(sx + k, slots_y, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, slots_y, BoxDraw::TR, border);
        ctx.put(sx, slots_y + 1, BoxDraw::V, border);
        ctx.put(sx + slot_w - 1, slots_y + 1, BoxDraw::V, border);
        ctx.put(sx, slots_y + 2, BoxDraw::BL, border);
        for (int k = 1; k < slot_w - 1; ++k) ctx.put(sx + k, slots_y + 2, BoxDraw::H, border);
        ctx.put(sx + slot_w - 1, slots_y + 2, BoxDraw::BR, border);

        std::string label = "SLOT " + std::to_string(i + 1);
        int lx = sx + (slot_w - static_cast<int>(label.size())) / 2;
        ctx.text({.x = lx, .y = slots_y + 3, .content = label,
                  .tag = selected ? UITag::TextWarning : UITag::TextDim});

        const auto& slot = player_->cooking_slots[i];
        if (slot.item_def_id == 0) {
            std::string empty = "empty";
            int ex = sx + (slot_w - static_cast<int>(empty.size())) / 2;
            ctx.text({.x = ex, .y = slots_y + 1, .content = empty, .tag = UITag::TextDim});
        } else {
            std::string display = cooking_short_name(slot.item_def_id, slot_w - 5)
                                  + " x" + std::to_string(slot.qty);
            int dx = sx + (slot_w - static_cast<int>(display.size())) / 2;
            ctx.text({.x = dx, .y = slots_y + 1, .content = display});
        }
    }

    // Aura indicator below slots.
    int aura_y = slots_y + 5;
    bool near_fire = has_effect(player_->effects, EffectId::CookingFireAura);
    std::string aura_line = near_fire ? "* Near a cooking fire"
                                      : "o No cooking source nearby";
    UITag aura_tag = near_fire ? UITag::TextSuccess : UITag::TextDim;
    int ax = (half - static_cast<int>(aura_line.size())) / 2;
    ctx.text({.x = ax, .y = aura_y, .content = aura_line, .tag = aura_tag});

    // Ingredients list.
    int ing_hdr_y = aura_y + 2;
    draw_section_header(ctx, ing_hdr_y, "INGREDIENTS");
    int ing_y = ing_hdr_y + 2;

    std::vector<const Item*> ing_list;
    for (const auto& it : player_->inventory.items)
        if (it.type == ItemType::Ingredient) ing_list.push_back(&it);

    if (ing_list.empty()) {
        ctx.text({.x = 3, .y = ing_y, .content = "(no ingredients)", .tag = UITag::TextDim});
    } else {
        int max_rows = ctx.height() - ing_y - 2;
        if (cooking_picker_active_) {
            if (cooking_picker_cursor_ >= static_cast<int>(ing_list.size()))
                cooking_picker_cursor_ = static_cast<int>(ing_list.size()) - 1;
            if (cooking_picker_cursor_ < 0) cooking_picker_cursor_ = 0;
        }

        int rows = std::min<int>(static_cast<int>(ing_list.size()), std::max(0, max_rows));
        for (int i = 0; i < rows; ++i) {
            bool picker_sel = cooking_picker_active_ && cooking_picker_cursor_ == i;
            std::string cursor = picker_sel ? "> " : "  ";
            std::string line = cursor + ing_list[i]->name;
            ctx.text({.x = 3, .y = ing_y + i, .content = line,
                      .tag = picker_sel ? UITag::TextWarning : UITag::TextDefault});
            std::string qty = "x" + std::to_string(ing_list[i]->stack_count);
            int qty_x = half - 2 - static_cast<int>(qty.size());
            ctx.text({.x = qty_x, .y = ing_y + i, .content = qty, .tag = UITag::TextDim});
        }

        if (cooking_picker_active_) {
            std::string hint = "Pick ingredient for SLOT " +
                               std::to_string(cooking_picker_slot_ + 1) +
                               "   Enter=confirm  Esc=cancel";
            int hy = ing_y + rows + 1;
            if (hy >= ctx.height() - 1) hy = ctx.height() - 2;
            ctx.text({.x = 3, .y = hy, .content = hint, .tag = UITag::TextWarning});
        }
    }

    // ── RIGHT HALF: blueprint-style cookbook ──
    int right_x = half + 2;
    int right_edge = w;
    int panel_w = right_edge - right_x - 2;
    if (panel_w < 10) panel_w = 10;

    // Section header at the top of the right half.
    draw_section_header(ctx, 0, "COOKBOOK", right_x - 1, right_edge);

    // Resolve the list of known recipes via recipe_catalog(); preserve catalog order.
    std::vector<const Recipe*> known;
    known.reserve(player_->known_recipes.size());
    for (const Recipe& r : recipe_catalog()) {
        bool has = false;
        for (uint16_t rid : player_->known_recipes)
            if (rid == r.id) { has = true; break; }
        if (has) known.push_back(&r);
    }

    int cy = 2;
    int cy_max = ctx.height() - 1;

    if (known.empty()) {
        ctx.text({.x = right_x, .y = cy, .content = "(no recipes learned)", .tag = UITag::TextDim});
    } else {
        auto wrap = [](const std::string& s, int width) {
            std::vector<std::string> out;
            std::string cur;
            size_t i = 0;
            while (i < s.size()) {
                size_t j = s.find(' ', i);
                std::string word = (j == std::string::npos) ? s.substr(i) : s.substr(i, j - i);
                if (cur.empty()) cur = word;
                else if (static_cast<int>(cur.size() + 1 + word.size()) <= width)
                    cur += " " + word;
                else { out.push_back(cur); cur = word; }
                if (j == std::string::npos) break;
                i = j + 1;
            }
            if (!cur.empty()) out.push_back(cur);
            return out;
        };

        struct Line {
            bool is_header = false;
            int recipe_idx = 0;
            std::string text;
            UITag tag = UITag::TextDim;
        };
        std::vector<Line> lines;

        for (int ri = 0; ri < static_cast<int>(known.size()); ++ri) {
            const Recipe* r = known[ri];
            bool collapsed = cooking_collapsed_recipes_.count(r->id) > 0;

            Line hdr; hdr.is_header = true; hdr.recipe_idx = ri;
            hdr.text = r->name; hdr.tag = UITag::TextAccent;
            lines.push_back(hdr);

            if (!collapsed) {
                for (const auto& ing : r->ingredients) {
                    Line l; l.recipe_idx = ri;
                    int have = cooking_inventory_qty_for_def(*player_, ing.item_def_id);
                    bool enough = have >= ing.qty;
                    l.text = "  + " + std::to_string(ing.qty) + "x " +
                             cooking_short_name(ing.item_def_id,
                                                std::max(8, panel_w - 12)) +
                             "  (have " + std::to_string(have) + ")";
                    l.tag = enough ? UITag::TextSuccess : UITag::TextDim;
                    lines.push_back(l);
                }

                // Look up the result Item to resolve its DishOutput.
                Item result_item = build_by_def_id(r->result_item_def_id);
                Line cr; cr.recipe_idx = ri;
                cr.text = "  Creates: " + cooking_short_name(r->result_item_def_id,
                                                             std::max(8, panel_w - 12));
                cr.tag = UITag::TextDefault;
                lines.push_back(cr);

                if (result_item.dish) {
                    const auto& d = *result_item.dish;
                    if (d.hunger_shift < 0) {
                        Line h; h.recipe_idx = ri;
                        h.text = "    Hunger: " + std::to_string(-d.hunger_shift) +
                                 " step" + (d.hunger_shift == -1 ? "" : "s") + " toward Satiated";
                        h.tag = UITag::TextDefault;
                        lines.push_back(h);
                    }
                    if (d.hp_restore > 0) {
                        Line h; h.recipe_idx = ri;
                        h.text = "    Heals: " + std::to_string(d.hp_restore) + " HP";
                        h.tag = UITag::TextSuccess;
                        lines.push_back(h);
                    }
                    for (EffectId eid : d.granted) {
                        Effect e = effect_for_id(eid);
                        std::string desc = "    Grants: " + e.name;
                        if (e.duration > 0) {
                            desc += " (" + std::to_string(e.duration) + " turns";
                            if (e.tick_damage < 0) {
                                desc += ", +" + std::to_string(-e.tick_damage) + " HP/tick";
                            }
                            if (e.modifiers.av != 0) {
                                desc += ", ";
                                desc += (e.modifiers.av > 0 ? "+" : "");
                                desc += std::to_string(e.modifiers.av) + " AV";
                            }
                            if (e.modifiers.max_hp != 0) {
                                desc += ", ";
                                desc += (e.modifiers.max_hp > 0 ? "+" : "");
                                desc += std::to_string(e.modifiers.max_hp) + " max HP";
                            }
                            desc += ")";
                        }
                        Line eline; eline.recipe_idx = ri;
                        eline.text = desc;
                        eline.tag = UITag::TextAccent;
                        lines.push_back(eline);
                    }
                }

                if (!r->description.empty()) {
                    lines.push_back({}); // blank
                    for (const auto& wl : wrap(r->description, panel_w - 2)) {
                        Line d; d.recipe_idx = ri; d.text = "  " + wl; d.tag = UITag::TextDim;
                        lines.push_back(d);
                    }
                }
                lines.push_back({}); // trailing blank
            }
        }

        int recipe_count = static_cast<int>(known.size());
        if (cooking_cookbook_cursor_ >= recipe_count) cooking_cookbook_cursor_ = recipe_count - 1;
        if (cooking_cookbook_cursor_ < 0) cooking_cookbook_cursor_ = 0;

        // Scroll so the cursor-header is visible.
        int cursor_line_idx = 0;
        for (int li = 0; li < static_cast<int>(lines.size()); ++li) {
            if (lines[li].is_header && lines[li].recipe_idx == cooking_cookbook_cursor_) {
                cursor_line_idx = li; break;
            }
        }
        int visible_rows = cy_max - cy;
        if (visible_rows < 1) visible_rows = 1;
        if (cursor_line_idx < cooking_cookbook_scroll_) cooking_cookbook_scroll_ = cursor_line_idx;
        if (cursor_line_idx >= cooking_cookbook_scroll_ + visible_rows)
            cooking_cookbook_scroll_ = cursor_line_idx - visible_rows + 1;
        if (cooking_cookbook_scroll_ < 0) cooking_cookbook_scroll_ = 0;

        int total_lines = static_cast<int>(lines.size());
        int end_line = std::min(total_lines, cooking_cookbook_scroll_ + visible_rows);

        int bar_x0 = right_x - 1;
        int bar_x1 = right_edge;
        for (int li = cooking_cookbook_scroll_; li < end_line; ++li) {
            const auto& L = lines[li];
            int y = cy + (li - cooking_cookbook_scroll_);

            if (L.is_header) {
                bool cursor_here = (cooking_focus_ == CookingFocus::Cookbook &&
                                    L.recipe_idx == cooking_cookbook_cursor_);
                const Recipe* r = known[L.recipe_idx];
                bool collapsed = cooking_collapsed_recipes_.count(r->id) > 0;

                Color bar_bg = cursor_here ? static_cast<Color>(235) : static_cast<Color>(233);
                for (int fx = bar_x0; fx < bar_x1; ++fx)
                    ctx.put(fx, y, ' ', bar_bg, bar_bg);

                const char* tri = collapsed ? "\xe2\x96\xb8" : "\xe2\x96\xbe"; // ▸ / ▾
                ctx.put(bar_x0 + 1, y, tri, Color::DarkGray);
                ctx.put(bar_x0 + 3, y, BoxDraw::V, Color::Black);

                Color name_fg = cursor_here ? Color::Yellow : Color::White;
                int lx = bar_x0 + 5;
                for (char ch : r->name) {
                    if (lx >= bar_x1 - 1) break;
                    ctx.put(lx++, y, ch, name_fg, bar_bg);
                }
            } else if (!L.text.empty()) {
                ctx.text({.x = right_x, .y = y, .content = L.text, .tag = L.tag});
            }
        }

        if (cooking_cookbook_scroll_ > 0)
            ctx.put(right_edge - 1, cy, '^', Color::DarkGray);
        if (end_line < total_lines)
            ctx.put(right_edge - 1, cy_max - 1, 'v', Color::DarkGray);
    }

    // ── Quantity prompt overlay ──
    if (cooking_qty_prompt_active_) {
        int prompt_y = ctx.height() - 2;
        int available = cooking_inventory_qty_for_def(*player_, cooking_qty_prompt_item_def_id_);
        std::string name = cooking_short_name(cooking_qty_prompt_item_def_id_, 16);
        std::string line = "How many " + name + "? [ " +
                           std::to_string(cooking_qty_prompt_value_) +
                           " ]   (1-" + std::to_string(std::max(1, available)) +
                           ")   Enter=confirm  Esc=cancel";
        ctx.text({.x = 2, .y = prompt_y, .content = line, .tag = UITag::TextWarning});
    }
}

// ─────────────────────────────────────────────────────────────────
// Cooking tab — input handlers
// ─────────────────────────────────────────────────────────────────

void PdaScreen::handle_cooking_key(int key) {
    // Tab cycles focus: Slots <-> Cookbook.
    if (key == '\t') {
        cooking_focus_ = (cooking_focus_ == CookingFocus::Slots)
                       ? CookingFocus::Cookbook : CookingFocus::Slots;
        return;
    }

    // Cook action is available from any focus.
    if (key == 'c' || key == 'C') { cooking_attempt_cook(); return; }

    // Clear a slot — works only with Slots focus.
    if ((key == 'x' || key == 'X') && cooking_focus_ == CookingFocus::Slots) {
        cooking_clear_slot(cooking_slot_cursor_);
        return;
    }

    if (cooking_focus_ == CookingFocus::Slots) {
        if (key == KEY_LEFT  && cooking_slot_cursor_ > 0) --cooking_slot_cursor_;
        if (key == KEY_RIGHT && cooking_slot_cursor_ < 2) ++cooking_slot_cursor_;
        if (key == ' ' || key == '\n' || key == '\r') {
            cooking_open_picker_for_slot(cooking_slot_cursor_);
        }
        return;
    }

    if (cooking_focus_ == CookingFocus::Cookbook) {
        std::vector<const Recipe*> known;
        for (const Recipe& r : recipe_catalog()) {
            for (uint16_t rid : player_->known_recipes) {
                if (rid == r.id) { known.push_back(&r); break; }
            }
        }
        int total = static_cast<int>(known.size());
        if (total == 0) return;
        if (cooking_cookbook_cursor_ >= total) cooking_cookbook_cursor_ = total - 1;
        if (cooking_cookbook_cursor_ < 0) cooking_cookbook_cursor_ = 0;
        if (key == KEY_UP   && cooking_cookbook_cursor_ > 0) --cooking_cookbook_cursor_;
        if (key == KEY_DOWN && cooking_cookbook_cursor_ < total - 1) ++cooking_cookbook_cursor_;
        if (key == ' ' || key == '\n' || key == '\r') {
            const Recipe* r = known[cooking_cookbook_cursor_];
            cooking_toggle_recipe(r->id);
        }
        return;
    }
}

void PdaScreen::handle_cooking_picker_key(int key) {
    std::vector<const Item*> ing;
    for (const auto& it : player_->inventory.items)
        if (it.type == ItemType::Ingredient) ing.push_back(&it);
    int count = static_cast<int>(ing.size());
    if (count == 0) { cooking_picker_active_ = false; return; }
    if (cooking_picker_cursor_ < 0) cooking_picker_cursor_ = 0;
    if (cooking_picker_cursor_ >= count) cooking_picker_cursor_ = count - 1;

    if (key == 27) { cooking_picker_active_ = false; return; }
    if (key == KEY_UP   && cooking_picker_cursor_ > 0) --cooking_picker_cursor_;
    if (key == KEY_DOWN && cooking_picker_cursor_ < count - 1) ++cooking_picker_cursor_;
    if (key == ' ' || key == '\n' || key == '\r') cooking_picker_confirm();
}

void PdaScreen::cooking_open_picker_for_slot(int slot_idx) {
    int count = 0;
    for (const auto& it : player_->inventory.items)
        if (it.type == ItemType::Ingredient) ++count;
    if (count == 0) {
        context_message_ = "You have no ingredients.";
        context_msg_timer_ = 3;
        return;
    }
    cooking_picker_active_ = true;
    cooking_picker_slot_ = slot_idx;
    cooking_picker_cursor_ = 0;
}

void PdaScreen::cooking_picker_confirm() {
    std::vector<const Item*> ing;
    for (const auto& it : player_->inventory.items)
        if (it.type == ItemType::Ingredient) ing.push_back(&it);
    if (cooking_picker_cursor_ < 0 ||
        cooking_picker_cursor_ >= static_cast<int>(ing.size())) return;

    uint16_t def = ing[cooking_picker_cursor_]->item_def_id;
    // Prefill qty from an existing slot with this def if any.
    int prefill = 1;
    for (const auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == def) { prefill = std::max(1, slot.qty); break; }
    }
    cooking_picker_active_ = false;
    cooking_qty_prompt_active_ = true;
    cooking_qty_prompt_edited_ = false;
    cooking_qty_prompt_item_def_id_ = def;
    cooking_qty_prompt_value_ = prefill;
}

void PdaScreen::handle_cooking_qty_prompt_key(int key) {
    uint16_t def = cooking_qty_prompt_item_def_id_;
    // Available qty for clamping.
    int available = 0;
    for (const auto& it : player_->inventory.items)
        if (it.item_def_id == def) available += it.stack_count;
    if (available <= 0) { cooking_qty_prompt_active_ = false; return; }

    if (key == 27) {
        cooking_qty_prompt_active_ = false;
        cooking_qty_prompt_edited_ = false;
        return;
    }
    if (key >= '0' && key <= '9') {
        int digit = key - '0';
        // First keystroke replaces the prefill; subsequent ones append.
        int next = cooking_qty_prompt_edited_
                 ? cooking_qty_prompt_value_ * 10 + digit
                 : digit;
        if (next > available) next = available;
        cooking_qty_prompt_value_ = std::max(0, next);
        cooking_qty_prompt_edited_ = true;
        return;
    }
    if (key == 127 || key == 8) {
        cooking_qty_prompt_value_ = std::max(0, cooking_qty_prompt_value_ / 10);
        cooking_qty_prompt_edited_ = true;
        return;
    }
    if (key == '\n' || key == '\r') {
        if (cooking_qty_prompt_value_ < 1) cooking_qty_prompt_value_ = 1;
        cooking_commit_qty_prompt();
        cooking_qty_prompt_edited_ = false;
        return;
    }
}

void PdaScreen::cooking_commit_qty_prompt() {
    uint16_t def = cooking_qty_prompt_item_def_id_;
    int qty = cooking_qty_prompt_value_;

    // If an existing slot (other than the target) holds this def, merge the
    // quantities there to keep the bag canonical.
    int target = cooking_picker_slot_;
    if (target < 0 || target > 2) target = cooking_slot_cursor_;
    for (int i = 0; i < 3; ++i) {
        if (i == target) continue;
        if (player_->cooking_slots[i].item_def_id == def) {
            player_->cooking_slots[i].qty = qty;
            // Clear the target so the bag doesn't have a duplicate.
            if (target >= 0 && target <= 2) {
                player_->cooking_slots[target] = PotSlot{};
            }
            cooking_qty_prompt_active_ = false;
            return;
        }
    }

    if (target < 0 || target > 2) {
        context_message_ = "No empty slot.";
        context_msg_timer_ = 3;
    } else {
        player_->cooking_slots[target] = PotSlot{def, qty};
    }
    cooking_qty_prompt_active_ = false;
}

void PdaScreen::cooking_clear_slot(int idx) {
    if (idx < 0 || idx > 2) return;
    player_->cooking_slots[idx] = PotSlot{};
}

void PdaScreen::cooking_toggle_recipe(uint16_t recipe_id) {
    auto it = cooking_collapsed_recipes_.find(recipe_id);
    if (it != cooking_collapsed_recipes_.end()) cooking_collapsed_recipes_.erase(it);
    else cooking_collapsed_recipes_.insert(recipe_id);
}

void PdaScreen::cooking_attempt_cook() {
    if (!has_effect(player_->effects, EffectId::CookingFireAura)) {
        context_message_ = "You need to be near a fire to cook.";
        context_msg_timer_ = 3;
        return;
    }

    // Ensure at least one slot is filled.
    bool any = false;
    for (const auto& s : player_->cooking_slots)
        if (s.item_def_id != 0 && s.qty > 0) { any = true; break; }
    if (!any) {
        context_message_ = "The pot is empty.";
        context_msg_timer_ = 3;
        return;
    }

    // Verify inventory still holds the slotted amounts.
    for (const auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == 0) continue;
        int avail = 0;
        for (const auto& it : player_->inventory.items)
            if (it.item_def_id == slot.item_def_id) avail += it.stack_count;
        if (avail < slot.qty) {
            context_message_ = "Not enough ingredients.";
            context_msg_timer_ = 3;
            return;
        }
    }

    const Recipe* hit = match_recipe(player_->cooking_slots);

    // Consume slotted ingredients.
    auto consume = [&](uint16_t def_id, int qty) {
        auto& items = player_->inventory.items;
        for (auto it = items.begin(); it != items.end() && qty > 0; ) {
            if (it->item_def_id == def_id) {
                int take = std::min(qty, it->stack_count);
                it->stack_count -= take;
                qty -= take;
                if (it->stack_count <= 0) {
                    it = items.erase(it);
                    continue;
                }
            }
            ++it;
        }
    };
    for (const auto& slot : player_->cooking_slots) {
        if (slot.item_def_id == 0) continue;
        consume(slot.item_def_id, slot.qty);
    }

    if (hit) {
        bool was_known = std::find(player_->known_recipes.begin(),
                                   player_->known_recipes.end(),
                                   hit->id) != player_->known_recipes.end();
        Item dish = build_by_def_id(hit->result_item_def_id);
        dish.stack_count = hit->result_qty;
        player_->inventory.items.push_back(std::move(dish));
        if (!was_known) {
            player_->known_recipes.push_back(hit->id);
            context_message_ = "New recipe learned: " + hit->name + "!";
        } else {
            context_message_ = "You cook " + hit->name + ".";
        }
        context_msg_timer_ = 3;
    } else {
        player_->inventory.items.push_back(build_by_def_id(ITEM_BURNT_SLOP));
        context_message_ = "The result is inedible. You produce Burnt Slop.";
        context_msg_timer_ = 3;
    }

    for (auto& s : player_->cooking_slots) s = PotSlot{};
}

} // namespace astra

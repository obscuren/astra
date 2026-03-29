#include "astra/character_screen.h"
#include "astra/character.h"
#include "astra/effect.h"
#include "astra/race.h"
#include "astra/skill_defs.h"
#include "astra/journal.h"
#include "astra/tinkering.h"

#include <algorithm>
#include <string>

namespace astra {

static const char* tab_names[] = {
    "Skills", "Attributes", "Equipment", "Tinkering",
    "Journal", "Quests", "Reputation", "Ship",
};

bool CharacterScreen::is_open() const { return open_; }

void CharacterScreen::open(Player* player, Renderer* renderer, QuestManager* quests) {
    player_ = player;
    renderer_ = renderer;
    quests_ = quests;
    open_ = true;
    cursor_ = 0;
    scroll_ = 0;
    for (int i = 0; i < 6; ++i) pending_points_[i] = 0;

    // Initialize skill category expand state: only learned categories start expanded
    const auto& catalog = skill_catalog();
    skill_cat_expanded_.assign(catalog.size(), false);
    for (size_t ci = 0; ci < catalog.size(); ++ci) {
        for (auto sid : player_->learned_skills) {
            if (sid == catalog[ci].unlock_id) {
                skill_cat_expanded_[ci] = true;
                break;
            }
        }
    }
}

bool CharacterScreen::has_pending() const {
    for (int i = 0; i < 6; ++i) if (pending_points_[i] > 0) return true;
    return false;
}

int CharacterScreen::total_pending() const {
    int t = 0;
    for (int i = 0; i < 6; ++i) t += pending_points_[i];
    return t;
}

void CharacterScreen::commit_pending() {
    if (!has_pending()) return;
    int spent = total_pending();
    auto& a = player_->attributes;
    int* attrs[] = {&a.strength, &a.agility, &a.toughness,
                    &a.intelligence, &a.willpower, &a.luck};
    for (int i = 0; i < 6; ++i) {
        *attrs[i] += pending_points_[i];
        pending_points_[i] = 0;
    }
    player_->attribute_points -= spent;
    // Recalculate derived stats
    player_->max_hp = player_->effective_max_hp();
    if (player_->hp > player_->max_hp) player_->hp = player_->max_hp;
}

void CharacterScreen::close() { open_ = false; }

bool CharacterScreen::handle_input(int key) {
    if (!open_) return false;

    // ESC: close overlays first, then the screen itself
    if (key == 27) {
        if (look_open_) {
            look_open_ = false;
            look_item_ = nullptr;
            return true;
        }
        if (context_menu_.is_open()) {
            context_menu_.close();
            return true;
        }
        close();
        return true;
    }

    // Tab switching with q/e — skip when current tab uses these keys
    bool tab_switch_blocked = false;
    if (key == 'q' && !tab_switch_blocked) {
        int t = static_cast<int>(active_tab_);
        t = (t - 1 + char_tab_count) % char_tab_count;
        active_tab_ = static_cast<CharTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        return true;
    }
    if (key == 'e' && !tab_switch_blocked) {
        int t = static_cast<int>(active_tab_);
        t = (t + 1) % char_tab_count;
        active_tab_ = static_cast<CharTab>(t);
        cursor_ = 0;
        scroll_ = 0;
        return true;
    }

    // Look overlay — any key closes
    if (look_open_) {
        look_open_ = false;
        look_item_ = nullptr;
        return true;
    }

    // Context menu intercepts input when open
    if (context_menu_.is_open()) {
        MenuResult mr = context_menu_.handle_input(key);
        if (mr == MenuResult::Selected) {
            if (active_tab_ == CharTab::Tinkering) {
                // Tinkering item/material picker result
                int sel = context_menu_.selected();
                if (tinker_focus_ == TinkerFocus::Workbench && !workbench_item_) {
                    // Find the sel-th equippable/repairable item in inventory
                    int count = 0;
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.slot.has_value() || it.max_durability > 0) {
                            if (count == sel) {
                                workbench_inv_idx_ = i;
                                workbench_item_ = &player_->inventory.items[i];
                                // Init enhancement slots if needed
                                if (workbench_item_->enhancement_slots == 0 && workbench_item_->slot.has_value())
                                    init_enhancement_slots(*workbench_item_);
                                context_message_ = "Placed " + workbench_item_->name + " on workbench.";
                                context_msg_timer_ = 2;
                                break;
                            }
                            count++;
                        }
                    }
                } else if (tinker_focus_ == TinkerFocus::Synthesizer) {
                    // Blueprint picker result
                    if (sel >= 0 && sel < static_cast<int>(player_->learned_blueprints.size())) {
                        if (synth_bp_cursor_ == 0) synth_bp1_ = sel;
                        else synth_bp2_ = sel;
                    }
                } else if (tinker_focus_ == TinkerFocus::Slots && workbench_item_) {
                    // Find the sel-th crafting material (non Nano-Fiber)
                    int count = 0;
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.type == ItemType::CraftingMaterial && it.id != 7001) {
                            if (count == sel) {
                                auto result = enhance_item(*workbench_item_, tinker_slot_cursor_, it.id, *player_);
                                context_message_ = result.message;
                                context_msg_timer_ = 3;
                                break;
                            }
                            count++;
                        }
                    }
                }
            } else {
                execute_context_action(context_menu_.selected_key());
            }
        }
        return true;
    }

    if (context_msg_timer_ > 0) --context_msg_timer_;

    // Tab-specific input
    if (active_tab_ == CharTab::Attributes) {
        int max_cursor = 13;
        if (key == KEY_UP && cursor_ > 0) --cursor_;
        if (key == KEY_DOWN && cursor_ < max_cursor) ++cursor_;

        // +/- to allocate/deallocate points on primary attributes (cursor 0-5)
        if (cursor_ < 6) {
            int remaining = player_->attribute_points - total_pending();
            if ((key == '+' || key == '=') && remaining > 0) {
                pending_points_[cursor_]++;
            }
            if (key == '-' && pending_points_[cursor_] > 0) {
                pending_points_[cursor_]--;
            }
            // Space commits pending points
            if (key == ' ' && has_pending()) {
                commit_pending();
            }
        }
    } else if (active_tab_ == CharTab::Equipment) {
        if (key == '\t') {
            equip_focus_ = (equip_focus_ == EquipFocus::PaperDoll)
                ? EquipFocus::Inventory : EquipFocus::PaperDoll;
            return true;
        }
        if (equip_focus_ == EquipFocus::PaperDoll) {
            if (key == KEY_UP && equip_cursor_ > 0) --equip_cursor_;
            if (key == KEY_DOWN && equip_cursor_ < equip_slot_count - 1) ++equip_cursor_;
        } else {
            int count = static_cast<int>(player_->inventory.items.size());
            if (key == KEY_UP && inv_cursor_ > 0) --inv_cursor_;
            if (key == KEY_DOWN && inv_cursor_ < count - 1) ++inv_cursor_;
        }
        if (key == ' ') {
            open_context_menu();
            return true;
        }
    } else if (active_tab_ == CharTab::Skills) {

        auto has_skill = [&](SkillId id) {
            for (auto sid : player_->learned_skills)
                if (sid == id) return true;
            return false;
        };

        auto vis = build_skill_vis();

        int max_c = static_cast<int>(vis.size()) - 1;
        if (key == KEY_UP && skill_cursor_ > 0) --skill_cursor_;
        if (key == KEY_DOWN && skill_cursor_ < max_c) ++skill_cursor_;
        if (skill_cursor_ > max_c) skill_cursor_ = max_c;

        if (skill_cursor_ >= 0 && skill_cursor_ < static_cast<int>(vis.size())) {
            const auto& v = vis[skill_cursor_];

            // Space: toggle expand/collapse (categories only)
            if (key == ' ' && v.is_cat) {
                skill_cat_expanded_[v.ci] = !skill_cat_expanded_[v.ci];
            }

            // l: learn (category unlock or skill)
            if (key == 'l') {
                if (v.is_cat) {
                    const auto& cat = skill_catalog()[v.ci];
                    if (!has_skill(cat.unlock_id) && player_->skill_points >= cat.sp_cost) {
                        player_->skill_points -= cat.sp_cost;
                        player_->learned_skills.push_back(cat.unlock_id);
                        skill_cat_expanded_[v.ci] = true;
                        context_message_ = "Unlocked " + cat.name + "!";
                        context_msg_timer_ = 3;
                    }
                } else {
                    const auto& cat = skill_catalog()[v.ci];
                    const auto& sk = cat.skills[v.si];
                    if (!has_skill(cat.unlock_id)) {} // locked category
                    else if (has_skill(sk.id)) {} // already learned
                    else if (player_->skill_points < sk.sp_cost) {} // can't afford
                    else {
                        bool meets_req = true;
                        if (sk.attribute_req > 0 && sk.attribute_name) {
                            const auto& a = player_->attributes;
                            std::string attr(sk.attribute_name);
                            int val = 0;
                            if (attr == "Agility") val = a.agility;
                            else if (attr == "Strength") val = a.strength;
                            else if (attr == "Toughness") val = a.toughness;
                            else if (attr == "Intelligence") val = a.intelligence;
                            else if (attr == "Willpower") val = a.willpower;
                            else if (attr == "Luck") val = a.luck;
                            if (val < sk.attribute_req) meets_req = false;
                        }
                        if (meets_req) {
                            player_->skill_points -= sk.sp_cost;
                            player_->learned_skills.push_back(sk.id);
                            if (sk.id == SkillId::Haggle)
                                add_effect(player_->effects, make_haggle());
                            if (sk.id == SkillId::ThickSkin)
                                add_effect(player_->effects, make_thick_skin());
                            context_message_ = "Learned " + sk.name + "!";
                            context_msg_timer_ = 3;
                        }
                    }
                }
            }
        }
    } else if (active_tab_ == CharTab::Tinkering) {
        // Navigation between workbench, slots, materials
        if (key == KEY_UP) {
            if (tinker_focus_ == TinkerFocus::Materials) tinker_focus_ = TinkerFocus::Synthesizer;
            else if (tinker_focus_ == TinkerFocus::Synthesizer) tinker_focus_ = TinkerFocus::Slots;
            else if (tinker_focus_ == TinkerFocus::Slots) tinker_focus_ = TinkerFocus::Workbench;
        }
        if (key == KEY_DOWN) {
            if (tinker_focus_ == TinkerFocus::Workbench) tinker_focus_ = TinkerFocus::Slots;
            else if (tinker_focus_ == TinkerFocus::Slots) tinker_focus_ = TinkerFocus::Synthesizer;
            else if (tinker_focus_ == TinkerFocus::Synthesizer) tinker_focus_ = TinkerFocus::Materials;
        }
        if (tinker_focus_ == TinkerFocus::Slots) {
            if (key == KEY_LEFT && tinker_slot_cursor_ > 0) --tinker_slot_cursor_;
            if (key == KEY_RIGHT && tinker_slot_cursor_ < 2) ++tinker_slot_cursor_;
        }

        // Space: place/remove item on workbench, or slot material
        if (key == ' ') {
            if (tinker_focus_ == TinkerFocus::Workbench) {
                if (workbench_item_) {
                    // Remove item from workbench
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                    context_message_ = "Item removed from workbench.";
                    context_msg_timer_ = 2;
                } else {
                    // Open item picker
                    context_menu_.close();
                    context_menu_.set_title("Place Item");
                    for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                        const auto& it = player_->inventory.items[i];
                        if (it.slot.has_value() || it.max_durability > 0) {
                            char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                            context_menu_.add_option(key_ch, it.name);
                        }
                    }
                    if (context_menu_.is_open()) {} // already has options
                    context_menu_.open();
                }
            } else if (tinker_focus_ == TinkerFocus::Slots && workbench_item_) {
                int si = tinker_slot_cursor_;
                if (si < workbench_item_->enhancement_slots) {
                    // Ensure vector is sized
                    while (static_cast<int>(workbench_item_->enhancements.size()) <= si)
                        workbench_item_->enhancements.push_back({});
                    if (!workbench_item_->enhancements[si].filled) {
                        // Open material picker
                        context_menu_.close();
                        context_menu_.set_title("Select Material");
                        for (int i = 0; i < static_cast<int>(player_->inventory.items.size()); ++i) {
                            const auto& it = player_->inventory.items[i];
                            if (it.type == ItemType::CraftingMaterial && it.id != 7001) {
                                char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                                context_menu_.add_option(key_ch, it.name);
                            }
                        }
                        context_menu_.open();
                    }
                }
            }
        }

        // Synthesizer input
        if (tinker_focus_ == TinkerFocus::Synthesizer) {
            if (key == KEY_LEFT) synth_bp_cursor_ = 0;
            if (key == KEY_RIGHT) synth_bp_cursor_ = 1;
            if (key == ' ' && !player_->learned_blueprints.empty()) {
                context_menu_.close();
                context_menu_.set_title("Select Blueprint");
                for (int i = 0; i < static_cast<int>(player_->learned_blueprints.size()); ++i) {
                    char key_ch = (i < 26) ? ('a' + i) : ('1' + i - 26);
                    context_menu_.add_option(key_ch, player_->learned_blueprints[i].name);
                }
                context_menu_.open();
            }
            if (key == 'y' && synth_bp1_ >= 0 && synth_bp2_ >= 0 &&
                player_has_skill(*player_, SkillId::Synthesize)) {
                const auto& bp1 = player_->learned_blueprints[synth_bp1_].name;
                const auto& bp2 = player_->learned_blueprints[synth_bp2_].name;
                auto result = synthesize_item(bp1, bp2, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 4;
                if (result.success) {
                    synth_bp1_ = -1;
                    synth_bp2_ = -1;
                }
            }
        }

        // Action hotkeys
        if (workbench_item_) {
            if (key == 'r') {
                auto result = repair_item(*workbench_item_, *player_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
            }
            if (key == 'a') {
                size_t bp_count_before = player_->learned_blueprints.size();
                std::string item_name = workbench_item_->name;
                auto result = analyze_item(*workbench_item_, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 4;
                // If a new blueprint was learned, create journal entry
                if (player_->learned_blueprints.size() > bp_count_before) {
                    const auto& bp = player_->learned_blueprints.back();
                    // TODO: get world_tick and phase from game — for now use 0
                    player_->journal.push_back(make_blueprint_journal_entry(
                        bp.name, bp.description, item_name, 0, "Unknown"));
                }
                if (!result.success) {
                    // Item destroyed — remove from inventory
                    if (workbench_inv_idx_ >= 0 && workbench_inv_idx_ < static_cast<int>(player_->inventory.items.size())) {
                        player_->inventory.items.erase(player_->inventory.items.begin() + workbench_inv_idx_);
                    }
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 's') {
                auto result = salvage_item(*workbench_item_, *player_, rng_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
                if (result.success) {
                    if (workbench_inv_idx_ >= 0 && workbench_inv_idx_ < static_cast<int>(player_->inventory.items.size())) {
                        player_->inventory.items.erase(player_->inventory.items.begin() + workbench_inv_idx_);
                    }
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 'f') {
                // Assemble (finalize) — commit pending enhancements
                auto result = commit_enhancements(*workbench_item_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
                if (result.success) {
                    // Clear workbench after successful assembly
                    workbench_item_ = nullptr;
                    workbench_inv_idx_ = -1;
                }
            }
            if (key == 'x' && tinker_focus_ == TinkerFocus::Slots) {
                // Clear slot — undo pending enhancement
                auto result = clear_enhancement_slot(*workbench_item_, tinker_slot_cursor_, *player_);
                context_message_ = result.message;
                context_msg_timer_ = 3;
            }
        }
    } else if (active_tab_ == CharTab::Journal) {
        int count = static_cast<int>(player_->journal.size());
        if (count > 0) {
            // List is rendered newest-first, so Up = higher index, Down = lower index
            if (key == KEY_UP) journal_cursor_ = (journal_cursor_ + 1) % count;
            if (key == KEY_DOWN) journal_cursor_ = (journal_cursor_ - 1 + count) % count;
        }
    } else if (active_tab_ == CharTab::Reputation) {
        int count = static_cast<int>(player_->reputation.size());
        if (count > 0) {
            if (key == KEY_UP && cursor_ > 0) --cursor_;
            if (key == KEY_DOWN && cursor_ < count - 1) ++cursor_;
        }
    }

    return true;
}

// ─────────────────────────────────────────────────────────────────
// Context menu
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::open_context_menu() {
    context_menu_.close(); // reset

    if (equip_focus_ == EquipFocus::PaperDoll) {
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        const auto& item = player_->equipment.slot_ref(slot);
        if (!item) return;
        context_menu_.add_option('l', "look");
        context_menu_.add_option('r', "remove");
        if (item->ranged) {
            context_menu_.add_option('u', "unload");
        }
    } else {
        if (player_->inventory.items.empty()) return;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(player_->inventory.items.size())) return;
        const auto& item = player_->inventory.items[inv_cursor_];
        context_menu_.add_option('l', "look");
        if (item.slot.has_value()) {
            if (item.type == ItemType::MeleeWeapon) {
                // Melee weapons can go in either hand
                context_menu_.add_option('e', "equip right hand");
                context_menu_.add_option('q', "equip left hand");
            } else {
                context_menu_.add_option('e', "equip");
            }
        }
        if (item.ranged) {
            context_menu_.add_option('r', "reload");
            context_menu_.add_option('u', "unload");
        }
    }

    context_menu_.open();
}

void CharacterScreen::execute_context_action(char key) {
    if (equip_focus_ == EquipFocus::PaperDoll) {
        auto slot = static_cast<EquipSlot>(equip_cursor_);
        auto& equipped = player_->equipment.slot_ref(slot);
        if (!equipped) return;

        if (key == 'l') {
            look_item_ = &(*equipped);
            look_open_ = true;
        } else if (key == 'r') {
            if (!player_->inventory.can_add(*equipped)) {
                context_message_ = "Inventory too heavy.";
                context_msg_timer_ = 3;
                return;
            }
            context_message_ = "Removed " + equipped->name + ".";
            context_msg_timer_ = 3;
            player_->inventory.items.push_back(std::move(*equipped));
            equipped.reset();
        } else if (key == 'u') {
            if (equipped->ranged && equipped->ranged->current_charge > 0) {
                context_message_ = "Unloaded " + std::to_string(equipped->ranged->current_charge) + " charge.";
                equipped->ranged->current_charge = 0;
            } else {
                context_message_ = "Nothing to unload.";
            }
            context_msg_timer_ = 3;
        }
    } else {
        auto& items = player_->inventory.items;
        if (inv_cursor_ < 0 || inv_cursor_ >= static_cast<int>(items.size())) return;

        if (key == 'l') {
            look_item_ = &items[inv_cursor_];
            look_open_ = true;
        } else if (key == 'e' || key == 'q') {
            auto& item = items[inv_cursor_];
            if (item.slot) {
                EquipSlot target_slot = *item.slot;
                // 'q' = left hand for melee weapons
                if (key == 'q' && item.type == ItemType::MeleeWeapon) {
                    target_slot = EquipSlot::LeftHand;
                }
                auto& sl = player_->equipment.slot_ref(target_slot);
                Item to_equip = std::move(item);
                items.erase(items.begin() + inv_cursor_);
                if (sl) items.push_back(std::move(*sl));
                sl = std::move(to_equip);
                context_message_ = "Equipped " + sl->name + ".";
                context_msg_timer_ = 3;
                if (inv_cursor_ >= static_cast<int>(items.size()) && inv_cursor_ > 0)
                    --inv_cursor_;
            }
        } else if (key == 'r') {
            context_message_ = "Reload not yet implemented.";
            context_msg_timer_ = 3;
        } else if (key == 'u') {
            auto& item = items[inv_cursor_];
            if (item.ranged && item.ranged->current_charge > 0) {
                context_message_ = "Unloaded " + std::to_string(item.ranged->current_charge) + " charge.";
                item.ranged->current_charge = 0;
            } else {
                context_message_ = "Nothing to unload.";
            }
            context_msg_timer_ = 3;
        }
    }
}


void CharacterScreen::draw_look_overlay(DrawContext& ctx) {
    if (!look_open_ || !look_item_) return;

    const auto& item = *look_item_;

    int win_w = 44;
    int win_h = 18;
    int mx = (ctx.width() - win_w) / 2;
    int my = (ctx.height() - win_h) / 2;

    Window win(renderer_, Rect{ctx.bounds().x + mx, ctx.bounds().y + my, win_w, win_h}, item.name);
    win.set_footer("[any key] Close");
    win.draw();

    DrawContext lc = win.content();
    draw_item_info(lc, item);
}

void CharacterScreen::draw(int screen_w, int screen_h) {
    if (!open_ || !renderer_) return;

    // Inset from screen edges
    int pad_x = 2;
    int pad_y = 2;
    int win_w = screen_w - pad_x * 2;
    int win_h = screen_h - pad_y * 2;
    Panel panel(renderer_, Rect{pad_x, pad_y, win_w, win_h});
    if (active_tab_ == CharTab::Tinkering) {
        panel.set_footer("[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Nav  [Space] Select  [r] Repair  [a] Analyze  [s] Salvage  [f] Assemble  [x] Clear  [y] Synth");
    } else if (active_tab_ == CharTab::Skills) {
        panel.set_footer("[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate  [Space] Expand  [l] Learn");
    } else if (has_pending()) {
        panel.set_footer("[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate  [-/+] Adjust  [Space] Commit");
    } else {
        panel.set_footer("[ESC] Close  [\xe2\x86\x91\xe2\x86\x93] Navigate");
    }
    panel.draw();

    DrawContext ctx = panel.content();

    draw_tab_bar(ctx);

    // Full-width area below tab bar (for section headers that span full width)
    DrawContext full = ctx.sub(Rect{0, 2, ctx.width(), ctx.height() - 2});
    // Padded content area for tab content
    int pad = 3;
    DrawContext content = ctx.sub(Rect{pad, 2, ctx.width() - pad * 2, ctx.height() - 2});

    switch (active_tab_) {
        case CharTab::Attributes: draw_attributes(content); break;
        case CharTab::Skills:     draw_skills(full); break;
        case CharTab::Equipment:  draw_equipment(content); break;
        case CharTab::Reputation: draw_reputation(content); break;
        case CharTab::Tinkering:  draw_tinkering(full); break;
        case CharTab::Journal:    draw_journal(content); break;
        case CharTab::Quests: {
            if (!quests_ || quests_->active_quests().empty()) {
                draw_stub(content, "No active quests.");
            } else {
                int y = 0;
                for (const auto& q : quests_->active_quests()) {
                    if (y >= content.height() - 1) break;
                    content.text(1, y, q.title, Color::Yellow);
                    y++;
                    content.text(2, y, q.description, Color::DarkGray);
                    y++;
                    for (const auto& obj : q.objectives) {
                        if (y >= content.height() - 1) break;
                        std::string status = obj.complete() ? "[x] " : "[ ] ";
                        std::string progress = " (" + std::to_string(obj.current_count) + "/" +
                                              std::to_string(obj.target_count) + ")";
                        Color c = obj.complete() ? Color::Green : Color::White;
                        content.text(3, y, status + obj.description + progress, c);
                        y++;
                    }
                    // Reward summary
                    if (y < content.height() - 1) {
                        std::string rew = "  Reward:";
                        if (q.reward.xp > 0) rew += " " + std::to_string(q.reward.xp) + " XP";
                        if (q.reward.credits > 0) rew += " " + std::to_string(q.reward.credits) + "$";
                        if (q.reward.skill_points > 0) rew += " " + std::to_string(q.reward.skill_points) + " SP";
                        content.text(2, y, rew, Color::Cyan);
                        y++;
                    }
                    y++; // blank line between quests
                }
                // Completed quests
                if (!quests_->completed_quests().empty() && y < content.height() - 1) {
                    content.text(1, y, "Completed:", Color::DarkGray);
                    y++;
                    for (const auto& q : quests_->completed_quests()) {
                        if (y >= content.height() - 1) break;
                        Color c = q.status == QuestStatus::Completed ? Color::Green : Color::Red;
                        content.text(2, y, q.title, c);
                        y++;
                    }
                }
            }
            break;
        }
        case CharTab::Ship:       draw_stub(content, "Ship systems not available."); break;
    }

    // Draw vertical divider only for tabs that use a split layout
    bool needs_divider = (active_tab_ == CharTab::Attributes
                       || active_tab_ == CharTab::Skills
                       || active_tab_ == CharTab::Equipment
                       || (active_tab_ == CharTab::Tinkering && player_has_skill(*player_, SkillId::Cat_Tinkering))
                       || (active_tab_ == CharTab::Journal && !player_->journal.empty()));
    if (needs_divider) {
        int divider_x = content.width() / 2;
        int last = content.height() - 1;
        ctx.put(divider_x, 1, BoxDraw::TT, Color::DarkGray);  // ┬ connects to tab separator
        for (int vy = 0; vy < last; ++vy) {
            content.put(divider_x, vy, BoxDraw::V, Color::DarkGray);
        }
        content.put(divider_x, last, BoxDraw::BT, Color::DarkGray); // ┴ at bottom
    }

    // Context menu overlay (equipment tab)
    context_menu_.draw(renderer_, screen_w, screen_h);

    // Look overlay
    draw_look_overlay(content);

    // Status message at bottom of content
    if (context_msg_timer_ > 0 && !context_message_.empty()) {
        content.text_center(content.height() - 1, context_message_, Color::Green);
    }
}

// ─────────────────────────────────────────────────────────────────
// Tab bar
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_tab_bar(DrawContext& ctx) {
    // Calculate total width: [Q] + space + tab labels with gaps + space + [E]
    int total_w = 4 + 1; // "[Q] "
    for (int i = 0; i < char_tab_count; ++i) {
        std::string label = tab_names[i];
        bool active = (static_cast<int>(active_tab_) == i);
        total_w += (active ? static_cast<int>(label.size()) + 2 : static_cast<int>(label.size()));
        if (i < char_tab_count - 1) total_w += 2; // gap
    }
    total_w += 1 + 3; // " [E]"

    int x = (ctx.width() - total_w) / 2;
    if (x < 0) x = 0;

    // [Q]
    ctx.put(x, 0, '[', Color::White);
    ctx.put(x + 1, 0, 'Q', Color::Green);
    ctx.put(x + 2, 0, ']', Color::White);
    x += 4;

    // Tab labels
    for (int i = 0; i < char_tab_count; ++i) {
        bool active = (static_cast<int>(active_tab_) == i);
        std::string label = tab_names[i];
        if (active) label = "[" + label + "]";

        Color fg = active ? Color::White : Color::DarkGray;
        ctx.text(x, 0, label, fg);
        x += static_cast<int>(label.size()) + 2;
    }

    // [E]
    ctx.put(x - 1, 0, '[', Color::White);
    ctx.put(x, 0, 'E', Color::Green);
    ctx.put(x + 1, 0, ']', Color::White);

    ctx.hline(1, BoxDraw::H, Color::DarkGray);
}

// ─────────────────────────────────────────────────────────────────
// Stat box drawing helper
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_stat_box(DrawContext& ctx, int x, int y,
                                     const char* label, int value,
                                     bool selected, int modifier,
                                     int pending, bool can_allocate) {
    // Box is 7 wide. Height: 3 (label + value) or 4 (+ modifier row)
    bool has_mod = (modifier != -999);
    int h = has_mod ? 4 : 3;
    Color border_color = selected ? Color::Yellow : Color::DarkGray;

    // Top border
    ctx.put(x, y, BoxDraw::TL, border_color);
    for (int i = 1; i < 6; ++i) ctx.put(x + i, y, BoxDraw::H, border_color);
    ctx.put(x + 6, y, BoxDraw::TR, border_color);

    // Label row
    ctx.put(x, y + 1, BoxDraw::V, border_color);
    std::string lbl(label);
    int pad = static_cast<int>(5 - lbl.size()) / 2;
    ctx.text(x + 1 + pad, y + 1, lbl, selected ? Color::Yellow : Color::Cyan);
    ctx.put(x + 6, y + 1, BoxDraw::V, border_color);

    // Value row — green if has pending points
    ctx.put(x, y + 2, BoxDraw::V, border_color);
    std::string val = std::to_string(value);
    int vpad = static_cast<int>(5 - val.size()) / 2;
    Color val_color = (pending > 0) ? Color::Green : Color::White;
    ctx.text(x + 1 + vpad, y + 2, val, val_color);
    ctx.put(x + 6, y + 2, BoxDraw::V, border_color);

    // Modifier row (primary attributes only)
    if (has_mod) {
        ctx.put(x, y + 3, BoxDraw::V, border_color);
        std::string mod_str;
        Color mod_color;
        if (modifier > 0) {
            mod_str = "[+" + std::to_string(modifier) + "]";
            mod_color = Color::Green;
        } else if (modifier < 0) {
            mod_str = "[" + std::to_string(modifier) + "]";
            mod_color = Color::Red;
        } else {
            mod_str = "[ 0]";
            mod_color = Color::DarkGray;
        }
        int mpad = static_cast<int>(5 - mod_str.size()) / 2;
        ctx.text(x + 1 + mpad, y + 3, mod_str, mod_color);
        ctx.put(x + 6, y + 3, BoxDraw::V, border_color);
    }

    // Bottom border — show -/+ hint when allocatable
    int bot = y + h;
    ctx.put(x, bot, BoxDraw::BL, border_color);
    bool show_hint = (pending > 0) || (selected && can_allocate);
    if (show_hint) {
        ctx.put(x + 1, bot, BoxDraw::H, border_color);
        ctx.put(x + 2, bot, '-', Color::Yellow);
        ctx.put(x + 3, bot, '/', Color::DarkGray);
        ctx.put(x + 4, bot, '+', Color::Yellow);
        ctx.put(x + 5, bot, BoxDraw::H, border_color);
    } else {
        for (int i = 1; i < 6; ++i) ctx.put(x + i, bot, BoxDraw::H, border_color);
    }
    ctx.put(x + 6, bot, BoxDraw::BR, border_color);
}

void CharacterScreen::draw_section_header(DrawContext& ctx, int y,
                                           const char* title, int left_margin) {
    // ──┤ TITLE ├──────── (stops before the vertical divider)
    int divider_x = ctx.width() / 2;
    // Leading ──
    ctx.put(left_margin, y, BoxDraw::H, Color::DarkGray);
    ctx.put(left_margin + 1, y, BoxDraw::H, Color::DarkGray);
    // ┤
    ctx.put(left_margin + 2, y, BoxDraw::RT, Color::DarkGray);
    // space + TITLE + space
    ctx.put(left_margin + 3, y, ' ');
    ctx.text(left_margin + 4, y, title, Color::White);
    int after_title = left_margin + 4 + static_cast<int>(std::string(title).size());
    ctx.put(after_title, y, ' ');
    // ├
    ctx.put(after_title + 1, y, BoxDraw::LT, Color::DarkGray);
    // Trailing ──
    for (int x = after_title + 2; x < divider_x; ++x) {
        ctx.put(x, y, BoxDraw::H, Color::DarkGray);
    }
}

// ─────────────────────────────────────────────────────────────────
// Attributes tab
// ─────────────────────────────────────────────────────────────────

static const char* primary_labels[] = {"STR", "AGI", "TOU", "INT", "WIL", "LUC"};
static const char* primary_names[] = {"Strength", "Agility", "Toughness", "Intelligence", "Willpower", "Luck"};
static const char* primary_descriptions[] = {
    "determines melee damage and carry capacity.",
    "affects dodge value, move speed, and ranged accuracy.",
    "increases max HP and physical resistance.",
    "improves tinkering, hacking, and XP gain.",
    "strengthens mental resistance and energy regen.",
    "influences critical hits and loot quality.",
};

static const char* sec_labels[] = {"QN", "MS", "AV", "DV"};
static const char* sec_names[] = {"Quickness", "Move Speed", "Armor Value", "Dodge Value"};
static const char* sec_descriptions[] = {
    "determines how often you act relative to others.",
    "affects how fast you move across the map.",
    "reduces incoming physical damage.",
    "determines your chance to avoid attacks entirely.",
};

static const char* res_labels[] = {"AR", "ER", "CR", "HR"};
static const char* res_names[] = {"Acid Resistance", "Electrical Resistance", "Cold Resistance", "Heat Resistance"};
static const char* res_descriptions[] = {
    "reduces damage from corrosive and acidic sources.",
    "reduces damage from electrical and ion attacks.",
    "reduces damage from cold and cryo effects.",
    "reduces damage from heat, fire, and plasma.",
};

void CharacterScreen::draw_attributes(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    // Character identity header
    int y = 1;
    ctx.put(2, y, '@', Color::White);
    ctx.text(4, y, player_->name, Color::White);
    y++;
    std::string subtitle = std::string(race_name(player_->race)) + " "
                         + class_name(player_->player_class);
    ctx.text(4, y, subtitle, Color::DarkGray);
    y++;
    std::string info = "Level: " + std::to_string(player_->level)
        + " \xc2\xb7 HP: " + std::to_string(player_->hp) + "/" + std::to_string(player_->effective_max_hp())
        + " \xc2\xb7 XP: " + std::to_string(player_->xp) + "/" + std::to_string(player_->max_xp);
    ctx.text(4, y, info, Color::DarkGray);
    y += 2;



    // ──┤ MAIN ATTRIBUTES ├────┤ Attribute Points: 0 ├──
    draw_section_header(ctx, y, "MAIN ATTRIBUTES");
    {
        // Draw second label right-aligned before divider
        int divider_x = ctx.width() / 2;
        int remaining = player_->attribute_points - total_pending();
        std::string pts = std::to_string(remaining);
        std::string label = " Attribute Points: ";
        // Position: ──┤ label N ├──  ending at divider_x
        int total_len = 2 + 1 + static_cast<int>(label.size()) + static_cast<int>(pts.size()) + 1 + 1; // ──┤labelN├─
        int start_x = divider_x - total_len;
        if (start_x > 0) {
            ctx.put(start_x, y, BoxDraw::RT, Color::DarkGray);
            ctx.text(start_x + 1, y, label, Color::White);
            int num_x = start_x + 1 + static_cast<int>(label.size());
            ctx.text(num_x, y, pts, Color::Green);
            ctx.put(num_x + static_cast<int>(pts.size()), y, ' ');
            ctx.put(num_x + static_cast<int>(pts.size()) + 1, y, BoxDraw::LT, Color::DarkGray);
        }
    }
    y += 2;

    // Primary attribute boxes: single row of 6
    int box_x = 2;
    int box_spacing = 8; // 7 wide + 1 gap
    const auto& a = player_->attributes;
    int primary_base[] = {a.strength, a.agility, a.toughness,
                          a.intelligence, a.willpower, a.luck};
    int remaining_pts = player_->attribute_points - total_pending();

    for (int i = 0; i < 6; ++i) {
        int bx = box_x + i * box_spacing;
        int by = y;
        int display_val = primary_base[i] + pending_points_[i];
        int modifier = (display_val - 10) / 2;
        bool selected = (cursor_ == i);
        draw_stat_box(ctx, bx, by, primary_labels[i], display_val,
                      selected, modifier, pending_points_[i],
                      remaining_pts > 0);
    }

    // Description text below primary boxes — "Name determines ..."
    // Shared description renderer: attribute name in Yellow, rest in DarkGray
    int desc_y = y + 6 + 1;
    auto draw_desc = [&](int dy, const char* attr_name, const char* desc_text) {
        int dx = 2;
        // Draw attribute name in yellow
        std::string name_str(attr_name);
        ctx.text(dx, dy, name_str, Color::Yellow);
        dx += static_cast<int>(name_str.size()) + 1;
        // Draw rest of description in DarkGray, with simple word wrap
        std::string desc(desc_text);
        int max_w = half - 4;
        int line_x = dx;
        size_t i = 0;
        while (i < desc.size()) {
            if (desc[i] == ' ' && line_x - 2 >= max_w) {
                dy++;
                line_x = 2;
                i++; // skip the space
                continue;
            }
            ctx.put(line_x, dy, desc[i], Color::DarkGray);
            line_x++;
            if (line_x - 2 >= max_w) {
                dy++;
                line_x = 2;
            }
            i++;
        }
    };

    if (cursor_ < 6) {
        draw_desc(desc_y, primary_names[cursor_], primary_descriptions[cursor_]);
    }

    // ── SECONDARY ATTRIBUTES ──
    int sec_y = desc_y + 3;
    draw_section_header(ctx, sec_y, "SECONDARY ATTRIBUTES");
    sec_y += 2;

    int sec_values[] = {
        player_->quickness + (a.agility - 10) / 2,
        player_->move_speed + (a.agility - 10) / 4,
        player_->effective_defense(),
        player_->effective_dodge(),
    };

    for (int i = 0; i < 4; ++i) {
        int bx = box_x + i * box_spacing;
        bool selected = (cursor_ == 6 + i);
        draw_stat_box(ctx, bx, sec_y, sec_labels[i], sec_values[i], selected);
    }

    // Description for secondary
    int sec_desc_y = sec_y + 5;
    if (cursor_ >= 6 && cursor_ < 10) {
        int si = cursor_ - 6;
        draw_desc(sec_desc_y, sec_names[si], sec_descriptions[si]);
    }

    // ── RESISTANCES ──
    int res_y = sec_desc_y + 3;
    draw_section_header(ctx, res_y, "RESISTANCES");
    res_y += 2;

    int res_values[] = {
        player_->resistances.acid,
        player_->resistances.electrical,
        player_->resistances.cold,
        player_->resistances.heat,
    };

    for (int i = 0; i < 4; ++i) {
        int bx = box_x + i * box_spacing;
        bool selected = (cursor_ == 10 + i);
        draw_stat_box(ctx, bx, res_y, res_labels[i], res_values[i], selected);
    }

    // Description for resistance
    int res_desc_y = res_y + 5;
    if (cursor_ >= 10 && cursor_ <= 13) {
        int ri = cursor_ - 10;
        draw_desc(res_desc_y, res_names[ri], res_descriptions[ri]);
    }
}

// ─────────────────────────────────────────────────────────────────
// Skills tab
// ─────────────────────────────────────────────────────────────────

std::vector<CharacterScreen::SkillVisItem> CharacterScreen::build_skill_vis() const {
    std::vector<SkillVisItem> vis;
    const auto& catalog = skill_catalog();
    for (int ci = 0; ci < static_cast<int>(catalog.size()); ++ci) {
        vis.push_back({true, ci, -1});
        if (ci < static_cast<int>(skill_cat_expanded_.size()) && skill_cat_expanded_[ci]) {
            for (int si = 0; si < static_cast<int>(catalog[ci].skills.size()); ++si)
                vis.push_back({false, ci, si});
        }
    }
    return vis;
}

void CharacterScreen::draw_skills(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;
    const auto& catalog = skill_catalog();



    // Header bar: ──┤ STR:14 AGI:12 ... ├───┤ Skill Points: 200 ├──
    {
        int divider_x = half;
        // Fill entire line with ─
        for (int x = 0; x < divider_x; ++x)
            ctx.put(x, 0, BoxDraw::H, Color::DarkGray);

        // Left label: attribute overview
        const auto& a = player_->attributes;
        const char* labels[] = {"STR", "AGI", "TOU", "INT", "WIL", "LUC"};
        int vals[] = {a.strength, a.agility, a.toughness,
                      a.intelligence, a.willpower, a.luck};
        std::string attr_str;
        for (int i = 0; i < 6; ++i) {
            if (i > 0) attr_str += " ";
            attr_str += labels[i];
            attr_str += ":";
            attr_str += std::to_string(vals[i]);
        }

        int lx = 1;
        ctx.put(lx, 0, BoxDraw::H, Color::DarkGray);
        ctx.put(lx + 1, 0, BoxDraw::RT, Color::DarkGray);
        ctx.put(lx + 2, 0, ' ');
        // Draw attribute overview with labels in DarkGray, values in White
        int ax = lx + 3;
        for (int i = 0; i < 6; ++i) {
            if (i > 0) { ctx.put(ax, 0, ' '); ax++; }
            std::string lbl = std::string(labels[i]) + ":";
            ctx.text(ax, 0, lbl, Color::DarkGray);
            ax += static_cast<int>(lbl.size());
            std::string val = std::to_string(vals[i]);
            ctx.text(ax, 0, val, Color::White);
            ax += static_cast<int>(val.size());
        }
        ctx.put(ax, 0, ' ');
        ctx.put(ax + 1, 0, BoxDraw::LT, Color::DarkGray);

        // Right label: skill points
        std::string pts = std::to_string(player_->skill_points);
        std::string sp_label = " Skill Points: ";
        int sp_len = 1 + static_cast<int>(sp_label.size()) + static_cast<int>(pts.size()) + 2;
        int sp_x = divider_x - sp_len;
        if (sp_x > ax + 2) {
            ctx.put(sp_x, 0, BoxDraw::RT, Color::DarkGray);
            ctx.text(sp_x + 1, 0, sp_label, Color::White);
            int num_x = sp_x + 1 + static_cast<int>(sp_label.size());
            ctx.text(num_x, 0, pts, Color::Green);
            ctx.put(num_x + static_cast<int>(pts.size()), 0, ' ');
            ctx.put(num_x + static_cast<int>(pts.size()) + 1, 0, BoxDraw::LT, Color::DarkGray);
        }
    }

    auto visible = build_skill_vis();

    // Clamp cursor
    if (skill_cursor_ >= static_cast<int>(visible.size()))
        skill_cursor_ = static_cast<int>(visible.size()) - 1;
    if (skill_cursor_ < 0) skill_cursor_ = 0;

    // Draw list on left side
    int y = 2;
    int list_h = ctx.height() - 2;
    const SkillDef* selected_skill = nullptr;
    int selected_cat_idx = -1; // category index if a category row is selected

    for (int i = 0; i < static_cast<int>(visible.size()); ++i) {
        if (y - 2 >= list_h) break;
        const auto& ve = visible[i];
        bool selected = (skill_cursor_ == i);

        if (ve.is_cat) {
            const auto& cat = catalog[ve.ci];
            bool unlocked = false;
            for (auto sid : player_->learned_skills)
                if (sid == cat.unlock_id) { unlocked = true; break; }

            // Category header: ──┤ [+] Name ├──────── cost
            if (selected) {
                ctx.put(0, y, '>', Color::Yellow);
                selected_cat_idx = ve.ci;
            }

            // Build: ──┤ [+/-] Name ├────
            int lx = 1;
            ctx.put(lx, y, BoxDraw::H, Color::DarkGray);
            ctx.put(lx + 1, y, BoxDraw::RT, Color::DarkGray);
            lx += 2;

            std::string toggle = skill_cat_expanded_[ve.ci] ? " [-] " : " [+] ";
            ctx.text(lx, y, toggle, Color::White);
            lx += static_cast<int>(toggle.size());

            Color name_color = unlocked ? Color::Green : Color::DarkGray;
            if (selected) name_color = Color::White;
            ctx.text(lx, y, cat.name, name_color);
            lx += static_cast<int>(cat.name.size());

            ctx.put(lx, y, ' ');
            ctx.put(lx + 1, y, BoxDraw::LT, Color::DarkGray);
            int trail_start = lx + 2;

            // Cost right-aligned (only for locked categories)
            int cost_end = half - 1;
            if (!unlocked) {
                std::string cost = std::to_string(cat.sp_cost) + " SP";
                int cx_pos = cost_end - static_cast<int>(cost.size());
                ctx.text(cx_pos, y, cost, Color::Yellow);
                cost_end = cx_pos - 1;
            }

            // Fill ── between ├ and cost/divider
            for (int fx = trail_start; fx <= cost_end; ++fx)
                ctx.put(fx, y, BoxDraw::H, Color::DarkGray);
        } else {
            // Skill entry
            const auto& sk = catalog[ve.ci].skills[ve.si];
            bool learned = false;
            for (auto sid : player_->learned_skills) {
                if (sid == sk.id) { learned = true; break; }
            }

            if (selected) {
                ctx.put(3, y, '>', Color::Yellow);
                selected_skill = &sk;
            }

            // Learned marker
            if (learned) {
                ctx.put(5, y, '*', Color::Green);
            }

            // Check if affordable/meets requirements
            bool can_afford = player_->skill_points >= sk.sp_cost;
            bool meets_req = true;
            if (sk.attribute_req > 0 && sk.attribute_name) {
                const auto& a = player_->attributes;
                std::string attr(sk.attribute_name);
                int val = 0;
                if (attr == "Agility") val = a.agility;
                else if (attr == "Strength") val = a.strength;
                else if (attr == "Toughness") val = a.toughness;
                else if (attr == "Intelligence") val = a.intelligence;
                else if (attr == "Willpower") val = a.willpower;
                else if (attr == "Luck") val = a.luck;
                if (val < sk.attribute_req) meets_req = false;
            }

            Color name_color;
            if (learned) name_color = Color::Green;
            else if (selected) name_color = Color::White;
            else if (!can_afford || !meets_req) name_color = Color::DarkGray;
            else name_color = Color::Default;

            ctx.text(7, y, sk.name, name_color);

            // SP cost right-aligned
            std::string cost = std::to_string(sk.sp_cost) + " SP";
            int cx = half - 2 - static_cast<int>(cost.size());
            if (sk.attribute_req > 0 && sk.attribute_name) {
                std::string req = std::to_string(sk.attribute_req)
                                + std::string(sk.attribute_name).substr(0, 3);
                ctx.text(cx - static_cast<int>(req.size()) - 1, y, req,
                         meets_req ? Color::DarkGray : Color::Red);
            }
            ctx.text(cx, y, cost, learned ? Color::DarkGray : (can_afford ? Color::Yellow : Color::Red));
        }
        y++;
    }

    // Detail panel on right
    int rx = half + 2;
    int rw = w - half - 3;

    // Helper: word-wrap text
    auto wrap_text = [&](int start_y, const std::string& text, Color color) {
        int dy = start_y;
        int line_x = 0;
        for (size_t i = 0; i < text.size(); ++i) {
            if (text[i] == ' ' && line_x >= rw) {
                dy++;
                line_x = 0;
                continue;
            }
            ctx.put(rx + line_x, dy, text[i], color);
            line_x++;
            if (line_x >= rw) {
                dy++;
                line_x = 0;
            }
        }
        return dy;
    };

    if (selected_cat_idx >= 0 && !selected_skill) {
        // Category detail
        const auto& cat = catalog[selected_cat_idx];
        bool unlocked = false;
        for (auto sid : player_->learned_skills)
            if (sid == cat.unlock_id) { unlocked = true; break; }

        ctx.text(rx, 2, cat.name, Color::White);
        ctx.text(rx, 3, unlocked ? "[Learned]" : "[Unlearned]",
                 unlocked ? Color::Green : Color::Red);

        std::string cost_str = ":: " + std::to_string(cat.sp_cost) + " SP ::";
        ctx.text(rx, 5, cost_str, Color::Yellow);

        wrap_text(7, cat.description, Color::DarkGray);
    } else if (selected_skill) {
        const auto& sk = *selected_skill;
        bool learned = false;
        for (auto sid : player_->learned_skills) {
            if (sid == sk.id) { learned = true; break; }
        }

        ctx.text(rx, 2, sk.name, Color::White);
        ctx.text(rx, 3, sk.passive ? "[Passive]" : "[Active]", Color::Cyan);
        ctx.text(rx, 4, "Cost: " + std::to_string(sk.sp_cost) + " SP", Color::Yellow);

        int dy = 5;
        if (sk.attribute_req > 0 && sk.attribute_name) {
            std::string req = "Requires: " + std::to_string(sk.attribute_req)
                            + " " + sk.attribute_name;
            ctx.text(rx, dy, req, Color::DarkGray);
            dy++;
        }
        dy++;

        if (learned) {
            ctx.text(rx, dy, "LEARNED", Color::Green);
            dy += 2;
        }

        wrap_text(dy, sk.description, Color::DarkGray);
    }
}

// ─────────────────────────────────────────────────────────────────
// Equipment tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_equipment(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;



    // Right side header: credits and weight
    std::string money_str = std::to_string(player_->money) + "$";
    std::string weight_str = std::to_string(player_->inventory.total_weight())
                           + "/" + std::to_string(player_->inventory.max_carry_weight) + " lb";
    ctx.text(w - 1 - static_cast<int>(weight_str.size()), 0, weight_str, Color::Cyan);
    ctx.text(w - 1 - static_cast<int>(weight_str.size()) - 3 - static_cast<int>(money_str.size()),
             0, money_str, Color::Yellow);

    // Paper doll on the left — CoQ-style with connector lines
    // Each box: 7 wide × 3 tall (border + item glyph centered)
    // Slot name centered below the box
    // Vertical lines connect: Face → Head → Body → Back → Feet (center spine)

    // Box dimensions
    constexpr int bw = 7;  // box width
    constexpr int bh = 3;  // box height
    constexpr int slot_h = 5; // box (3) + label (1) + gap (1)
    int cx = (half - 1) / 2; // center x of left half
    int dy = 1; // start y

    // Slot positions: {x, y, slot} — x is left edge of box
    // Center column: cx - bw/2
    // Left column: cx - bw/2 - bw - 2
    // Right column: cx - bw/2 + bw + 2
    int col_c = cx - bw / 2;
    int col_l = col_c - bw - 2;
    int col_r = col_c + bw + 2;
    int col_ll = col_l - bw - 2; // far left
    int col_rr = col_r + bw + 2; // far right

    struct SlotPos { int x; int y; EquipSlot slot; const char* label; };
    SlotPos positions[] = {
        // Row 0: Face (center)
        {col_c,  dy,              EquipSlot::Face,      "Face"},
        // Row 1: Head (center)
        {col_c,  dy + slot_h,     EquipSlot::Head,      "Head"},
        // Row 2: L.Hand, L.Arm, Body, R.Arm, R.Hand
        {col_ll, dy + slot_h * 2, EquipSlot::LeftHand,  "L.Hand"},
        {col_l,  dy + slot_h * 2, EquipSlot::LeftArm,   "L.Arm"},
        {col_c,  dy + slot_h * 2, EquipSlot::Body,      "Body"},
        {col_r,  dy + slot_h * 2, EquipSlot::RightArm,  "R.Arm"},
        {col_rr, dy + slot_h * 2, EquipSlot::RightHand, "R.Hand"},
        // Row 3: Back (center)
        {col_c,  dy + slot_h * 3, EquipSlot::Back,      "Back"},
        // Row 4: Feet (center)
        {col_c,  dy + slot_h * 4, EquipSlot::Feet,      "Feet"},
        // Row 5: Thrown, Missile
        {col_l,  dy + slot_h * 5, EquipSlot::Thrown,    "Thrown"},
        {col_r,  dy + slot_h * 5, EquipSlot::Missile,   "Missile"},
    };

    // Draw connector lines (center spine: between Face→Head→Body→Back→Feet)
    Color line_color = Color::DarkGray;
    auto draw_vconn = [&](int row_top, int row_bot) {
        // Vertical line from bottom of top box to top of bottom box
        int x = col_c + bw / 2;
        int y_start = positions[row_top].y + bh;     // just below top box
        int y_end = positions[row_bot].y;             // just above bottom box
        for (int vy = y_start; vy < y_end; ++vy) {
            ctx.put(x, vy, BoxDraw::V, line_color);
        }
    };
    // Face(0) → Head(1) → Body(4) → Back(7) → Feet(8)
    draw_vconn(0, 1);
    draw_vconn(1, 4);
    draw_vconn(4, 7);
    draw_vconn(7, 8);

    // Horizontal connectors on Body row: L.Hand─L.Arm─Body─R.Arm─R.Hand
    {
        int row_y = positions[4].y + bh / 2; // middle of Body row
        // L.Hand to L.Arm
        for (int hx = col_ll + bw; hx < col_l; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // L.Arm to Body
        for (int hx = col_l + bw; hx < col_c; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // Body to R.Arm
        for (int hx = col_c + bw; hx < col_r; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
        // R.Arm to R.Hand
        for (int hx = col_r + bw; hx < col_rr; ++hx)
            ctx.put(hx, row_y, BoxDraw::H, line_color);
    }

    // Draw each slot box
    for (int i = 0; i < equip_slot_count; ++i) {
        const auto& sp = positions[i];
        bool selected = (equip_focus_ == EquipFocus::PaperDoll && equip_cursor_ == i);
        Color border_color = selected ? Color::Yellow : Color::DarkGray;
        const auto& item = player_->equipment.slot_ref(sp.slot);

        int bx = sp.x;
        int by = sp.y;

        // Box border (7 wide × 3 tall)
        ctx.put(bx, by, BoxDraw::TL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by, BoxDraw::TR, border_color);

        ctx.put(bx, by + 1, BoxDraw::V, border_color);
        // Content: item glyph centered, or empty
        if (item) {
            int mid = bx + bw / 2;
            ctx.put(mid, by + 1, item->glyph, rarity_color(item->rarity));
        } else {
            ctx.text(bx + 2, by + 1, "   ", Color::DarkGray);
        }
        ctx.put(bx + bw - 1, by + 1, BoxDraw::V, border_color);

        ctx.put(bx, by + 2, BoxDraw::BL, border_color);
        for (int j = 1; j < bw - 1; ++j) ctx.put(bx + j, by + 2, BoxDraw::H, border_color);
        ctx.put(bx + bw - 1, by + 2, BoxDraw::BR, border_color);

        // Label centered below box
        std::string label(sp.label);
        int label_x = bx + (bw - static_cast<int>(label.size())) / 2;
        ctx.text(label_x, by + 3, label, selected ? Color::Yellow : Color::Cyan);
    }

    // Bonuses at the bottom of left side
    int bonus_y = dy + slot_h * 6 + 1;
    if (bonus_y < ctx.height() - 3) {
        draw_section_header(ctx, bonus_y, "BONUSES");
        auto mods = player_->equipment.total_modifiers();
        std::string line1 = "ATK +" + std::to_string(mods.attack)
                          + "  DEF +" + std::to_string(mods.defense)
                          + "  HP +" + std::to_string(mods.max_hp);
        std::string line2 = "VIS +" + std::to_string(mods.view_radius)
                          + "  QCK +" + std::to_string(mods.quickness);
        ctx.text(2, bonus_y + 1, line1, Color::DarkGray);
        ctx.text(2, bonus_y + 2, line2, Color::DarkGray);
    }

    // Right side: categorized inventory
    int ry = 2;
    int rx = half + 2;
    int rw = w - half - 3;

    auto& items = player_->inventory.items;
    if (items.empty()) {
        ctx.text(rx, ry, "Inventory empty.", Color::DarkGray);
        return;
    }

    for (int i = 0; i < static_cast<int>(items.size()); ++i) {
        if (ry >= ctx.height() - 1) break;
        const auto& item = items[i];
        bool selected = (equip_focus_ == EquipFocus::Inventory && inv_cursor_ == i);

        if (selected) ctx.put(rx - 1, ry, '>', Color::Yellow);

        ctx.put(rx, ry, item.glyph, rarity_color(item.rarity));
        draw_item_name(ctx, rx + 2, ry, item, selected);

        std::string price = std::to_string(item.sell_value) + "$";
        int px = half + rw - static_cast<int>(price.size());
        ctx.text(px, ry, price, Color::Yellow);

        ry++;
    }
}

// ─────────────────────────────────────────────────────────────────
// Reputation tab
// ─────────────────────────────────────────────────────────────────

// ─────────────────────────────────────────────────────────────────
// Tinkering tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_tinkering(DrawContext& ctx) {
    // Gate: require Tinkering category unlocked
    if (!player_has_skill(*player_, SkillId::Cat_Tinkering)) {
        ctx.text_center(ctx.height() / 2 - 1, "Tinkering workbench unavailable.", Color::DarkGray);
        ctx.text_center(ctx.height() / 2 + 1, "Learn the Tinkering skill to use this station.", Color::DarkGray);
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
        std::string display = std::string(1, workbench_item_->glyph) + " " + workbench_item_->name;
        if (static_cast<int>(display.size()) > wb_w - 4) display = display.substr(0, wb_w - 4);
        int nx = wb_x + (wb_w - static_cast<int>(display.size())) / 2;
        ctx.put(nx, wb_y + 1, workbench_item_->glyph, rarity_color(workbench_item_->rarity));
        ctx.text(nx + 2, wb_y + 1, workbench_item_->name, rarity_color(workbench_item_->rarity));
    } else {
    {
        std::string empty_msg = "Empty, no item";
        int emx = wb_x + (wb_w - static_cast<int>(empty_msg.size())) / 2;
        ctx.text(emx, wb_y + 1, empty_msg, Color::DarkGray);
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
        ctx.text(lx, sy + 3, label, selected ? Color::Yellow : Color::DarkGray);

        // Content
        if (locked) {
        {
            int lpad = (slot_w - 2 - 6) / 2; // center "locked" (6 chars) in inner width
            ctx.text(sx + 1 + lpad, sy + 1, "locked", Color::DarkGray);
        }
        } else if (workbench_item_ && si < static_cast<int>(workbench_item_->enhancements.size())
                   && workbench_item_->enhancements[si].filled) {
            const auto& enh = workbench_item_->enhancements[si];
            std::string bonus;
            if (enh.bonus.attack) bonus = "+" + std::to_string(enh.bonus.attack) + "ATK";
            else if (enh.bonus.defense) bonus = "+" + std::to_string(enh.bonus.defense) + "DEF";
            else if (enh.bonus.view_radius) bonus = "+" + std::to_string(enh.bonus.view_radius) + "VIS";
            Color enh_color = enh.committed ? Color::Green : Color::Yellow;
        {
            int bpad = (slot_w - 2 - static_cast<int>(bonus.size())) / 2;
            ctx.text(sx + 1 + bpad, sy + 1, bonus, enh_color);
        }
        } else {
        {
            int epad = (slot_w - 2 - 5) / 2; // center "empty" (5 chars)
            ctx.text(sx + 1 + epad, sy + 1, "empty", Color::DarkGray);
        }
        }
    }

    // Synthesizer section
    int synth_y = slot_y + 5;
    draw_section_header(ctx, synth_y, "SYNTHESIZER");
    synth_y += 2;

    bool has_synthesize = player_has_skill(*player_, SkillId::Synthesize);
    if (!has_synthesize) {
        ctx.text(3, synth_y, "Requires Synthesize skill to use.", Color::DarkGray);
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
                ctx.text(nx, by + 1, name, Color::Cyan);
            } else {
            {
                std::string placeholder = (bi == 0) ? "Blueprint 1" : "Blueprint 2";
                int px = bx + (bp_w - static_cast<int>(placeholder.size())) / 2;
                ctx.text(px, by + 1, placeholder, Color::DarkGray);
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
                ctx.text(3, synth_y, "Result: ", Color::DarkGray);
                ctx.text(11, synth_y, recipe->result_name, Color::Green);
                synth_y++;

                // Show cost
                std::string cost;
                for (int m = 0; m < 4; ++m) {
                    if (recipe->material_cost[m] <= 0) continue;
                    if (!cost.empty()) cost += ", ";
                    const char* names[] = {"Nano-Fiber", "Power Core", "Circuit Board", "Alloy Ingot"};
                    cost += std::to_string(recipe->material_cost[m]) + "x " + names[m];
                }
                ctx.text(3, synth_y, "Cost: " + cost, Color::DarkGray);
                synth_y++;
                ctx.text(3, synth_y, "[y] Synthesize", Color::Yellow);
            } else {
                ctx.text(3, synth_y, "No known recipe.", Color::DarkGray);
            }
        }
        synth_y += 2;
    } else {
        ctx.text(3, synth_y, "Learn 2+ blueprints to synthesize.", Color::DarkGray);
        synth_y += 2;
    }

    // Materials section
    int mat_y = synth_y;
    draw_section_header(ctx, mat_y, "MATERIALS");
    mat_y += 2;
    int mx = 3;
    for (const auto& item : player_->inventory.items) {
        if (item.type == ItemType::CraftingMaterial) {
            std::string label = item.name + " x" + std::to_string(item.stack_count);
            ctx.put(mx, mat_y, '+', item.color);
            ctx.text(mx + 2, mat_y, label, Color::DarkGray);
            mat_y++;
        }
    }
    if (mat_y == slot_y + 7) {
        ctx.text(3, mat_y, "No crafting materials.", Color::DarkGray);
    }

    // Right panel — split into upper (detail/actions) and lower (catalog)
    int rx = half + 3;
    int rw_avail = w - half - 4;

    // Calculate catalog height (fixed at bottom)
    int catalog_lines = 0;
    for (const auto& bp : player_->learned_blueprints) {
        catalog_lines++; // blueprint name
        for (const auto& recipe : synthesis_recipes()) {
            if (bp.name == recipe.blueprint_1 || bp.name == recipe.blueprint_2)
                catalog_lines++;
        }
        catalog_lines++; // gap
    }
    int catalog_h = std::min(catalog_lines + 3, ctx.height() / 2); // cap at half height
    int split_y = ctx.height() - catalog_h;

    // --- Upper pane: detail + actions ---
    int ry = 1;

    if (workbench_item_) {
        const auto& item = *workbench_item_;
        ctx.text(rx, ry, item.name, rarity_color(item.rarity));
        ry++;
        ctx.text(rx, ry, rarity_name(item.rarity), rarity_color(item.rarity));
        ry += 2;

        if (item.modifiers.attack)
            ctx.text(rx, ry++, "ATK: +" + std::to_string(item.modifiers.attack), Color::Red);
        if (item.modifiers.defense)
            ctx.text(rx, ry++, "DEF: +" + std::to_string(item.modifiers.defense), Color::Blue);

        if (item.max_durability > 0) {
            ry++;
            ctx.text(rx, ry, "Durabl: ", Color::DarkGray);
            int bar_w = std::min(14, rw_avail - 14);
            if (bar_w > 0) {
                Color dur_color = (item.durability * 3 > item.max_durability) ? Color::Green : Color::Red;
                ctx.bar(rx + 8, ry, bar_w, item.durability, item.max_durability, dur_color);
            }
            std::string dur = std::to_string(item.durability) + "/" + std::to_string(item.max_durability);
            ctx.text(rx + 8 + bar_w + 1, ry, dur, Color::Green);
            ry++;
        }

        // Enhancement slot details
        if (ry + 5 < split_y) {
            ry++;
            for (int si = 0; si < 3; ++si) {
                bool locked = (si >= item.enhancement_slots);
                std::string slot_label = "[" + std::to_string(si + 1) + "] ";
                ctx.text(rx, ry, slot_label, Color::White);
                if (locked) ctx.text(rx + 4, ry, "locked", Color::DarkGray);
                else if (si < static_cast<int>(item.enhancements.size()) && item.enhancements[si].filled)
                {
                    const auto& enh = item.enhancements[si];
                    std::string label = enh.material_name;
                    if (!enh.committed) label += " (pending)";
                    ctx.text(rx + 4, ry, label, enh.committed ? Color::Green : Color::Yellow);
                }
                else ctx.text(rx + 4, ry, "empty", Color::DarkGray);
                ry++;
            }
        }

        // Actions
        if (ry + 5 < split_y) {
            ry += 2;
            bool has_repair = player_has_skill(*player_, SkillId::BasicRepair);
            bool has_analyze = player_has_skill(*player_, SkillId::Cat_Tinkering);
            bool has_salvage = player_has_skill(*player_, SkillId::Disassemble);

            int cost = repair_cost(item);
            std::string repair_label = "[r] Repair";
            if (cost > 0) repair_label += "  (" + std::to_string(cost) + " Nano-Fiber)";
            ctx.text(rx, ry++, repair_label, has_repair ? Color::White : Color::DarkGray);
            ctx.text(rx, ry++, "[a] Analyze", has_analyze ? Color::White : Color::DarkGray);
            ctx.text(rx, ry++, "[s] Salvage", has_salvage ? Color::White : Color::DarkGray);

            bool pending = has_pending_enhancements(item);
            ctx.text(rx, ry++, "[f] Assemble", pending ? Color::Yellow : Color::DarkGray);
            ctx.text(rx, ry++, "[x] Clear slot", Color::DarkGray);
        }
    } else {
        ctx.text(rx, 3, "Place an item on the", Color::DarkGray);
        ctx.text(rx, 4, "workbench to begin.", Color::DarkGray);
        ctx.text(rx, 6, "1. Select workbench", Color::DarkGray);
        ctx.text(rx, 7, "2. Press [Space] to place", Color::DarkGray);
        ctx.text(rx, 8, "3. Use [r] [a] [s] actions", Color::DarkGray);
        ctx.text(rx, 9, "4. Select slots to enhance", Color::DarkGray);
    }

    // --- Horizontal separator ---
    for (int sx = half + 1; sx < w; ++sx)
        ctx.put(sx, split_y, BoxDraw::H, Color::DarkGray);

    // --- Lower pane: Blueprint Catalog (always visible) ---
    int cy = split_y + 1;
    ctx.text(rx, cy, "BLUEPRINT CATALOG", Color::White);
    cy += 2;

    if (player_->learned_blueprints.empty()) {
        ctx.text(rx, cy, "No blueprints learned.", Color::DarkGray);
    } else {
        for (const auto& bp : player_->learned_blueprints) {
            if (cy >= ctx.height() - 1) break;
            ctx.put(rx, cy, '+', Color::Cyan);
            ctx.text(rx + 2, cy, bp.name, Color::Cyan);
            cy++;

            for (const auto& recipe : synthesis_recipes()) {
                if (cy >= ctx.height() - 1) break;
                if (bp.name == recipe.blueprint_1 || bp.name == recipe.blueprint_2) {
                    const char* other = (bp.name == recipe.blueprint_1)
                        ? recipe.blueprint_2 : recipe.blueprint_1;
                    bool has_other = false;
                    for (const auto& obp : player_->learned_blueprints)
                        if (obp.name == other) { has_other = true; break; }

                    std::string line = "  + " + std::string(other) + " = " + recipe.result_name;
                    ctx.text(rx, cy, line, has_other ? Color::Green : Color::DarkGray);
                    cy++;
                }
            }
            cy++;
        }
    }
}

// ─────────────────────────────────────────────────────────────────
// Journal tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_journal(DrawContext& ctx) {
    int w = ctx.width();
    int half = w / 2;

    if (player_->journal.empty()) {
        ctx.text_center(ctx.height() / 2, "No entries yet.", Color::DarkGray);
        return;
    }

    // Clamp cursor
    int count = static_cast<int>(player_->journal.size());
    if (journal_cursor_ >= count) journal_cursor_ = count - 1;
    if (journal_cursor_ < 0) journal_cursor_ = 0;

    // Left panel: entry list (newest first)
    int y = 1;
    for (int i = count - 1; i >= 0; --i) {
        if (y >= ctx.height() - 1) break;
        const auto& entry = player_->journal[i];
        bool selected = (journal_cursor_ == i);

        if (selected) ctx.put(0, y, '>', Color::Yellow);

        // Category icon
        Color cat_color;
        char cat_icon;
        switch (entry.category) {
            case JournalCategory::Blueprint:  cat_icon = '+'; cat_color = Color::Cyan; break;
            case JournalCategory::Discovery:  cat_icon = '*'; cat_color = Color::Green; break;
            case JournalCategory::Encounter:  cat_icon = '!'; cat_color = Color::Red; break;
            case JournalCategory::Event:      cat_icon = '.'; cat_color = Color::White; break;
        }
        ctx.put(2, y, cat_icon, cat_color);

        // Title (truncated to fit left half)
        std::string title = entry.title;
        int max_title = half - 5;
        if (static_cast<int>(title.size()) > max_title)
            title = title.substr(0, max_title);
        ctx.text(4, y, title, selected ? Color::White : Color::DarkGray);

        y++;

        // Timestamp below title
        if (y < ctx.height() - 1) {
            ctx.text(4, y, entry.timestamp, Color::DarkGray);
            y += 2; // extra gap between entries
        }
    }

    // Right panel: selected entry detail
    if (journal_cursor_ >= 0 && journal_cursor_ < count) {
        const auto& entry = player_->journal[journal_cursor_];
        int rx = half + 3;
        int rw = w - half - 4;
        int ry = 1;

        // Timestamp
        ctx.text(rx, ry, entry.timestamp, Color::DarkGray);
        ry += 2;

        // Title
        ctx.text(rx, ry, entry.title, Color::White);
        ry += 2;

        // Technical section
        if (!entry.technical.empty()) {
            // Word-wrap the technical text
            int line_x = 0;
            for (size_t i = 0; i < entry.technical.size() && ry < ctx.height() - 2; ++i) {
                if (entry.technical[i] == '\n') {
                    ry++;
                    line_x = 0;
                    continue;
                }
                if (entry.technical[i] == ' ' && line_x >= rw) {
                    ry++;
                    line_x = 0;
                    continue;
                }
                ctx.put(rx + line_x, ry, entry.technical[i], Color::Cyan);
                line_x++;
                if (line_x >= rw) {
                    ry++;
                    line_x = 0;
                }
            }
            ry += 2;
        }

        // Separator
        if (ry < ctx.height() - 4) {
            ctx.text(rx, ry, "--- Commander's Notes ---", Color::DarkGray);
            ry += 2;
        }

        // Personal notes
        if (!entry.personal.empty() && ry < ctx.height() - 2) {
            int line_x = 0;
            for (size_t i = 0; i < entry.personal.size() && ry < ctx.height() - 1; ++i) {
                if (entry.personal[i] == '\n') {
                    ry++;
                    line_x = 0;
                    continue;
                }
                if (entry.personal[i] == ' ' && line_x >= rw) {
                    ry++;
                    line_x = 0;
                    continue;
                }
                ctx.put(rx + line_x, ry, entry.personal[i], Color::Default);
                line_x++;
                if (line_x >= rw) {
                    ry++;
                    line_x = 0;
                }
            }
        }
    }
}

void CharacterScreen::draw_reputation(DrawContext& ctx) {
    if (player_->reputation.empty()) {
        ctx.text(2, 2, "No faction standings.", Color::DarkGray);
        return;
    }

    int y = 2;
    for (int i = 0; i < static_cast<int>(player_->reputation.size()); ++i) {
        if (y >= ctx.height() - 2) break;
        const auto& f = player_->reputation[i];
        bool selected = (cursor_ == i);

        if (selected) ctx.put(1, y, '>', Color::Yellow);
        ctx.text(3, y, f.faction_name, selected ? Color::White : Color::Default);

        std::string rep = "Reputation: " + std::to_string(f.reputation);
        Color rep_color = f.reputation > 0 ? Color::Green
                        : f.reputation < 0 ? Color::Red
                        : Color::DarkGray;
        ctx.text(ctx.width() - 2 - static_cast<int>(rep.size()), y, rep, rep_color);

        // Flavor text
        y++;
        std::string flavor;
        if (f.reputation >= 50) flavor = "They consider you a trusted ally.";
        else if (f.reputation > 0) flavor = "They view you with curiosity.";
        else if (f.reputation == 0) flavor = "They are indifferent toward you.";
        else if (f.reputation > -50) flavor = "They are wary of you.";
        else flavor = "They are hostile toward you.";
        ctx.text(5, y, flavor, Color::DarkGray);
        y += 2;
    }
}

// ─────────────────────────────────────────────────────────────────
// Stub tab
// ─────────────────────────────────────────────────────────────────

void CharacterScreen::draw_stub(DrawContext& ctx, const char* message) {
    ctx.text_center(ctx.height() / 2, message, Color::DarkGray);
}

} // namespace astra

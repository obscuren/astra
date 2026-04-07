#include "astra/dev_console.h"
#include "astra/animation.h"
#include "astra/biome_profile.h"
#include "astra/effect.h"
#include "astra/game.h"
#include "astra/item_defs.h"
#include "astra/lore_generator.h"
#include "astra/tilemap.h"

#include <sstream>

namespace astra {

void DevConsole::toggle() {
    open_ = !open_;
    input_.clear();
}

void DevConsole::log(const std::string& msg) {
    output_.push_back(msg);
    if (output_.size() > max_output_) {
        output_.pop_front();
    }
}

bool DevConsole::handle_input(int key, Game& game) {
    if (!open_) return false;

    switch (key) {
        case 27: // Esc
            open_ = false;
            return true;
        case '\n': case '\r':
            if (!input_.empty()) {
                history_.push_back(input_);
                if (history_.size() > max_history_)
                    history_.pop_front();
                log("> " + input_);
                execute_command(input_, game);
                input_.clear();
                scroll_ = 0;
                history_idx_ = -1;
            }
            return true;
        case 127: case 8:
            if (!input_.empty()) input_.pop_back();
            return true;
        case KEY_UP: {
            int sz = static_cast<int>(history_.size());
            if (sz > 0) {
                if (history_idx_ < 0) history_idx_ = sz;
                if (history_idx_ > 0) {
                    --history_idx_;
                    input_ = history_[history_idx_];
                }
            }
            return true;
        }
        case KEY_DOWN: {
            int sz = static_cast<int>(history_.size());
            if (history_idx_ >= 0) {
                ++history_idx_;
                if (history_idx_ >= sz) {
                    history_idx_ = -1;
                    input_.clear();
                } else {
                    input_ = history_[history_idx_];
                }
            }
            return true;
        }
        case KEY_PAGE_UP:
            scroll_++;
            return true;
        case KEY_PAGE_DOWN:
            if (scroll_ > 0) scroll_--;
            return true;
        default:
            if (key >= 32 && key < 127) {
                input_ += static_cast<char>(key);
            }
            return true;
    }
}

void DevConsole::execute_command(const std::string& cmd, Game& game) {
    std::vector<std::string> args;
    std::string token;
    for (char c : cmd) {
        if (c == ' ') {
            if (!token.empty()) { args.push_back(token); token.clear(); }
        } else {
            token += c;
        }
    }
    if (!token.empty()) args.push_back(token);
    if (args.empty()) return;

    auto& player = game.player();
    const auto& verb = args[0];

    if (verb == "help") {
        log("Commands:");
        log("  warp random        - warp to random map");
        log("  warp stamp <type>  - POI stamp test (ruins, ship, outpost, cave, settlement, landing)");
        log("  give hp <n>        - set HP");
        log("  give xp <n>        - set XP");
        log("  give money <n>     - set credits");
        log("  give sp <n>        - set skill points");
        log("  give ap <n>        - set attribute points");
        log("  give rep <faction> <n> - set faction reputation");
        log("  give ship <component>  - install ship component (engine/hull/navi/shield)");
        log("  set invuln         - toggle invulnerability");
        log("  set level <n>      - set player level");
        log("  effect burn <dur>  - apply burn effect");
        log("  effect regen <dur> - apply regen effect");
        log("  effect clear       - remove all effects");
        log("  kill all           - kill all hostile NPCs");
        log("  quest kill         - random kill quest");
        log("  quest fetch        - random fetch quest");
        log("  quest deliver      - random deliver quest");
        log("  quest scout        - random scout quest");
        log("  quest story        - The Missing Hauler");
        log("  heal               - full heal");
        log("  bearings           - regain bearings if lost");
        log("  lore list           - list lore-annotated systems");
        log("  lore warp <feature> - warp to system (beacon/megastructure/terraformed/scarred/battle/weapon/plague/tier1-3)");
        log("  history             - show world lore history");
        log("  editor             - open map editor");
        log("  clear              - clear console");
    }
    else if (verb == "clear") {
        clear();
    }
    else if (verb == "heal") {
        player.hp = player.effective_max_hp();
        log("HP restored to " + std::to_string(player.hp));
    }
    else if (verb == "flash") {
        game.animations().spawn_effect(anim_damage_flash, player.x, player.y);
        log("Spawned damage flash at player (" + std::to_string(player.x) + "," + std::to_string(player.y) +
            "). Active effects: " + std::to_string(game.animations().has_active_effects() ? 1 : 0));
    }
    else if (verb == "editor") {
        game.map_editor().open(game);
        if (game.map_editor().is_open()) {
            log("Map editor opened.");
        }
    }
    else if (verb == "bearings") {
        if (game.lost()) {
            game.set_lost(false);
            log("Bearings regained.");
        } else {
            log("You're not lost.");
        }
    }
    else if (verb == "warp" && args.size() >= 2) {
        if (args[1] == "random") {
            game.dev_command_warp_random();
            log("Warped to random map.");
        } else if (args[1] == "stamp" && args.size() >= 3) {
            static const std::pair<const char*, Tile> stamps[] = {
                {"ruins", Tile::OW_Ruins}, {"ship", Tile::OW_CrashedShip},
                {"outpost", Tile::OW_Outpost}, {"cave", Tile::OW_CaveEntrance},
                {"settlement", Tile::OW_Settlement}, {"landing", Tile::OW_Landing},
            };
            bool found = false;
            for (const auto& [name, tile] : stamps) {
                if (args[2] == name) {
                    game.dev_command_warp_stamp(tile);
                    log("Warped to " + std::string(name) + " stamp.");
                    found = true;
                    break;
                }
            }
            if (!found) log("Unknown stamp: " + args[2]);
        } else {
            log("Usage: warp random | warp stamp <type>");
        }
    }
    else if (verb == "biome_test" && args.size() >= 2) {
        Biome biome;
        if (!parse_biome(args[1], biome)) {
            log("Unknown biome: " + args[1]);
            log("Options: grassland, forest, jungle, sandy, rocky, volcanic,");
            log("  aquatic, ice, fungal, crystal, corroded,");
            log("  alien_crystalline, alien_organic, alien_geometric,");
            log("  alien_void, alien_light, scarred_scorched, scarred_glassed");
            return;
        }
        int layer = 0;
        if (args.size() >= 3) {
            try { layer = std::stoi(args[2]); } catch (...) {
                log("Invalid layer: " + args[2]);
                return;
            }
        }
        game.dev_command_biome_test(biome, layer);
        log("Biome test: " + args[1] + " (360x150, layer " + std::to_string(layer) + ")");
    }
    else if (verb == "give" && args.size() >= 3 && args[1] == "ship") {
        Item item;
        if (args[2] == "engine") item = build_engine_coil_mk1();
        else if (args[2] == "hull") item = build_hull_plate();
        else if (args[2] == "navi") item = build_navi_computer_mk2();
        else if (args[2] == "shield") item = build_shield_generator();
        else {
            log("Unknown component: " + args[2] + ". Options: engine, hull, navi, shield");
            return;
        }
        log("Added " + item.name + " to ship cargo.");
        player.ship.cargo.push_back(std::move(item));
    }
    else if (verb == "give" && args.size() >= 3) {
        int val = 0;
        try { val = std::stoi(args[2]); } catch (...) {
            log("Invalid number: " + args[2]);
            return;
        }
        if (args[1] == "hp") {
            player.hp = val;
            if (player.hp > player.effective_max_hp()) player.hp = player.effective_max_hp();
            log("HP set to " + std::to_string(player.hp));
        } else if (args[1] == "xp") {
            player.xp = val;
            log("XP set to " + std::to_string(player.xp));
            game.dev_command_level_up();
        } else if (args[1] == "money") {
            player.money = val;
            log("Credits set to " + std::to_string(player.money));
        } else if (args[1] == "sp") {
            player.skill_points = val;
            log("SP set to " + std::to_string(player.skill_points));
        } else if (args[1] == "ap") {
            player.attribute_points = val;
            log("AP set to " + std::to_string(player.attribute_points));
        } else if (args[1] == "rep" && args.size() >= 4) {
            // give rep <faction words...> <value>
            // Last arg is the value, everything between is the faction name
            int rep_val = 0;
            try { rep_val = std::stoi(args.back()); } catch (...) {
                log("Invalid number: " + args.back());
                return;
            }
            std::string faction;
            for (size_t i = 2; i < args.size() - 1; ++i) {
                if (!faction.empty()) faction += " ";
                faction += args[i];
            }
            bool found = false;
            for (auto& fs : player.reputation) {
                if (fs.faction_name == faction) {
                    fs.reputation = rep_val;
                    found = true;
                    break;
                }
            }
            if (found)
                log("Reputation with " + faction + " set to " + std::to_string(rep_val));
            else
                log("Unknown faction: " + faction);
        } else {
            log("Unknown: give " + args[1]);
        }
    }
    else if (verb == "set" && args.size() >= 2) {
        if (args[1] == "invuln") {
            if (has_effect(player.effects, EffectId::Invulnerable)) {
                remove_effect(player.effects, EffectId::Invulnerable);
                log("Invulnerability OFF");
            } else {
                add_effect(player.effects, make_invulnerable());
                log("Invulnerability ON");
            }
        } else if (args[1] == "level" && args.size() >= 3) {
            int lvl = 0;
            try { lvl = std::stoi(args[2]); } catch (...) {
                log("Invalid number: " + args[2]);
                return;
            }
            while (player.level < lvl) {
                game.dev_command_level_up();
            }
            log("Level set to " + std::to_string(player.level));
        } else {
            log("Unknown: set " + args[1]);
        }
    }
    else if (verb == "effect" && args.size() >= 2) {
        if (args[1] == "clear") {
            player.effects.clear();
            log("All effects cleared.");
        } else if (args[1] == "burn") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_burn(dur, 1));
            log("Burn applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "regen") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_regen(dur, 1));
            log("Regen applied for " + std::to_string(dur) + " ticks.");
        } else if (args[1] == "poison") {
            int dur = (args.size() >= 3) ? std::stoi(args[2]) : 10;
            add_effect(player.effects, make_poison(dur, 1));
            log("Poison applied for " + std::to_string(dur) + " ticks.");
        } else {
            log("Unknown effect: " + args[1]);
        }
    }
    else if (verb == "kill" && args.size() >= 2 && args[1] == "all") {
        int count = 0;
        for (auto& npc : game.npcs()) {
            if (npc.alive() && npc.disposition == Disposition::Hostile) {
                npc.hp = 0;
                ++count;
            }
        }
        game.dev_command_kill_hostiles();
        log("Killed " + std::to_string(count) + " hostile NPCs.");
    }
    else if (verb == "quest") {
        if (args.size() >= 2 && args[1] == "kill") {
            auto q = game.quests().generate_kill_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "fetch") {
            auto q = game.quests().generate_fetch_quest(game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "deliver") {
            auto q = game.quests().generate_deliver_quest("Merchant", game.world().rng());
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "scout") {
            // Pick a random landable body from the current system
            auto& nav = game.world().navigation();
            std::string body;
            int body_idx = -1;
            uint32_t sys_id = 0;
            for (auto& sys : nav.systems) {
                if (sys.id == nav.current_system_id) {
                    generate_system_bodies(sys);
                    sys_id = sys.id;
                    std::vector<int> landable;
                    for (int i = 0; i < static_cast<int>(sys.bodies.size()); ++i) {
                        if (sys.bodies[i].landable) landable.push_back(i);
                    }
                    if (!landable.empty()) {
                        int idx = std::uniform_int_distribution<int>(
                            0, static_cast<int>(landable.size()) - 1)(game.world().rng());
                        body_idx = landable[idx];
                        body = sys.bodies[body_idx].name;
                    }
                    break;
                }
            }
            if (body.empty()) body = "Unknown Body";
            auto q = game.quests().generate_scout_quest(body, game.world().rng());
            // Register map marker
            if (sys_id != 0 && body_idx >= 0) {
                q.target_system_id = sys_id;
                q.target_body_index = body_idx;
                LocationKey mk = {sys_id, body_idx, -1, false, -1, -1, 0, -1, -1};
                QuestLocationMeta meta;
                meta.quest_id = q.id;
                meta.quest_title = q.title;
                meta.target_system_id = sys_id;
                meta.target_body_index = body_idx;
                meta.remove_on_completion = true;
                game.world().quest_locations()[mk] = std::move(meta);
            }
            log("Quest: " + q.title);
            log("  " + q.description);
            game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
        } else if (args.size() >= 2 && args[1] == "story") {
            auto* sq = find_story_quest("story_missing_hauler");
            if (sq && !game.quests().has_active_quest("story_missing_hauler")) {
                auto q = sq->create_quest();
                log("Quest: " + q.title);
                log("  " + q.description);
                game.quests().accept_quest(std::move(q), game.world().world_tick(), game.player());
                sq->on_accepted(game);
                log("Quest markers placed on star chart.");
            } else if (game.quests().has_active_quest("story_missing_hauler")) {
                log("Quest already active.");
            } else {
                log("Story quest not found.");
            }
        } else {
            log("Usage: quest kill|fetch|deliver|scout|story");
        }
    }
    else if (verb == "history") {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        // Close console and open the lore viewer
        open_ = false;
        game.open_lore_viewer();
    }
    else if (verb == "lore" && args.size() >= 2) {
        if (!game.world().lore().generated) {
            log("No world lore generated yet.");
            return;
        }
        auto& nav = game.world().navigation();

        // Match a lore feature name to a system predicate
        auto match_system = [](const std::string& feature, const LoreAnnotation& la) -> bool {
            if (feature == "beacon")         return la.beacon;
            if (feature == "megastructure")  return la.has_megastructure;
            if (feature == "terraformed")    return la.terraformed;
            if (feature == "scarred" || feature == "scar")
                return la.battle_site || la.weapon_test_site;
            if (feature == "battle")         return la.battle_site;
            if (feature == "weapon")         return la.weapon_test_site;
            if (feature == "plague")         return la.plague_origin;
            if (feature == "tier3")          return la.lore_tier >= 3;
            if (feature == "tier2")          return la.lore_tier >= 2;
            if (feature == "tier1")          return la.lore_tier >= 1;
            return false;
        };

        if (args[1] == "list") {
            // List all systems with lore features
            int count = 0;
            for (const auto& sys : nav.systems) {
                const auto& la = sys.lore;
                if (la.lore_tier == 0) continue;
                std::string flags;
                if (la.beacon)          flags += " [beacon]";
                if (la.has_megastructure) flags += " [mega]";
                if (la.terraformed)     flags += " [terraform]";
                if (la.battle_site)     flags += " [battle]";
                if (la.weapon_test_site) flags += " [weapon]";
                if (la.plague_origin)   flags += " [plague]";
                if (la.scar_count > 0)  flags += " [scars:" + std::to_string(la.scar_count) + "]";
                log("  #" + std::to_string(sys.id) + " " + sys.name +
                    " (tier " + std::to_string(la.lore_tier) + ")" + flags);
                if (++count >= 30) { log("  ... (truncated)"); break; }
            }
            if (count == 0) log("No lore-annotated systems found.");
            else log(std::to_string(count) + " systems shown.");
        }
        else if (args[1] == "warp" && args.size() >= 3) {
            const std::string& feature = args[2];
            // Find first matching system
            for (const auto& sys : nav.systems) {
                if (match_system(feature, sys.lore)) {
                    game.dev_command_warp_to_system(sys.id);

                    std::string flags;
                    if (sys.lore.beacon) flags += " beacon";
                    if (sys.lore.has_megastructure) flags += " mega";
                    if (sys.lore.terraformed) flags += " terraform";
                    if (sys.lore.battle_site) flags += " battle";
                    if (sys.lore.weapon_test_site) flags += " weapon";
                    if (sys.lore.scar_count > 0) flags += " scars:" + std::to_string(sys.lore.scar_count);
                    log("Warped to " + sys.name + " (#" + std::to_string(sys.id) +
                        ", tier " + std::to_string(sys.lore.lore_tier) + ")" + flags);
                    return;
                }
            }
            log("No system found with feature: " + feature);
        }
        else {
            log("Usage: lore list | lore warp <feature>");
            log("Features: beacon, megastructure, terraformed, scarred, battle, weapon, plague, tier1, tier2, tier3");
        }
    }
    else {
        log("Unknown command: " + verb + ". Type 'help' for commands.");
    }
}

void DevConsole::draw(Renderer* renderer, int screen_w, int screen_h) {
    if (!open_) return;

    int con_h = std::min(20, screen_h / 2);
    if (con_h < 10) con_h = 10;
    Rect bounds{0, screen_h - con_h, screen_w, con_h};
    UIContext outer(renderer, bounds);
    auto ctx = outer.panel({
        .title = "Console",
        .footer = "[Esc] Close  [Enter] Execute  [Up/Down] History  [PgUp/PgDn] Scroll",
    });

    int content_h = ctx.height();
    int input_row = content_h - 1;

    // Input prompt
    ctx.styled_text({.x = 0, .y = input_row, .segments = {
        {"> ", UITag::TextAccent},
        {input_ + "_", UITag::TextBright},
    }});

    // Scrollable output
    int out_rows = input_row;
    int total = static_cast<int>(output_.size());

    int max_scroll = total - out_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (scroll_ > max_scroll) scroll_ = max_scroll;

    int end = total - scroll_;
    int start = end - out_rows;
    if (start < 0) start = 0;

    int row = 0;
    for (int i = start; i < end && row < out_rows; ++i, ++row) {
        const auto& line = output_[i];
        // Echo lines (starting with >) show bright, output shows dim
        UITag tag = (line.size() >= 2 && line[0] == '>') ? UITag::TextBright : UITag::TextDim;
        ctx.text({.x = 0, .y = row, .content = line, .tag = tag});
    }
}

} // namespace astra

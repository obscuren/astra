#include "astra/dev_console.h"
#include "astra/animation.h"
#include "astra/biome_profile.h"
#include "astra/body_presets.h"
#include "astra/effect.h"
#include "astra/faction.h"
#include "astra/game.h"
#include "astra/item_defs.h"
#include "astra/lore_generator.h"
#include "astra/npc.h"
#include "astra/quest_fixture.h"
#include "astra/star_chart.h"
#include "astra/station_type.h"
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
        log("  budget             - dump current planet's PoiBudget");
        log("  discoveries        - list Discovery-category journal entries");
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
        log("  quest begin <id>   - force-start a story quest by id (bypass prereqs)");
        log("  quest finish <id>  - force-complete active quest by id (fires cascade)");
        log("  heal               - full heal");
        log("  bearings           - regain bearings if lost");
        log("  lore list           - list lore-annotated systems");
        log("  lore warp <feature> - warp to system (beacon/megastructure/terraformed/scarred/battle/weapon/plague/tier1-3)");
        log("  chart create [kind] [name] - create custom system (kind: asteroid|scar|rock|neutron|derelict)");
        log("  chart reveal <name> - reveal system by name substring");
        log("  chart hide <name>   - hide system by name substring");
        log("  spawn <role> - spawn an enemy NPC adjacent to player");
        log("  fixtures     - list quest fixtures (id, location key, tile)");
        log("  tp <x> <y>   - teleport to tile (x, y) on current map");
        log("  tp <fixture_id> - teleport to that quest fixture if it's on the current map");
        log("  history             - show world lore history");
        log("  biome_test <biome> [settlement [frontier|advanced|ruined]]");
        log("                     [ruins [monolithic|baroque|crystal|industrial] [connected]]");
        log("                     [outpost]");
        log("                     [ship [pod|freighter|corvette]]");
        log("                     [cave [natural|mine|excavation]]");
        log("    biomes: grassland forest jungle sandy rocky volcanic marsh ice");
        log("    fungal crystal corroded aquatic alien_crystalline alien_organic");
        log("    alien_geometric alien_void alien_light scarred_scorched scarred_glassed");
        log("    settlement styles: frontier, advanced, ruined (default: frontier)");
        log("    ruins: generates ruin POI; civ style optional; 'connected' sets all 4 edges");
        log("    outpost: fenced fort with main building, tents, campfires");
        log("    ship: crashed wreck; class optional (auto = lore-weighted)");
        log("    cave: dungeon entrance; variant optional (natural/mine/excavation)");
        log("  editor             - open map editor");
        log("  clear              - clear console");
    }
    else if (verb == "clear") {
        clear();
    }
    else if (verb == "budget") {
        const TileMap& owm = game.world().map();
        if (owm.map_type() != MapType::Overworld) {
            output_.push_back("(not on an overworld — fly to a planet)");
        } else {
            const PoiBudget& b = owm.poi_budget();
            std::string report = format_poi_budget(b);
            size_t start = 0;
            while (start < report.size()) {
                size_t nl = report.find('\n', start);
                std::string line = report.substr(start, nl - start);
                if (!line.empty()) output_.push_back(line);
                if (nl == std::string::npos) break;
                start = nl + 1;
            }
            output_.push_back("Hidden POIs: " +
                std::to_string(owm.hidden_pois().size()));
            output_.push_back("Anchor hints: " +
                std::to_string(owm.anchor_hints().size()));
        }
    }
    else if (verb == "discoveries") {
        const auto& journal = game.player().journal;
        int count = 0;
        for (const auto& e : journal) {
            if (e.category == JournalCategory::Discovery) {
                output_.push_back(e.title);
                ++count;
            }
        }
        if (count == 0) output_.push_back("(no discoveries)");
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
                {"settlement", Tile::OW_Settlement},
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
    else if (verb == "biome_test" && args.size() < 2) {
        log("Usage: biome_test <biome>");
        log("Type 'help' for biome list.");
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
        std::string poi_type;
        std::string poi_style;
        bool connected = false;
        float ruin_decay_override = -1.0f;
        for (size_t i = 2; i < args.size(); ++i) {
            if (args[i] == "settlement") {
                poi_type = "settlement";
            } else if (args[i] == "ruins") {
                poi_type = "ruins";
            } else if (args[i] == "outpost") {
                poi_type = "outpost";
            } else if (args[i] == "ship" || args[i] == "crashed_ship") {
                poi_type = "ship";
            } else if (args[i] == "pod" || args[i] == "freighter" || args[i] == "corvette") {
                if (poi_type.empty()) poi_type = "ship";
                poi_style = args[i];
            } else if (args[i] == "cave" || args[i] == "cave_entrance") {
                poi_type = "cave";
            } else if (args[i] == "natural" || args[i] == "mine" || args[i] == "excavation") {
                if (poi_type.empty()) poi_type = "cave";
                poi_style = args[i];
            } else if (args[i] == "connected") {
                connected = true;
            } else if (args[i] == "frontier") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "frontier";
            } else if (args[i] == "advanced") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "advanced";
            } else if (args[i] == "ruined") {
                if (poi_type.empty()) poi_type = "settlement";
                poi_style = "ruined";
            } else if (args[i] == "monolithic" || args[i] == "baroque" ||
                       args[i] == "crystal" || args[i] == "industrial") {
                if (poi_type.empty()) poi_type = "ruins";
                poi_style = args[i];
            } else {
                // Try as float first (for ruin decay), then int (for layer)
                try {
                    float f = std::stof(args[i]);
                    if (args[i].find('.') != std::string::npos) {
                        ruin_decay_override = f;
                    } else {
                        layer = static_cast<int>(f);
                    }
                } catch (...) {
                    log("Invalid arg: " + args[i]);
                    return;
                }
            }
        }
        std::string civ_name;
        if (poi_type == "ruins") civ_name = poi_style;
        game.dev_command_biome_test(biome, layer, poi_type,
                                    poi_type == "ruins" ? "" : poi_style,
                                    connected, civ_name, ruin_decay_override);
        std::string msg = "Biome test: " + args[1] + " (360x150)";
        if (poi_type == "settlement") {
            std::string style_display = poi_style.empty() ? "frontier" : poi_style;
            msg += " + settlement (" + style_display + ")";
        } else if (poi_type == "ruins") {
            msg += " + ruins";
            if (!civ_name.empty()) msg += " (" + civ_name + ")";
            if (ruin_decay_override >= 0.0f)
                msg += " decay=" + std::to_string(ruin_decay_override).substr(0, 4);
            if (connected) msg += " (connected)";
        } else if (poi_type == "outpost") {
            msg += " + outpost";
        } else if (poi_type == "ship") {
            msg += " + crashed ship";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        } else if (poi_type == "cave") {
            msg += " + cave entrance";
            if (!poi_style.empty()) msg += " (" + poi_style + ")";
        }
        log(msg);
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
            if (npc.alive() && is_hostile_to_player(npc.faction, game.player())) {
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
                LocationKey mk = {sys_id, body_idx, -1, false, -1, -1, 0};
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
        } else if (args.size() >= 2 && args[1] == "fixture") {
            // Register a debug def and plant it adjacent to the player.
            QuestFixtureDef def;
            def.id = "dev_smoke_fixture";
            def.glyph = '*';
            def.color = 135;
            def.prompt = "Play debug transmission";
            def.log_message = "You nudge the debug fixture. It beeps.";
            def.log_title = "DEV SMOKE TRANSMISSION";
            def.log_lines = {
                "This is line one of the debug transmission.",
                "",
                "Line two. The reveal should advance at thirty chars per second.",
                "Press Space to skip; press Esc to close; re-interact to replay.",
            };
            register_quest_fixture(def);

            FixtureData fd;
            fd.type = FixtureType::QuestFixture;
            fd.interactable = true;
            fd.passable = true;
            fd.quest_fixture_id = def.id;

            int fx = game.player().x + 1;
            int fy = game.player().y;
            if (fx >= 0 && fx < game.world().map().width() &&
                fy >= 0 && fy < game.world().map().height() &&
                game.world().map().passable(fx, fy) &&
                game.world().map().fixture_id(fx, fy) < 0) {
                game.world().map().add_fixture(fx, fy, fd);
                log("Planted dev_smoke_fixture at (" +
                    std::to_string(fx) + "," + std::to_string(fy) + ")");
            } else {
                log("No open tile adjacent to player for fixture.");
            }
        } else if (args.size() >= 3 && args[1] == "begin") {
            // Force-start a story quest by id, bypassing prereqs and dialog.
            const std::string& qid = args[2];
            if (game.quests().has_active_quest(qid)) {
                log("quest begin: already active");
                return;
            }
            auto* sq = find_story_quest(qid);
            if (!sq) {
                log("quest begin: no story quest with id '" + qid + "'");
                return;
            }
            auto q = sq->create_quest();
            game.quests().accept_quest(std::move(q), game.world().world_tick(),
                                       game.player());
            sq->on_accepted(game);
            log("Force-started quest: " + qid);
        } else if (args.size() >= 3 && args[1] == "finish") {
            // Force-complete an active quest by id (fires on_completed + DAG).
            const std::string& qid = args[2];
            if (!game.quests().has_active_quest(qid)) {
                log("quest finish: '" + qid + "' is not active");
                return;
            }
            // Tick every objective to its target so complete_quest sees it done
            // and its reward / journal paths run normally.
            if (Quest* q = game.quests().find_active(qid)) {
                for (auto& obj : q->objectives) obj.current_count = obj.target_count;
            }
            game.quests().complete_quest(qid, game, game.world().world_tick());
            log("Force-finished quest: " + qid);
        } else {
            log("Usage: quest kill|fetch|deliver|scout|story|fixture");
            log("       quest begin <id> | quest finish <id>");
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
    else if (verb == "chart") {
        auto& nav = game.world().navigation();
        if (args.size() >= 2 && args[1] == "create") {
            std::string kind = "asteroid";
            std::string name = "Custom";
            if (args.size() == 3) {
                // Single extra arg: name only (back-compat, `chart create Foo`).
                name = args[2];
            } else if (args.size() >= 4) {
                // Two extra args: <kind> <name>. Kind must be known.
                std::string a2 = args[2];
                if (a2 != "asteroid" && a2 != "scar" &&
                    a2 != "rock" && a2 != "neutron" && a2 != "derelict") {
                    log("chart create: unknown kind '" + a2 +
                        "' (expected asteroid|scar|rock|neutron|derelict)");
                    return;
                }
                kind = a2;
                name = args[3];
            }

            auto coords = pick_coords_near(nav, nav.current_system_id,
                                           2.0f, 5.0f, game.world().rng());
            if (!coords) {
                log("chart create: couldn't find a spot near current system");
                return;
            }

            CustomSystemSpec spec;
            spec.name = name;
            spec.gx = coords->first;
            spec.gy = coords->second;
            spec.star_class = StarClass::ClassM;
            spec.discovered = true;

            if (kind == "asteroid") {
                spec.bodies = { make_landable_asteroid(name + " Rock") };
            } else if (kind == "scar") {
                spec.bodies = { make_scar_planet(name + " Prime") };
            } else if (kind == "neutron") {
                spec.star_class = StarClass::Neutron;
                spec.bodies = { make_landable_asteroid(name + " Fragment") };
            } else if (kind == "derelict") {
                spec.has_station = true;
                spec.station.type = StationType::Abandoned;
                spec.station.specialty = StationSpecialty::Generic;
                spec.station.name = name + " Outpost";
                spec.star_class = StarClass::ClassG;
                spec.bodies = {};
            } else { // "rock"
                CelestialBody b;
                b.name = name + " Rock";
                b.type = BodyType::Rocky;
                b.atmosphere = Atmosphere::None;
                b.temperature = Temperature::Cold;
                b.size = 2;
                b.landable = true;
                b.danger_level = 1;
                b.day_length = 200;
                spec.bodies = { std::move(b) };
            }

            uint32_t id = add_custom_system(nav, std::move(spec));
            log("Created custom " + kind + " system '" + name + "' id=" +
                std::to_string(id) + " at (" + std::to_string(coords->first) +
                ", " + std::to_string(coords->second) + ")");
        } else if (args.size() >= 2 && args[1] == "reveal") {
            if (args.size() < 3) { log("Usage: chart reveal <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (reveal_system(nav, s.id)) {
                        log("Revealed '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else if (args.size() >= 2 && args[1] == "hide") {
            if (args.size() < 3) { log("Usage: chart hide <name-substring>"); return; }
            const std::string& needle = args[2];
            for (auto& s : nav.systems) {
                if (s.name.find(needle) != std::string::npos) {
                    if (hide_system(nav, s.id)) {
                        log("Hid '" + s.name + "' (id=" + std::to_string(s.id) + ")");
                    }
                    return;
                }
            }
            log("No system matches '" + needle + "'");
        } else {
            log("Usage: chart create [kind] [name]|reveal <name>|hide <name>");
        }
    }
    else if (verb == "spawn") {
        if (args.size() < 2) {
            log("Usage: spawn <role>  (archon_remnant|void_reaver|archon_sentinel)");
            return;
        }
        std::string role_arg = args[1];
        std::string role_name;
        if      (role_arg == "archon_remnant")  role_name = "Archon Remnant";
        else if (role_arg == "void_reaver")     role_name = "Void Reaver";
        else if (role_arg == "archon_sentinel") role_name = "Archon Sentinel";
        else {
            log("spawn: unknown role '" + role_arg +
                "' (archon_remnant|void_reaver|archon_sentinel)");
            return;
        }

        Npc npc = create_npc_by_role(role_name, game.world().rng());
        // Walk the 8 neighbours until a passable empty tile is found.
        const int dx[] = {1, -1, 0, 0, 1, 1, -1, -1};
        const int dy[] = {0, 0, 1, -1, 1, -1, 1, -1};
        bool placed = false;
        for (int i = 0; i < 8 && !placed; ++i) {
            int nx = game.player().x + dx[i];
            int ny = game.player().y + dy[i];
            if (nx < 0 || nx >= game.world().map().width()) continue;
            if (ny < 0 || ny >= game.world().map().height()) continue;
            if (!game.world().map().passable(nx, ny)) continue;
            bool occupied = false;
            for (const auto& other : game.world().npcs()) {
                if (other.alive() && other.x == nx && other.y == ny) {
                    occupied = true;
                    break;
                }
            }
            if (occupied) continue;
            npc.x = nx;
            npc.y = ny;
            game.world().npcs().push_back(std::move(npc));
            log("Spawned " + role_name + " at (" + std::to_string(nx) +
                "," + std::to_string(ny) + ")");
            placed = true;
        }
        if (!placed) log("spawn: no adjacent passable tile");
    }
    else if (verb == "fixtures") {
        // Dump quest-fixture placements across all quest_locations so the
        // dev can see what's been stamped where (and on which map key).
        const auto& qlocs = game.world().quest_locations();
        if (qlocs.empty()) {
            log("No quest_locations registered.");
            return;
        }
        int total = 0;
        for (const auto& [key, meta] : qlocs) {
            auto [sys, b, m, stn, ow_x, ow_y, d] = key;
            for (const auto& p : meta.fixtures) {
                ++total;
                std::string loc = "sys=" + std::to_string(sys) +
                                  " body=" + std::to_string(b) +
                                  (m >= 0 ? " moon=" + std::to_string(m) : "") +
                                  (stn ? " [station]" : "") +
                                  (d > 0 ? " depth=" + std::to_string(d) : "");
                if (p.x < 0 || p.y < 0) {
                    log(p.fixture_id + " — " + loc + " — (unplaced)");
                } else {
                    log(p.fixture_id + " — " + loc + " — tile (" +
                        std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                }
            }
        }
        if (total == 0) log("No quest fixtures declared.");
    }
    else if (verb == "tp" && args.size() >= 3) {
        // Teleport player to (x, y) on the current map.
        int tx = std::atoi(args[1].c_str());
        int ty = std::atoi(args[2].c_str());
        if (tx < 0 || tx >= game.world().map().width() ||
            ty < 0 || ty >= game.world().map().height()) {
            log("tp: out of bounds");
            return;
        }
        game.player().x = tx;
        game.player().y = ty;
        log("Teleported to (" + std::to_string(tx) + "," + std::to_string(ty) + ")");
    }
    else if (verb == "tp" && args.size() == 2) {
        // Teleport to a quest fixture by id, if it's on the current map.
        const std::string& fid = args[1];
        for (const auto& [key, meta] : game.world().quest_locations()) {
            for (const auto& p : meta.fixtures) {
                if (p.fixture_id != fid) continue;
                if (p.x < 0 || p.y < 0) {
                    log("tp: fixture '" + fid + "' hasn't been placed yet (enter its map first)");
                    return;
                }
                if (p.x >= game.world().map().width() ||
                    p.y >= game.world().map().height()) {
                    log("tp: fixture '" + fid + "' is on a different map");
                    return;
                }
                game.player().x = p.x;
                game.player().y = p.y;
                log("Teleported to '" + fid + "' at (" +
                    std::to_string(p.x) + "," + std::to_string(p.y) + ")");
                return;
            }
        }
        log("tp: fixture '" + fid + "' not found");
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

#include "astra/scenario_effects.h"

#include "astra/faction.h"
#include "astra/game.h"
#include "astra/playback_viewer.h"
#include "astra/world_manager.h"

namespace astra {

void shift_faction_standing(Game& game, const std::string& faction, int delta) {
    modify_faction_standing(game.player(), faction, delta);
}

void set_world_flag(Game& game, const std::string& flag, bool value) {
    game.world().set_world_flag(flag, value);
}

void open_transmission(Game& game,
                       const std::string& header,
                       const std::vector<std::string>& lines) {
    game.playback_viewer().open(PlaybackStyle::AudioLog, header, lines);
}

void inject_location_encounter(Game& game,
                               uint32_t system_id,
                               int body_index,
                               bool is_station,
                               const std::vector<std::string>& npc_roles,
                               const std::string& source_tag) {
    LocationKey key = {system_id, body_index, -1, is_station, -1, -1, 0};
    QuestLocationMeta meta;
    meta.quest_id = source_tag;
    meta.quest_title = "";
    meta.target_system_id = system_id;
    meta.target_body_index = body_index;
    meta.npc_roles = npc_roles;
    meta.remove_on_completion = false;
    game.world().quest_locations()[key] = std::move(meta);
}

} // namespace astra

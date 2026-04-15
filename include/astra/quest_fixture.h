#pragma once

#include <string>
#include <vector>

namespace astra {

// Authored definition for a single quest fixture instance.
// Registered at boot (via StoryQuest::register_fixtures) and looked up
// by FixtureData::quest_fixture_id during rendering and interaction.
struct QuestFixtureDef {
    std::string id;           // unique registry key, e.g. "nova_signal_node_echo1"
    char glyph = '?';
    int color = 7;            // terminal color index
    std::string prompt;       // UI hint, e.g. "Plant receiver drone"
    std::string log_message;  // optional; written to game log on interact ("" = silent)
    std::string log_title;    // viewer header title (empty = none)
    std::vector<std::string> log_lines;  // viewer body; empty = no playback
};

// Idempotent: re-registering the same id overwrites the existing def.
void register_quest_fixture(QuestFixtureDef def);

// Returns nullptr if no def is registered for `id`.
const QuestFixtureDef* find_quest_fixture(const std::string& id);

// Drops all registered defs. Used by tests and by game-restart code paths.
void clear_quest_fixtures();

} // namespace astra

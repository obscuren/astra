#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace astra {

enum class JournalCategory : uint8_t {
    Blueprint,
    Discovery,
    Encounter,
    Event,
    Quest,
};

struct JournalEntry {
    JournalCategory category = JournalCategory::Event;
    std::string title;           // e.g. "Blueprint: Plasma Emitter"
    std::string technical;       // technical/analysis section
    std::string personal;        // commander's notes
    std::string timestamp;       // e.g. "Cycle 1, Day 3 — Dawn"
    int world_tick = 0;
    std::string quest_id;        // links entry to a quest for updates (empty = none)

    // Populated for Discovery entries. Used to render a live map preview.
    bool has_discovery_location = false;
    int discovery_system_id = 0;
    int discovery_body_index = 0;
    int discovery_moon_index = -1;
    int discovery_overworld_x = 0;
    int discovery_overworld_y = 0;
    std::string discovery_location_name;
};

const char* journal_category_name(JournalCategory c);

// Generate a journal entry for a newly learned blueprint
JournalEntry make_blueprint_journal_entry(
    const std::string& blueprint_name,
    const std::string& blueprint_desc,
    const std::string& source_item_name,
    int world_tick,
    const std::string& phase_name);

// Generate a journal entry for a key event
JournalEntry make_event_journal_entry(
    JournalCategory category,
    const std::string& title,
    const std::string& description,
    int world_tick,
    const std::string& phase_name);


// Generate a journal entry for a discovered ruin/POI with live map preview coords
JournalEntry make_discovery_journal_entry(
    const std::string& title,
    const std::string& technical,
    const std::string& personal,
    int world_tick,
    const std::string& phase_name,
    int system_id,
    int body_index,
    int moon_index,
    int overworld_x,
    int overworld_y,
    const std::string& location_name);

// Find a journal entry by quest_id (returns nullptr if not found)
JournalEntry* find_journal_entry(std::vector<JournalEntry>& journal, const std::string& quest_id);

} // namespace astra

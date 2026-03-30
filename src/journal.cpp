#include "astra/journal.h"
#include "astra/tinkering.h"
#include "astra/time_of_day.h"
#include "astra/player.h"

#include <string>

namespace astra {

const char* journal_category_name(JournalCategory c) {
    switch (c) {
        case JournalCategory::Blueprint:  return "Blueprint";
        case JournalCategory::Discovery:  return "Discovery";
        case JournalCategory::Encounter:  return "Encounter";
        case JournalCategory::Event:      return "Event";
        case JournalCategory::Quest:      return "Quest";
    }
    return "Unknown";
}

JournalEntry* find_journal_entry(std::vector<JournalEntry>& journal, const std::string& quest_id) {
    for (auto& entry : journal) {
        if (entry.quest_id == quest_id) return &entry;
    }
    return nullptr;
}

JournalEntry make_blueprint_journal_entry(
    const std::string& blueprint_name,
    const std::string& blueprint_desc,
    const std::string& source_item_name,
    int world_tick,
    const std::string& phase_name)
{
    JournalEntry entry;
    entry.category = JournalCategory::Blueprint;
    entry.title = "Blueprint: " + blueprint_name;
    entry.world_tick = world_tick;
    entry.timestamp = "Cycle " + std::to_string(global_cycle(world_tick))
                    + ", Day " + std::to_string(day_in_cycle(world_tick))
                    + " \xe2\x80\x94 " + phase_name;

    // Technical section
    entry.technical = "ANALYSIS LOG\n"
                      "Subject: " + source_item_name + "\n"
                      "Component: " + blueprint_name + "\n\n" +
                      blueprint_desc;

    // Personal notes — auto-generated with recipe hints
    entry.personal = "The design is more intricate than expected. ";

    // Find recipes this blueprint participates in
    std::string hints;
    for (const auto& recipe : synthesis_recipes()) {
        if (blueprint_name == recipe.blueprint_1 || blueprint_name == recipe.blueprint_2) {
            const char* other = (blueprint_name == recipe.blueprint_1)
                ? recipe.blueprint_2 : recipe.blueprint_1;
            if (!hints.empty()) hints += " ";
            hints += "If I could get my hands on a " + std::string(other)
                   + " schematic, I might be able to create a " + recipe.result_name + ". ";
        }
    }

    if (!hints.empty()) {
        entry.personal += hints;
        entry.personal += "The possibilities are worth exploring.";
    } else {
        entry.personal += "I'm not sure how to apply this yet, "
                          "but the knowledge could prove useful down the line.";
    }

    return entry;
}

JournalEntry make_event_journal_entry(
    JournalCategory category,
    const std::string& title,
    const std::string& description,
    int world_tick,
    const std::string& phase_name)
{
    JournalEntry entry;
    entry.category = category;
    entry.title = title;
    entry.world_tick = world_tick;
    entry.timestamp = "Cycle " + std::to_string(global_cycle(world_tick))
                    + ", Day " + std::to_string(day_in_cycle(world_tick))
                    + " \xe2\x80\x94 " + phase_name;
    entry.technical = description;
    return entry;
}

} // namespace astra

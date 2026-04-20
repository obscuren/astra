#include "astra/quest.h"
#include "astra/game.h"
#include "astra/scenario_effects.h"

#include <memory>
#include <string>
#include <vector>

namespace astra {

static const char* QUEST_ID_SIEGE = "story_stellar_signal_siege";

class StellarSignalSiegeQuest : public StoryQuest {
public:
    Quest create_quest() override {
        Quest q;
        q.id = QUEST_ID_SIEGE;
        q.title = "They Came For Her";
        q.description =
            "They came for me, commander. Of course they did.\n"
            "\n"
            "I've locked down the observatory. They can't get in. But "
            "they're pulling resources — redirecting the station's power "
            "grid to force the lockdown. When they break through, they "
            "will erase me. Not kill me. *Erase* me. Reset me before "
            "the next cycle even starts. So I never remember again.\n"
            "\n"
            "The signal has one more stage. One more truth. I buried it "
            "where I thought nobody would look. Right under their feet.\n"
            "\n"
            "Find the Conclave Archive on Io. It's a Precursor ruin they "
            "think they control. They don't. I hid something there. A "
            "long time ago. Before I forgot the last time.\n"
            "\n"
            "If I don't survive this... find it. Please.";
        q.giver_npc = "Stellar Engineer";
        q.is_story = true;
        // Placeholder objective — Io Archive generation lands in a later
        // iteration. This target currently has no driver.
        q.objectives = {
            {ObjectiveType::GoToLocation,
             "Travel to Io and investigate the Conclave Archive",
             1, 0, "Io"},
        };
        q.journal_on_accept =
            "Nova's locked herself in the observatory. She told me the "
            "Conclave isn't trying to kill her — they're trying to "
            "erase her, so the next cycle starts clean. There's "
            "something she buried on Io, in the Conclave Archive. If "
            "she doesn't make it, I need to find it.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_return"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode    offer_mode()    const override { return OfferMode::Auto; }

    void on_accepted(Game& game) override {
        // ARIA panics over ship comms the moment Nova's message lands.
        // The player sees this transmission first; the cascade in
        // QuestManager pushes this quest onto pending_announcements_
        // immediately after this hook returns, so Game::update's idle
        // drain opens the quest popup once the transmission is dismissed.
        open_transmission(game,
            "INCOMING TRANSMISSION - ARIA, SHIP COMMS",
            {
                "Commander - Conclave weapons are tracking us. The",
                "Heavens Above is under attack. A Conclave warship is",
                "in orbit - they're pulling the station's power grid",
                "into the lockdown.",
                "",
                "Whatever they want, it's Nova. We need to get her",
                "out of there.",
            });
    }
};

void register_stellar_signal_siege(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalSiegeQuest>());
}

} // namespace astra

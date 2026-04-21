#include "astra/quest.h"
#include "astra/quest_fixture.h"
#include "astra/game.h"
#include "astra/scenario_effects.h"
#include "astra/world_manager.h"
#include "astra/dungeon/conclave_archive.h"
#include "astra/dungeon_recipe.h"

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
        q.objectives = {
            {ObjectiveType::GoToLocation,
             "Land on Io", 1, 0, "Io"},
            {ObjectiveType::InteractFixture,
             "Recover Nova's fragment from the Conclave Archive",
             1, 0, "nova_resonance_crystal"},
        };
        q.reward.xp      = 400;
        q.reward.credits = 500;
        q.journal_on_accept =
            "Nova's locked herself in the observatory. She told me the "
            "Conclave isn't trying to kill her — they're trying to "
            "erase her, so the next cycle starts clean. There's "
            "something she buried on Io, in the Conclave Archive. If "
            "she doesn't make it, I need to find it.";
        q.journal_on_complete =
            "Played Nova's final message. Heard her three choices. "
            "THA's comms are open again - the Conclave pulled back. "
            "I think they didn't expect anyone to reach the vault. "
            "Nova's voice is still in my head.";
        return q;
    }

    std::string arc_id() const override    { return "stellar_signal"; }
    std::string arc_title() const override { return "The Stellar Signal"; }
    std::vector<std::string> prerequisite_ids() const override {
        return {"story_stellar_signal_return"};
    }
    RevealPolicy reveal_policy() const override { return RevealPolicy::Full; }
    OfferMode    offer_mode()    const override { return OfferMode::Auto; }

    void register_fixtures() override {
        register_quest_fixture({
            "conclave_archive_entrance",
            'v', 135,
            "Descend into the Conclave Archive",
            "You drop through the hatch into the ruin below.",
            "", {}
        });
        register_quest_fixture({
            "nova_resonance_crystal",
            '*', 135,
            "A small Stellari-resonance crystal hums on a Precursor pedestal. Activate it?",
            "The crystal lights up. Nova's voice fills the chamber.",
            "STELLARI RESONANCE CRYSTAL - FINAL LOG",
            {
                // Placeholder — user will replace with verbatim lines from
                // /Users/jeffrey/dev/Unreal/lyra/nova-arc-the-stellar-signal.md
                // lines 292-343.
                "If you're hearing this, it means I'm probably gone.",
                "Erased. Wiped. Whatever they call it.",
                "",
                "But listen, commander - and I mean *really* listen.",
                "",
                "The cycle is a choice. Always was.",
                "(...crystal audio log to be completed...)",
            },
        });
    }

    void on_accepted(Game& game) override {
        // Star chart marker: Sol = 1, Jupiter body = 5, Io moon = 0.
        LocationKey k{1, 5, 0, false, -1, -1, 0};
        QuestLocationMeta meta;
        meta.quest_id = QUEST_ID_SIEGE;
        meta.quest_title = "They Came For Her";
        meta.target_system_id = 1;
        meta.target_body_index = 5;
        meta.target_moon_index = 0;
        meta.poi_type = Tile::OW_PrecursorArchive;
        meta.npc_roles = {"Conclave Sentry", "Conclave Sentry", "Conclave Sentry"};
        game.world().quest_locations()[k] = std::move(meta);

        // Register Conclave Archive dungeon recipe — 3 levels of Precursor
        // ruin with Archon Sentinel boss on the deepest level.
        DungeonRecipe recipe;
        recipe.root        = k;
        recipe.kind_tag    = "conclave_archive";
        recipe.level_count = 3;
        recipe.levels      = build_conclave_archive_levels();
        game.world().dungeon_recipes()[k] = std::move(recipe);

        // If the player has already visited Io, the overworld was
        // generated without the Archive POI. Retro-stamp it now —
        // Nova has told them where to look.
        auto it = game.world().location_cache().find(k);
        if (it != game.world().location_cache().end()) {
            auto& state = it->second;
            int cx = state.map.width() / 2;
            int cy = state.map.height() / 2;
            // Walk outward in rings for the first walkable, non-special tile.
            for (int r = 0; r < std::max(state.map.width(), state.map.height()); ++r) {
                bool placed = false;
                for (int dy = -r; dy <= r && !placed; ++dy) {
                    for (int dx = -r; dx <= r && !placed; ++dx) {
                        if (std::abs(dx) != r && std::abs(dy) != r) continue;
                        int px = cx + dx, py = cy + dy;
                        if (px < 0 || py < 0 || px >= state.map.width() || py >= state.map.height()) continue;
                        Tile t = state.map.get(px, py);
                        if (t == Tile::OW_Mountains || t == Tile::OW_Lake ||
                            t == Tile::OW_River || t == Tile::OW_Swamp) continue;
                        state.map.set(px, py, Tile::OW_PrecursorArchive);
                        placed = true;
                    }
                }
                if (placed) break;
            }
        }

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

    void on_completed(Game& game) override {
        // Siege resolves when the Conclave Archive arc wraps up; THA
        // becomes landable again. Full post-siege reshaping of the
        // station (NPC state, Observatory access) lives in that slice.
        set_world_flag(game, "tha_lockdown", false);

        // The Archive is no longer a live quest location. Unregister the
        // recipe so descending the hatch from here on is a no-op (the
        // DungeonHatch handler bails when no recipe is present), and
        // replace the bespoke POI tile on Io's overworld with a plain
        // ruin so the map doesn't keep advertising a special landmark.
        LocationKey k{1, 5, 0, false, -1, -1, 0};
        game.world().dungeon_recipes().erase(k);
        game.world().quest_locations().erase(k);
        auto it = game.world().location_cache().find(k);
        if (it != game.world().location_cache().end()) {
            auto& state = it->second;
            for (int y = 0; y < state.map.height(); ++y) {
                for (int x = 0; x < state.map.width(); ++x) {
                    if (state.map.get(x, y) == Tile::OW_PrecursorArchive) {
                        state.map.set(x, y, Tile::OW_Ruins);
                    }
                }
            }
        }
    }
};

void register_stellar_signal_siege(std::vector<std::unique_ptr<StoryQuest>>& catalog) {
    catalog.push_back(std::make_unique<StellarSignalSiegeQuest>());
}

} // namespace astra

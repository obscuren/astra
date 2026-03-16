#pragma once

#include <optional>
#include <string>
#include <vector>

namespace astra {

// A single node in a conversation tree.
struct DialogNode {
    std::string text; // NPC says this

    struct Choice {
        std::string label;     // player option text
        int next_node = -1;    // index into dialog tree, -1 = end conversation
    };

    std::vector<Choice> choices;
};

// Trait: NPC can talk (simple greeting or branching dialog)
struct TalkTrait {
    std::string greeting;              // opening line shown in dialog body
    std::vector<DialogNode> nodes;     // dialog tree; choices reference indices here
};

// Trait: NPC has a shop
struct ShopTrait {
    std::string shop_name;
    // std::vector<ShopItem> inventory; // future
};

// Trait: NPC gives quests
struct QuestTrait {
    std::string quest_intro;           // top-level option label, e.g. "Any work available?"
    std::vector<DialogNode> nodes;     // quest dialog tree
};

// Everything an NPC can do when interacted with.
// Each optional trait is independent — compose freely.
struct InteractionData {
    std::optional<TalkTrait> talk;
    std::optional<ShopTrait> shop;
    std::optional<QuestTrait> quest;

    bool empty() const { return !talk && !shop && !quest; }
};

} // namespace astra

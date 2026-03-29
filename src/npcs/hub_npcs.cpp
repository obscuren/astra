#include "astra/npc_defs.h"
#include "astra/item_defs.h"

namespace astra {

Npc build_food_merchant(Race race, std::mt19937& rng, int faction_rep) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'F';
    npc.color = Color::Yellow;
    npc.role = "Food Merchant";
    npc.hp = 12;
    npc.max_hp = 12;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Kreth Mining Guild";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Welcome! Best synth-grub on the station. What'll it be?",
        {
            {
                "Everything's fresh — well, fresh as anything gets on a "
                "station orbiting a gas giant. I've got protein bars, "
                "nutrient paste, and the house special: spiced void-eel.",
                {
                    {"What's void-eel?", 1},
                    {"Sounds good.", -1},
                },
            },
            {
                "Deep-space catch. The haulers bring 'em in frozen from "
                "the outer belt. Tastes better than it sounds, trust me.",
                {
                    {"I'll take your word for it.", -1},
                },
            },
        },
    };

    npc.interactions.shop = ShopTrait{
        npc.name + "'s Kitchen",
        generate_food_merchant_stock(rng, faction_rep),
    };

    return npc;
}

Npc build_medic(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'D';
    npc.color = Color::Green;
    npc.role = "Medic";
    npc.hp = 10;
    npc.max_hp = 10;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Stellari Conclave";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Come in. The healing pods are available if you need them.",
        {
            {
                "I've patched up more spacers than I can count. Plasma "
                "burns, decompression injuries, alien parasites — you name it. "
                "The pods handle most of the work now, but I keep an eye "
                "on the diagnostics.",
                {
                    {"What are healing pods exactly?", 1},
                    {"Thanks, doc.", -1},
                },
            },
            {
                "Nanite-infused gel capsules. You climb in, the nanites "
                "assess your injuries and repair tissue at the cellular "
                "level. Takes about thirty seconds for a full restore. "
                "Just don't use them too frequently — they need time to "
                "recharge between sessions.",
                {
                    {"Good to know.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_commander(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'C';
    npc.color = Color::White;
    npc.role = "Station Commander";
    npc.hp = 25;
    npc.max_hp = 25;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Stellari Conclave";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Commander. I run this station. What do you need?",
        {
            // Node 0: Main
            {
                "The Heavens Above is the last major outpost before the "
                "inner systems. We keep the peace, maintain the docks, "
                "and make sure the supply lines stay open. Beyond here, "
                "you're on your own.",
                {
                    {"What's the situation out there?", 1},
                    {"I heard you might have a spare nav computer.", 2},
                    {"Understood.", -1},
                },
            },
            // Node 1: Situation report
            {
                "Xytomorph activity has been increasing in the asteroid "
                "belts. We've lost contact with two survey teams this "
                "cycle. If you're heading out, watch yourself. And if "
                "you find anything useful, report back.",
                {
                    {"I'll keep my eyes open.", -1},
                },
            },
            // Node 2: Nav computer favor
            {
                "A nav computer? Maybe. We salvaged one from a wreck "
                "last cycle. I could part with it — but I need a favor "
                "first. We intercepted a distress signal from the lower "
                "decks. Something's down there that shouldn't be. Clear "
                "it out and the nav computer is yours.",
                {
                    {"I'll take a look.", -1},
                },
            },
        },
    };

    npc.interactions.quest = QuestTrait{
        "About that nav computer...",
        {
            // Node 0: offer
            {
                "Right. We salvaged a nav computer from a derelict last "
                "cycle. It's yours — consider it a welcome gift. Can't "
                "have ships stranded on my station.",
                {
                    {"Thank you, Commander.", 1},
                    {"Maybe later.", -1},
                },
            },
            // Node 1: accepted
            {
                "Don't mention it. Get your ship running and come see "
                "me when you're ready for real work.",
                {
                    {"Will do.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_arms_dealer(Race race, std::mt19937& rng, int faction_rep) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'A';
    npc.color = Color::Red;
    npc.role = "Arms Dealer";
    npc.hp = 20;
    npc.max_hp = 20;
    npc.disposition = Disposition::Neutral;
    npc.faction = "Kreth Mining Guild";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Looking for firepower? You've come to the right place.",
        {
            {
                "I deal in personal defense systems. Plasma sidearms, "
                "ion disruptors, kinetic accelerators — whatever you "
                "need to stay alive out there. Everything's tested and "
                "certified. Mostly.",
                {
                    {"What do you recommend?", 1},
                    {"Just browsing.", -1},
                },
            },
            {
                "Depends on what you're facing. Xytomorphs? You want "
                "something with stopping power — plasma or kinetic. For "
                "general exploration, an ion disruptor gives you range "
                "and reliability. Come back when you've got credits.",
                {
                    {"I'll think about it.", -1},
                },
            },
        },
    };

    npc.interactions.shop = ShopTrait{
        npc.name + "'s Arsenal",
        generate_arms_dealer_stock(rng, faction_rep),
    };

    return npc;
}

Npc build_astronomer(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'P';
    npc.color = Color::Cyan;
    npc.role = "Astronomer";
    npc.hp = 8;
    npc.max_hp = 8;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Stellari Conclave";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Ah, a visitor. Come, look at the stars with me.",
        {
            {
                "I've been mapping the gravitational anomalies near "
                "Sagittarius A* for decades. The patterns are... "
                "unsettling. Almost deliberate. As if something is "
                "arranging the stars.",
                {
                    {"What do you mean, deliberate?", 1},
                    {"Fascinating.", -1},
                },
            },
            {
                "The mass distributions around the galactic center don't "
                "match any natural model. There are structures there — "
                "ancient ones. Whatever civilization built them is long "
                "gone, but their work endures. If you're truly heading "
                "for Sgr A*, you'll see for yourself.",
                {
                    {"I will.", -1},
                },
            },
        },
    };

    return npc;
}

Npc build_engineer(Race race, std::mt19937& rng) {
    Npc npc;
    npc.race = race;
    npc.glyph = 'E';
    npc.color = Color::Yellow;
    npc.role = "Engineer";
    npc.hp = 15;
    npc.max_hp = 15;
    npc.disposition = Disposition::Friendly;
    npc.faction = "Stellari Conclave";
    add_effect(npc.effects, make_invulnerable());
    npc.quickness = 0;
    npc.name = generate_name(race, rng);

    npc.interactions.talk = TalkTrait{
        "Watch your step — live conduits everywhere.",
        {
            // Node 0: Main
            {
                "I keep this station running. Power distribution, life "
                "support, hull integrity — it all flows through here. "
                "Half of these systems are older than I am, but they "
                "still work. Barely.",
                {
                    {"What about the maintenance tunnels?", 1},
                    {"Can you repair my gear?", 2},
                    {"Keep up the good work.", -1},
                },
            },
            // Node 1: Maintenance tunnels hint
            {
                "The lower decks? Yeah, they're infested. Some kind of "
                "Xytomorph nest moved in a few cycles back. We sealed "
                "the hatch but if you're brave — or desperate — there's "
                "good salvage down there. Engine parts, conduit cores, "
                "the works. Just watch yourself. Those things bite.",
                {
                    {"Where's the hatch?", 3},
                    {"I'll gear up first.", -1},
                },
            },
            // Node 2: Repair bench
            {
                "The repair bench is under maintenance right now. "
                "Give me a few more cycles and I'll have it online. "
                "In the meantime, try not to break anything too badly.",
                {
                    {"I'll manage.", -1},
                },
            },
            // Node 3: Hatch location
            {
                "Should be in the storage area. Look for a floor hatch "
                "with caution markings. Can't miss it — assuming it's "
                "still sealed. Good luck down there.",
                {
                    {"Thanks.", -1},
                },
            },
        },
    };

    return npc;
}

} // namespace astra

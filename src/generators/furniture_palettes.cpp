#include "astra/settlement_types.h"

namespace astra {

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style) {
    FurniturePalette pal;

    switch (type) {
        case BuildingType::MainHall:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::TableSet,    FixtureType::Table,   FixtureType::Bench,   2, 4, 1.0f},
                {PlacementRule::WallShelf,   style.knowledge,      FixtureType::Shelf,   2, 3, 0.7f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   2, 4, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 2, 0.5f},
            };
            break;

        case BuildingType::Market:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Table,   FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   4, 6, 1.0f},
                {PlacementRule::WallShelf,   FixtureType::Shelf,   FixtureType::Shelf,   2, 3, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 3, 0.9f},
            };
            break;

        case BuildingType::Dwelling:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Bunk,    FixtureType::Bunk,    1, 1, 1.0f},
                {PlacementRule::TableSet,    FixtureType::Table,   FixtureType::Bench,   1, 1, 0.8f},
                {PlacementRule::WallUniform, style.cooking,        FixtureType::Table,   1, 1, 0.8f},
                {PlacementRule::WallUniform, style.knowledge,      FixtureType::Table,   1, 1, 0.3f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 2, 0.6f},
            };
            break;

        case BuildingType::Distillery:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   3, 5, 1.0f},
                {PlacementRule::Center,      FixtureType::Table,   FixtureType::Table,   1, 1, 0.6f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 4, 0.8f},
            };
            break;

        case BuildingType::Lookout:
            pal.groups = {
                {PlacementRule::Anchor,      style.knowledge,      FixtureType::Table,   1, 1, 0.9f},
                {PlacementRule::Center,      style.seating,        FixtureType::Table,   1, 1, 0.7f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   1, 1, 0.4f},
            };
            break;

        case BuildingType::Workshop:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Table,   FixtureType::Table,   1, 1, 1.0f},
                {PlacementRule::WallUniform, style.display,        FixtureType::Table,   2, 3, 0.8f},
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   1, 2, 0.5f},
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   2, 3, 0.7f},
            };
            break;

        case BuildingType::Storage:
            pal.groups = {
                {PlacementRule::Corner,      style.storage,        FixtureType::Table,   3, 4, 1.0f},
                {PlacementRule::WallUniform, FixtureType::Shelf,   FixtureType::Table,   3, 5, 0.7f},
                {PlacementRule::WallUniform, style.storage,        FixtureType::Table,   2, 4, 0.8f},
            };
            break;

        case BuildingType::Temple:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 0.7f},
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   3, 6, 0.8f},
                {PlacementRule::Center,      FixtureType::Debris,  FixtureType::Table,   2, 4, 0.9f},
            };
            break;

        case BuildingType::Vault:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Crate,   FixtureType::Table,   1, 2, 0.8f},
                {PlacementRule::Corner,      FixtureType::Debris,  FixtureType::Table,   2, 4, 0.9f},
                {PlacementRule::WallUniform, FixtureType::Shelf,   FixtureType::Table,   1, 3, 0.5f},
            };
            break;

        case BuildingType::GreatHall:
            pal.groups = {
                {PlacementRule::WallUniform, FixtureType::Conduit, FixtureType::Table,   4, 8, 0.8f},
                {PlacementRule::Center,      FixtureType::Debris,  FixtureType::Table,   3, 6, 1.0f},
            };
            break;

        case BuildingType::Archive:
            pal.groups = {
                {PlacementRule::WallShelf,   FixtureType::Shelf,   FixtureType::Shelf,   3, 6, 0.9f},
                {PlacementRule::Center,      FixtureType::Debris,  FixtureType::Table,   1, 3, 0.7f},
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 0.5f},
            };
            break;

        case BuildingType::Observatory:
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console, FixtureType::Table,   1, 1, 0.6f},
                {PlacementRule::Center,      FixtureType::Debris,  FixtureType::Table,   1, 2, 0.8f},
                {PlacementRule::Corner,      FixtureType::Debris,  FixtureType::Table,   1, 2, 0.7f},
            };
            break;

        case BuildingType::OutpostMain:
            // Fixture types are hardcoded (not style.*) because spawn_outpost_npcs
            // looks for Crate/Console/Bunk specifically to anchor NPCs.
            pal.groups = {
                {PlacementRule::Anchor,      FixtureType::Console,   FixtureType::Table, 1, 1, 1.0f},  // quest giver
                {PlacementRule::TableSet,    FixtureType::Table,     FixtureType::Bench, 1, 1, 1.0f},  // trader + seating
                {PlacementRule::WallUniform, FixtureType::Bunk,      FixtureType::Table, 1, 2, 1.0f},  // resting
                {PlacementRule::Corner,      FixtureType::Crate,     FixtureType::Table, 1, 2, 1.0f},  // loot / quartermaster
                {PlacementRule::WallUniform, FixtureType::CampStove, FixtureType::Table, 1, 1, 0.8f},  // cooking
            };
            break;
    }

    return pal;
}

} // namespace astra

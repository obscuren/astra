#include "astra/settlement_types.h"

namespace astra {

FurniturePalette furniture_palette(BuildingType type, const CivStyle& style) {
    FurniturePalette pal;
    //                          type,              freq, min,max, wall, clear, corner, center

    switch (type) {
        case BuildingType::MainHall:
            pal.entries = {
                // Central table arrangement
                {FixtureType::Table,   1.0f, 3, 6, false, false, false, true},
                // Seating around tables
                {style.seating,        0.9f, 2, 4, false, false, false, true},
                // Shelves/displays along walls
                {style.display,        0.7f, 2, 4, true,  true,  false, false},
                // Knowledge storage along walls
                {style.knowledge,      0.6f, 1, 2, true,  false, false, false},
                // Command console
                {FixtureType::Console, 0.8f, 1, 1, true,  true,  false, false},
                // Corner storage
                {style.storage,        0.5f, 1, 2, true,  false, true,  false},
            };
            break;

        case BuildingType::Market:
            pal.entries = {
                // Display racks along walls
                {style.display,  1.0f, 3, 6, true,  true,  false, false},
                // Storage crates in corners
                {style.storage,  0.9f, 2, 4, true,  false, true,  false},
                // Central counter/table
                {FixtureType::Table, 0.7f, 1, 2, false, false, false, true},
                // Seating for customers
                {style.seating,  0.5f, 1, 2, false, false, false, false},
            };
            break;

        case BuildingType::Dwelling:
            pal.entries = {
                // Bed/bunk
                {FixtureType::Bunk, 1.0f, 1, 2, true,  false, true,  false},
                // Cooking area
                {style.cooking,     0.8f, 1, 1, true,  true,  false, false},
                // Table with seating
                {FixtureType::Table, 0.8f, 1, 1, false, false, false, true},
                {style.seating,     0.7f, 1, 2, false, false, false, true},
                // Storage
                {style.storage,     0.6f, 1, 2, true,  false, true,  false},
                // Knowledge (bookshelf/terminal)
                {style.knowledge,   0.3f, 1, 1, true,  false, false, false},
            };
            break;

        case BuildingType::Distillery:
            pal.entries = {
                // Conduits/pipes along walls
                {FixtureType::Conduit, 1.0f, 3, 5, true,  false, false, false},
                // Vats/consoles
                {FixtureType::Console, 0.9f, 2, 3, true,  true,  false, false},
                // Storage barrels/crates
                {style.storage,        0.8f, 2, 4, true,  false, true,  false},
                // Central work table
                {FixtureType::Table,   0.6f, 1, 1, false, false, false, true},
            };
            break;

        case BuildingType::Lookout:
            pal.entries = {
                // Console/scope
                {style.knowledge, 0.9f, 1, 1, true,  true,  false, false},
                // Seating
                {style.seating,   0.7f, 1, 1, false, false, false, true},
                // Storage
                {style.storage,   0.4f, 1, 1, true,  false, true,  false},
            };
            break;

        case BuildingType::Workshop:
            pal.entries = {
                // Central workbenches
                {FixtureType::Table,   1.0f, 2, 3, false, false, false, true},
                // Tool racks along walls
                {style.display,        0.8f, 2, 3, true,  true,  false, false},
                // Storage in corners
                {style.storage,        0.7f, 2, 3, true,  false, true,  false},
                // Conduits/machinery
                {FixtureType::Conduit, 0.5f, 1, 2, true,  false, false, false},
            };
            break;

        case BuildingType::Storage:
            pal.entries = {
                // Lots of crates/storage
                {style.storage,      1.0f, 4, 8, true,  false, true,  false},
                {style.storage,      0.8f, 2, 4, true,  false, false, false},
                // Shelving
                {FixtureType::Shelf, 0.7f, 2, 3, true,  false, false, false},
            };
            break;
    }

    return pal;
}

} // namespace astra

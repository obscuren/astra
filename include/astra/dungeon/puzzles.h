#pragma once

#include <cstdint>
#include <string>
#include <utility>
#include <vector>

namespace astra::dungeon {

enum class PuzzleKind : uint8_t {
    SealedStairsDown = 0,
    // Future: SealedChamber, PressurePlateSequence, RuneAlignment, ...
};

struct RequiredPuzzle {
    PuzzleKind kind;
    uint32_t   depth_mask;   // which depths this puzzle runs on (depth_mask_bit(N))
};

// Per-level, per-puzzle runtime state. Persisted on TileMap.
// Kind-specific payload lives inline; future kinds may convert to std::variant.
struct PuzzleState {
    uint16_t   id         = 0;     // unique within the level (1-based; 0 = unlinked)
    PuzzleKind kind       = PuzzleKind::SealedStairsDown;
    bool       solved     = false;

    // SealedStairsDown payload:
    std::vector<std::pair<int,int>> sealed_tiles;           // floor -> StructuralWall positions
    std::pair<int,int>              button_pos  { -1, -1 }; // where the unlock button was placed
    std::pair<int,int>              stairs_pos  { -1, -1 }; // cached stairs_dn position
};

} // namespace astra::dungeon

// -- forward decls for apply_puzzles signature --
#include <random>

namespace astra { class TileMap; class Game; }

namespace astra::dungeon {

struct DungeonStyle;
struct LevelContext;

void apply_puzzles(astra::TileMap& map, const DungeonStyle& style,
                   LevelContext& ctx, std::mt19937& rng);

// Runtime unlock entry point. Invoked from dialog_manager when the player
// interacts with a PrecursorButton. `puzzle_id` comes from the fixture's
// puzzle_id field. Safe to call on unknown / already-solved ids (no-op).
void on_button_pressed(astra::Game& game, uint16_t puzzle_id);

} // namespace astra::dungeon

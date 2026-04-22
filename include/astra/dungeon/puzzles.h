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

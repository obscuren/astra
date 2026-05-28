#pragma once
#include <utility>
#include <vector>
namespace astra {
struct Netspace;
// True if (x,y) is in-bounds and a traversable pipe/port cell a payload rides.
bool is_pipe_cell(const Netspace& ns, int x, int y);
// Index of the NetRoom rect containing (x,y); else the room with min
// Chebyshev distance to its rect; -1 if ns.rooms is empty.
int room_index_at(const Netspace& ns, int x, int y);
// Strict INTERIOR variant: returns the room index whose interior
// (excluding wall ring) contains (x,y); -1 otherwise. No Chebyshev
// fallback. Use for engagement / adjacency checks that must be FALSE
// while the avatar is in a pipe OR on a room's pipe-port wall cell.
int room_index_at_strict(const Netspace& ns, int x, int y);
// Indices into ns.pipes whose A- or B-endpoint lies in the room at
// (ax,ay). Ascending order (deterministic Tab cycling).
std::vector<int> connected_pipe_indices(const Netspace& ns, int ax, int ay);
// Ordered pipe cells from the avatar-node end of ns.pipes[pipe_idx] to
// its far-node end. {} if pipe_idx invalid or no path.
std::vector<std::pair<int,int>> pipe_path_cells(const Netspace& ns,
                                                int pipe_idx, int ax, int ay);
// Floor a raw path length at 2 so 0/1-cell pipes still take a visible
// beat to traverse. Phase 5 S7e (2026-05-28): the [2,6] upper cap
// was dropped -- payload travel now scales 1:1 with physical pipe
// length, making pipe geometry a real tactical lever.
inline int clamp_seg_len(int raw) { return raw < 2 ? 2 : raw; }
}  // namespace astra

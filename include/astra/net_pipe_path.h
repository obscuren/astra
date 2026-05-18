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
// Indices into ns.pipes whose A- or B-endpoint lies in the room at
// (ax,ay). Ascending order (deterministic Tab cycling).
std::vector<int> connected_pipe_indices(const Netspace& ns, int ax, int ay);
// Ordered pipe cells from the avatar-node end of ns.pipes[pipe_idx] to
// its far-node end. {} if pipe_idx invalid or no path.
std::vector<std::pair<int,int>> pipe_path_cells(const Netspace& ns,
                                                int pipe_idx, int ax, int ay);
// Clamp a raw path length to the segment band [2,6] (Fork-2 decision).
inline int clamp_seg_len(int raw) { return raw < 2 ? 2 : (raw > 6 ? 6 : raw); }
}  // namespace astra

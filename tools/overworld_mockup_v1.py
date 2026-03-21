#!/usr/bin/env python3
"""Generate a colored overworld mockup using CP437 glyphs — v1 palette."""

import math
import random
import sys

W, H = 120, 50

# --- Value noise + fBm (matching the C++ implementation) ---

def hash_noise(x, y, seed):
    h = (x * 374761393 + y * 668265263 + seed * 1274126177) & 0xFFFFFFFF
    h = ((h ^ (h >> 13)) * 1103515245) & 0xFFFFFFFF
    h = h ^ (h >> 16)
    return (h & 0xFFFF) / 65535.0

def smooth_noise(fx, fy, seed):
    ix, iy = int(math.floor(fx)), int(math.floor(fy))
    dx, dy = fx - ix, fy - iy
    sx = dx * dx * (3.0 - 2.0 * dx)
    sy = dy * dy * (3.0 - 2.0 * dy)
    n00 = hash_noise(ix, iy, seed)
    n10 = hash_noise(ix + 1, iy, seed)
    n01 = hash_noise(ix, iy + 1, seed)
    n11 = hash_noise(ix + 1, iy + 1, seed)
    top = n00 + sx * (n10 - n00)
    bot = n01 + sx * (n11 - n01)
    return top + sy * (bot - top)

def fbm(x, y, seed, scale, octaves=4):
    value = 0.0
    amplitude = 1.0
    total = 0.0
    freq = scale
    for i in range(octaves):
        value += amplitude * smooth_noise(x * freq, y * freq, seed + i * 31)
        total += amplitude
        amplitude *= 0.5
        freq *= 2.0
    return value / total

# --- ANSI color helpers ---

def fg256(n):
    return f"\033[38;5;{n}m"

def bg256(n):
    return f"\033[48;5;{n}m"

RESET = "\033[0m"

# --- Terrain types ---

MOUNTAIN  = 0
LAKE      = 1
SWAMP     = 2
FOREST    = 3
PLAINS    = 4
DESERT    = 5
ICE       = 6
LAVA      = 7
RIVER     = 8
FUNGAL    = 9
CRATER    = 10
LANDING   = 11
CAVE      = 12
RUINS     = 13
SETTLE    = 14
CRASHED   = 15
OUTPOST   = 16

# Original v1 CP437 glyph choices
GLYPHS = {
    MOUNTAIN:  ['\u2206', '\u2229', '^'],  # △ ∩ ^
    LAKE:      ['\u2248'],                  # ≈
    SWAMP:     ['\u03C4', '"', ','],        # τ " ,
    FOREST:    ['\u2660', '\u03A6', '\u0192'],  # ♠ Φ ƒ
    PLAINS:    ['\u00B7', '.', ','],        # · . ,
    DESERT:    ['\u2591', '\u00B7', '.'],   # ░ · .
    ICE:       ['\u2591', '\u00B7', "'"],   # ░ · '
    LAVA:      ['\u2248', '~'],             # ≈ ~
    RIVER:     ['\u2248', '~'],             # ≈ ~
    FUNGAL:    ['\u03A6', '"', '\u03C4'],   # Φ " τ
    CRATER:    ['o', '\u00B0'],             # o °
    LANDING:   ['\u2261'],                  # ≡
    CAVE:      ['\u25BC'],                  # ▼
    RUINS:     ['\u03C0', '\u03A9'],        # π Ω
    SETTLE:    ['\u2666'],                  # ♦
    CRASHED:   ['%', '\u00A4'],             # % ¤
    OUTPOST:   ['+'],
}

# Original v1 single color per terrain
COLORS = {
    MOUNTAIN:  {'fg': 255, 'bg': 236},  # bright white on dark gray
    LAKE:      {'fg': 33,  'bg': 17},   # blue on dark blue
    SWAMP:     {'fg': 142, 'bg': 58},   # olive on dark olive
    FOREST:    {'fg': 28,  'bg': 22},   # green on dark green
    PLAINS:    {'fg': 34,  'bg': 0},    # green on black
    DESERT:    {'fg': 220, 'bg': 58},   # yellow on dark olive
    ICE:       {'fg': 159, 'bg': 17},   # light cyan on dark blue
    LAVA:      {'fg': 196, 'bg': 52},   # red on dark red
    RIVER:     {'fg': 39,  'bg': 0},    # light blue on black
    FUNGAL:    {'fg': 48,  'bg': 22},   # bright green on dark green
    CRATER:    {'fg': 245, 'bg': 0},    # gray on black
    LANDING:   {'fg': 226, 'bg': 0},    # bright yellow on black
    CAVE:      {'fg': 201, 'bg': 0},    # magenta on black
    RUINS:     {'fg': 176, 'bg': 0},    # light magenta on black
    SETTLE:    {'fg': 226, 'bg': 0},    # yellow on black
    CRASHED:   {'fg': 87,  'bg': 0},    # cyan on black
    OUTPOST:   {'fg': 118, 'bg': 0},    # bright green on black
}

DENSE_TERRAIN = {FOREST, SWAMP, DESERT, LAKE, LAVA, ICE, MOUNTAIN}

def classify(elev, moist, temp_seed):
    if elev > 0.72:
        return MOUNTAIN
    if elev < 0.22:
        return LAKE
    if elev < 0.30 and moist > 0.5:
        return SWAMP
    if moist > 0.65:
        return FOREST
    if moist > 0.50:
        return FUNGAL if random.random() < 0.3 else FOREST
    if moist > 0.30:
        return PLAINS
    if elev < 0.40:
        return DESERT
    return PLAINS

def main():
    seed_e = random.randint(0, 0xFFFFFFFF)
    seed_m = random.randint(0, 0xFFFFFFFF)

    grid = [[PLAINS] * W for _ in range(H)]
    elev = [[0.0] * W for _ in range(H)]

    for y in range(H):
        for x in range(W):
            e = fbm(x, y, seed_e, 0.06)
            m = fbm(x, y, seed_m, 0.10)
            elev[y][x] = e
            grid[y][x] = classify(e, m, seed_e)

    # Carve rivers
    sources = []
    for y in range(1, H - 1):
        for x in range(1, W - 1):
            if 0.55 < elev[y][x] < 0.72 and grid[y][x] != MOUNTAIN:
                adj_mt = any(grid[y+dy][x+dx] == MOUNTAIN
                             for dx in (-1, 0, 1) for dy in (-1, 0, 1)
                             if 0 <= x+dx < W and 0 <= y+dy < H)
                if adj_mt:
                    sources.append((x, y))

    random.shuffle(sources)
    for sx, sy in sources[:random.randint(3, 6)]:
        cx, cy = sx, sy
        visited = set()
        for _ in range(60):
            if cx <= 0 or cx >= W-1 or cy <= 0 or cy >= H-1:
                break
            if grid[cy][cx] == LAKE or grid[cy][cx] == RIVER:
                break
            if grid[cy][cx] != MOUNTAIN:
                grid[cy][cx] = RIVER
            visited.add((cx, cy))
            best_e, bx, by = elev[cy][cx], -1, -1
            for dx, dy in [(0,-1),(0,1),(-1,0),(1,0)]:
                nx, ny = cx+dx, cy+dy
                if 0 <= nx < W and 0 <= ny < H and (nx,ny) not in visited:
                    if grid[ny][nx] != MOUNTAIN and elev[ny][nx] < best_e:
                        best_e, bx, by = elev[ny][nx], nx, ny
            if bx < 0:
                for dx, dy in [(0,-1),(0,1),(-1,0),(1,0)]:
                    nx, ny = cx+dx, cy+dy
                    if 0 <= nx < W and 0 <= ny < H and (nx,ny) not in visited:
                        if grid[ny][nx] != MOUNTAIN:
                            bx, by = nx, ny
                            break
            if bx < 0:
                break
            cx, cy = bx, by

    # Place landing pad
    cx, cy = W // 2, H // 2
    for r in range(max(W, H)):
        found = False
        for dy in range(-r, r+1):
            for dx in range(-r, r+1):
                if abs(dx) != r and abs(dy) != r:
                    continue
                px, py = cx+dx, cy+dy
                if 0 <= px < W and 0 <= py < H:
                    if grid[py][px] in (PLAINS, DESERT, FOREST):
                        grid[py][px] = LANDING
                        found = True
                        break
            if found:
                break
        if found:
            break

    # Scatter POIs
    pois = [(CAVE, 4), (RUINS, 3), (SETTLE, 2), (CRASHED, 2), (OUTPOST, 2)]
    placed = []
    for poi_type, count in pois:
        attempts = 0
        c = 0
        while c < count and attempts < 200:
            attempts += 1
            px = random.randint(2, W-3)
            py = random.randint(2, H-3)
            if grid[py][px] not in (PLAINS, DESERT, FOREST, FUNGAL):
                continue
            if any(abs(px-qx) + abs(py-qy) < 8 for qx, qy in placed):
                continue
            grid[py][px] = poi_type
            placed.append((px, py))
            c += 1

    # Render
    rng = random.Random(42)
    output = []
    for y in range(H):
        line = []
        for x in range(W):
            t = grid[y][x]
            glyphs = GLYPHS[t]
            g = glyphs[rng.randint(0, len(glyphs) - 1)]
            c = COLORS[t]
            if t in DENSE_TERRAIN and c['bg'] != 0:
                line.append(f"{fg256(c['fg'])}{bg256(c['bg'])}{g}{RESET}")
            else:
                line.append(f"{fg256(c['fg'])}{g}{RESET}")
        output.append(''.join(line))

    title = " OVERWORLD MOCKUP — Temperate Terrestrial (120x50) "
    print(f"\033[1m{title:\u2550^{W+2}}\033[0m")
    for row in output:
        print(row)
    print(f"{'':═^{W+2}}")

    print()
    print("  LEGEND:")
    for t, name in [(MOUNTAIN, "Mountains"), (FOREST, "Forest"), (PLAINS, "Plains"),
                     (DESERT, "Desert"), (LAKE, "Lake"), (RIVER, "River"),
                     (SWAMP, "Swamp"), (FUNGAL, "Fungal"), (ICE, "Ice"),
                     (LAVA, "Lava"), (CRATER, "Crater"), (LANDING, "Landing"),
                     (CAVE, "Cave Entrance"), (RUINS, "Ruins"), (SETTLE, "Settlement"),
                     (CRASHED, "Crashed Ship"), (OUTPOST, "Outpost")]:
        glyphs = GLYPHS[t]
        c = COLORS[t]
        samples = []
        for g in glyphs:
            if t in DENSE_TERRAIN and c['bg'] != 0:
                samples.append(f"{fg256(c['fg'])}{bg256(c['bg'])}{g}{RESET}")
            else:
                samples.append(f"{fg256(c['fg'])}{g}{RESET}")
        print(f"    {' '.join(samples)}  {name}")

if __name__ == '__main__':
    main()

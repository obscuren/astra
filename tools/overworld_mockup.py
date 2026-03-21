#!/usr/bin/env python3
"""Generate a colored overworld mockup using CP437 glyphs."""

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

# Enriched CP437 glyph palette
GLYPHS = {
    MOUNTAIN:  ['\u2206', '\u2229', '^', '\u0393', '\u25B2'],  # △ ∩ ^ Γ ▲
    LAKE:      ['\u2248', '~'],                                  # ≈ ~
    SWAMP:     ['\u03BC', '\u221A', '\u03B2', '"', ','],         # µ √ β " ,
    FOREST:    ['\u2660', '\u0192', '\u2663', '\u221A'],         # ♠ ƒ ♣ √
    PLAINS:    ['\u00B7', '.', ',', '\u2022'],                   # · . , •
    DESERT:    ['\u2591', '\u03B4', '\u03C3', '\u00B0', '.'],    # ░ δ σ ° .
    ICE:       ['\u00B0', '\u2022', '\u00B7', "'"],              # ° • · '
    LAVA:      ['\u2248', '~', '\u2591'],                        # ≈ ~ ░
    RIVER:     ['\u2248', '~'],                                  # ≈ ~
    FUNGAL:    ['\u03C6', '\u03B2', '\u03BC', '\u2663'],         # φ β µ ♣
    CRATER:    ['\u03B5', '\u03C3', '\u2022', '\u00B0'],         # ε σ • °
    LANDING:   ['\u2261'],                                       # ≡
    CAVE:      ['\u0398'],                                       # Θ
    RUINS:     ['\u03C0', '\u03A9', '\u00A7', '\u03A3'],         # π Ω § Σ
    SETTLE:    ['\u2666'],                                       # ♦
    CRASHED:   ['\u2310', '\u00AC', '\u00BD'],                   # ⌐ ¬ ½
    OUTPOST:   ['+'],
}

# xterm-256 color palette — richer per-terrain palette
# Each terrain has a list of (fg, bg) pairs; renderer picks based on
# position hash so color varies spatially but deterministically.
COLOR_PALETTES = {
    MOUNTAIN:  [(255, 236), (250, 238), (245, 236)],     # white/gray peaks
    LAKE:      [(33, 17), (27, 18), (39, 17)],            # blue depths
    SWAMP:     [(142, 58), (101, 58), (136, 22)],         # murky olives
    FOREST:    [(28, 22), (34, 22), (22, 23), (29, 22)],  # varied greens
    PLAINS:    [(34, 0), (28, 0), (142, 0)],              # sparse green/tan
    DESERT:    [(220, 58), (178, 94), (180, 58)],         # yellows/tans
    ICE:       [(159, 17), (153, 18), (195, 17)],         # cyan/white frost
    LAVA:      [(196, 52), (208, 52), (202, 88)],         # reds/oranges
    RIVER:     [(39, 0), (33, 0), (75, 0)],               # flowing blues
    FUNGAL:    [(48, 22), (84, 22), (41, 23)],            # alien greens
    CRATER:    [(245, 0), (240, 0), (247, 0)],            # grays
    LANDING:   [(226, 0)],                                 # bright yellow
    CAVE:      [(201, 0)],                                 # magenta
    RUINS:     [(176, 0), (139, 0), (133, 0)],            # faded purples
    SETTLE:    [(226, 0), (220, 0)],                       # yellow
    CRASHED:   [(87, 0), (123, 0)],                        # cyan
    OUTPOST:   [(118, 0)],                                 # bright green
}

# Dense terrain types that use background color
DENSE_TERRAIN = {FOREST, SWAMP, DESERT, LAKE, LAVA, ICE, MOUNTAIN}

def pos_hash(x, y):
    """Deterministic hash for position-based variation."""
    h = x * 374761393 + y * 668265263
    h = ((h ^ (h >> 13)) * 1103515245) & 0xFFFFFFFF
    return h

def classify(elev, moist, temp_seed):
    """Classify terrain from elevation + moisture."""
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

    # Generate terrain
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

    # Render with position-based glyph + color variation
    output = []
    for y in range(H):
        line = []
        for x in range(W):
            t = grid[y][x]
            h = pos_hash(x, y)

            glyphs = GLYPHS[t]
            g = glyphs[h % len(glyphs)]

            palette = COLOR_PALETTES[t]
            fg_c, bg_c = palette[(h >> 8) % len(palette)]

            if t in DENSE_TERRAIN and bg_c != 0:
                line.append(f"{fg256(fg_c)}{bg256(bg_c)}{g}{RESET}")
            else:
                line.append(f"{fg256(fg_c)}{g}{RESET}")
        output.append(''.join(line))

    # Print with a border
    title = " OVERWORLD MOCKUP v2 — Temperate Terrestrial (120x50) "
    print(f"\033[1m{title:\u2550^{W+2}}\033[0m")
    for row in output:
        print(row)
    print(f"{'':═^{W+2}}")

    # Legend — show all glyphs per terrain
    print()
    print("  LEGEND:")
    for t, name in [(MOUNTAIN, "Mountains"), (FOREST, "Forest"), (PLAINS, "Plains"),
                     (DESERT, "Desert"), (LAKE, "Lake"), (RIVER, "River"),
                     (SWAMP, "Swamp"), (FUNGAL, "Fungal"), (ICE, "Ice"),
                     (LAVA, "Lava"), (CRATER, "Crater"), (LANDING, "Landing"),
                     (CAVE, "Cave Entrance"), (RUINS, "Ruins"), (SETTLE, "Settlement"),
                     (CRASHED, "Crashed Ship"), (OUTPOST, "Outpost")]:
        glyphs = GLYPHS[t]
        palette = COLOR_PALETTES[t]
        fg_c, bg_c = palette[0]
        samples = []
        for i, g in enumerate(glyphs):
            fg_i, bg_i = palette[i % len(palette)]
            if t in DENSE_TERRAIN and bg_i != 0:
                samples.append(f"{fg256(fg_i)}{bg256(bg_i)}{g}{RESET}")
            else:
                samples.append(f"{fg256(fg_i)}{g}{RESET}")
        glyph_str = ' '.join(samples)
        print(f"    {glyph_str}  {name}")

if __name__ == '__main__':
    main()

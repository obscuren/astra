# Astra

```
        .            *                .          |
   *         .              .                .  -o-
        _        _                   *           |
  .    / \   ___| |_ _ __ __ _           .
      / _ \ / __| __| '__/ _` |   .
 *   / ___ \\__ \ |_| | | (_| |        *
    /_/   \_\___/\__|_|  \__,_|  .
         .          *       .          .
   .            .                 *
```

[![Build](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml/badge.svg)](https://github.com/obscuren/astra/actions/workflows/cmake-multi-platform.yml)
[![License: MIT](https://img.shields.io/badge/License-MIT-yellow.svg)](LICENSE)
[![Latest Release](https://img.shields.io/github/v/release/obscuren/astra?include_prereleases&label=release)](https://github.com/obscuren/astra/releases)
[![Website](https://img.shields.io/badge/web-jeff.lookingforteam.com%2Fastra-blue)](https://jeff.lookingforteam.com/astra)

A sci-fi roguelike dungeon crawler set in the far future. Travel across star systems, crawl asteroid dungeons, upgrade your starship, and journey to the supermassive black hole at the center of the galaxy.

## Build

```bash
# Terminal only (default)
cmake -B build && cmake --build build

# With SDL3 support
cmake -B build -DSDL=ON && cmake --build build
```

Run: `./build/astra`

## Platforms

Linux, macOS, Windows

## License

[MIT](LICENSE)

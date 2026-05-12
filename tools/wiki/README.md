# Wiki Catalog Generator

Parses the Astra C++ source files and emits Markdown pages for the GitHub wiki.
Run it after adding or changing items, skills, or programs to keep the wiki in sync.

## What it generates

| Page | Source |
|---|---|
| `Items-Weapons.md` | Melee and ranged weapons grouped by class |
| `Items-Armor.md` | Body, head, feet, arm, shield, and accessory items |
| `Items-Cyberdecks.md` | Cyberdeck rigs |
| `Items-Implants.md` | Implants grouped by anatomical slot |
| `Items-Programs.md` | Program items with buy/sell prices |
| `Items-Consumables.md` | Rations, stims, grenades, mines, turrets, energy cells |
| `Items-Crafting-Materials.md` | Crafting materials, schematics, cookbooks, junk |
| `Skills.md` | Full skill tree by category (from `skill_defs.cpp`) |
| `Programs.md` | Cyberdeck programs grouped by kind (from `program.cpp`) |

The generator only writes these auto-generated pages. Any other files in the
wiki directory are left untouched.

## CLI usage

```
python3 tools/wiki/generate.py --repo <path-to-repo> --out <wiki-dir>
```

Example:

```bash
python3 tools/wiki/generate.py --repo $(pwd) --out /tmp/astra.wiki
```

`--repo` must point to the repository root (the directory containing `src/item_defs.cpp`).
`--out` must already exist — the tool will not create it.

## Cloning the wiki

```bash
git clone git@github.com:obscuren/astra.wiki.git /tmp/astra.wiki
```

## Regenerating after a content change

```bash
python3 tools/wiki/generate.py --repo $(pwd) --out /tmp/astra.wiki
```

## Pushing to the wiki

```bash
cd /tmp/astra.wiki
git add -A
git commit -m "wiki: regenerate catalog pages"
git push
```

## Notes

- The generator is run **manually** for now. It is not part of CI.
- Warnings about unparseable builders are printed to stderr and the item is
  skipped; the run still completes and a summary is printed at the end.
- Descriptions containing `desc_passive(...)`, `desc_active(...)`,
  `desc_trigger(...)`, and `display_name(DamageType::X)` calls are expanded
  to Markdown-styled text (`**Passive:**`, etc.) automatically.
- `COLOR_BEGIN`/`COLOR_END` runtime marker bytes are stripped so wiki pages
  contain only plain text.

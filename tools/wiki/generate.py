#!/usr/bin/env python3
"""
Astra wiki catalog generator.

Parses C++ source files and emits Markdown pages for the GitHub wiki.

Usage:
    python3 tools/wiki/generate.py --repo /path/to/crawler --out /tmp/astra.wiki
"""

import argparse
import os
import re
import sys
from collections import defaultdict
from typing import Optional, List, Dict, Any


# ---------------------------------------------------------------------------
# C++ inline helper → Markdown mappings
# ---------------------------------------------------------------------------

DAMAGE_TYPE_NAMES = {
    "Kinetic":    "Kinetic",
    "Plasma":     "Plasma",
    "Electrical": "Electrical",
    "Cryo":       "Cryo",
    "Acid":       "Acid",
}


def _extract_balanced_paren(text: str, start: int) -> str:
    """
    Starting just after an opening '(' at position start, return the contents
    up to the matching closing ')'.
    """
    depth = 1
    i = start
    while i < len(text) and depth > 0:
        c = text[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        i += 1
    return text[start:i - 1]


def _resolve_concat_expr(expr: str) -> str:
    """
    Resolve a C++ string-concat expression (mix of literals and helper calls)
    to plain text.  Called recursively from _expand_helpers_full.
    """
    # Gather tokens in order
    token_re = re.compile(
        r'"((?:[^"\\]|\\.)*)"'
        r'|desc_passive\s*\('
        r'|desc_active\s*\('
        r'|desc_trigger\s*\('
        r'|desc_dice\s*\('
        r'|desc_key\s*\('
        r'|desc_effect\s*\('
        r'|display_name\s*\(',
    )
    parts = []
    i = 0
    while i < len(expr):
        m = token_re.search(expr, i)
        if not m:
            break
        i = m.end()
        tok = m.group(0)
        if tok.startswith('"'):
            raw = m.group(1).replace('\\n', '\n').replace('\\t', '\t').replace('\\"', '"')
            parts.append(raw)
        elif tok.startswith('display_name'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1  # skip past closing ')'
            # display_name(DamageType::X)
            dm = re.search(r'DamageType\s*::\s*(\w+)', inner)
            if dm:
                parts.append(DAMAGE_TYPE_NAMES.get(dm.group(1), dm.group(1).lower()))
        elif tok.startswith('desc_effect'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            nm = re.search(r'"([^"]+)"', inner)
            if nm:
                parts.append(nm.group(1))
        elif tok.startswith('desc_key'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            km = re.search(r"[\"'](.+?)[\"']", inner)
            if km:
                parts.append(f'[{km.group(1)}]')
        elif tok.startswith('desc_dice'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            dm2 = re.search(r'"([^"]+)"', inner)
            if dm2:
                parts.append(dm2.group(1))
        elif tok.startswith('desc_passive'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            parts.append('**Passive:** ' + _resolve_concat_expr(inner))
        elif tok.startswith('desc_active'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            parts.append('**Active:** ' + _resolve_concat_expr(inner))
        elif tok.startswith('desc_trigger'):
            inner = _extract_balanced_paren(expr, i)
            i += len(inner) + 1
            parts.append('**Trigger:** ' + _resolve_concat_expr(inner))

    return ''.join(parts)


def expand_cpp_desc_helpers(text: str) -> str:
    """
    Expand C++ desc_* / display_name helper calls embedded in a text expression.
    Handles simple single-call forms via regex; falls back to paren-balanced
    walk for nested/concatenated forms.
    """
    return _resolve_concat_expr(text)


def strip_color_markers(text: str) -> str:
    """Strip COLOR_BEGIN (\\x02<byte>) and COLOR_END (\\x03) marker bytes."""
    text = re.sub(r'\x02.', '', text)
    text = re.sub(r'\x03', '', text)
    return text


def clean_description(raw: str) -> str:
    s = expand_cpp_desc_helpers(raw)
    s = strip_color_markers(s)
    s = re.sub(r'  +', ' ', s)
    return s.strip()


# ---------------------------------------------------------------------------
# item_defs.cpp parser
# ---------------------------------------------------------------------------

_FIELD_RE = {
    'name':         re.compile(r'\bit\.name\s*=\s*"((?:[^"\\]|\\.)*)"'),
    'type':         re.compile(r'\bit\.type\s*=\s*ItemType::(\w+)'),
    'slot':         re.compile(r'\bit\.slot\s*=\s*EquipSlot::(\w+)'),
    'required_implant_slot': re.compile(
        r'\bit\.required_implant_slot\s*=\s*ImplantSlotRequirement::(\w+)'),
    'rarity':       re.compile(r'\bit\.rarity\s*=\s*Rarity::(\w+)'),
    'buy_value':    re.compile(r'\bit\.buy_value\s*=\s*(\d+)'),
    'sell_value':   re.compile(r'\bit\.sell_value\s*=\s*(\d+)'),
    'weapon_class': re.compile(r'\bit\.weapon_class\s*=\s*WeaponClass::(\w+)'),
}

_MOD_RE = re.compile(r'\bit\.modifiers\.(\w+)\s*=\s*([^;]+);')
_DESC_START_RE = re.compile(r'\bit\.description\s*=')


def _collect_builder_blocks(source: str):
    """
    Yield (func_name, block_text) for every `Item build_<name>() { … }`.
    """
    header_re = re.compile(r'^Item\s+(build_\w+)\s*\(\s*\)\s*\{', re.MULTILINE)
    for m in header_re.finditer(source):
        func_name = m.group(1)
        start = m.end()
        depth = 1
        pos = start
        while pos < len(source) and depth > 0:
            ch = source[pos]
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
            pos += 1
        block = source[start:pos - 1]
        yield func_name, block


def _parse_description_from_block(block: str) -> str:
    """Extract description string from a builder block (multi-line-safe)."""
    m = _DESC_START_RE.search(block)
    if not m:
        return ''

    fragment = block[m.end():]
    # Find terminating ';' tracking paren depth
    depth = 0
    i = 0
    while i < len(fragment):
        c = fragment[i]
        if c == '(':
            depth += 1
        elif c == ')':
            depth -= 1
        elif c == ';' and depth == 0:
            break
        i += 1
    expr = fragment[:i]

    result = _resolve_concat_expr(expr)
    result = strip_color_markers(result)
    result = re.sub(r'  +', ' ', result)
    return result.strip()


# ---------------------------------------------------------------------------
# Delegation-pattern parsers
# ---------------------------------------------------------------------------

def _parse_delegate_build_cell(block: str) -> Optional[Dict]:
    """
    build_cell(DEF_ID, id, "Name", Rarity::X, capacity, weight, buy, sell[, slot])
    Returns partial item dict or None.
    """
    m = re.search(
        r'build_cell\s*\(\s*\w+\s*,\s*\d+\s*,\s*"((?:[^"\\]|\\.)*)"'
        r'\s*,\s*Rarity::(\w+)'
        r'[\s\S]*?,\s*(\d+)\s*,\s*(\d+)\s*[,\)]',
        block,
    )
    if not m:
        return None
    # buy/sell: need to get 5th and 6th numeric args after "Name", Rarity::X
    # Pattern: build_cell(DEF, id, "Name", Rarity::X, capacity, weight, buy, sell, ...)
    args_re = re.search(
        r'build_cell\s*\('
        r'[^,]+,\s*'          # def_id
        r'\d+\s*,\s*'         # id
        r'"(?:[^"\\]|\\.)*"\s*,\s*'   # name
        r'Rarity::\w+\s*,\s*'  # rarity
        r'\d+\s*,\s*'          # capacity
        r'\d+\s*,\s*'          # weight
        r'(\d+)\s*,\s*'        # buy
        r'(\d+)',              # sell
        block,
    )
    name = m.group(1)
    rarity = m.group(2)
    buy = int(args_re.group(1)) if args_re else 0
    sell = int(args_re.group(2)) if args_re else 0
    return {
        'name': name,
        'type': 'Battery',
        'slot': '',
        'required_implant_slot': '',
        'rarity': rarity,
        'buy_value': buy,
        'sell_value': sell,
        'weapon_class': '',
        'description': 'Persistent power cell. Holds energy for weapons, shields, and gadgets.',
        'modifiers': {},
    }


def _parse_delegate_build_consumable(block: str) -> Optional[Dict]:
    """
    build_consumable_(DEF_ID, id, "Name", "Desc", ItemType::X, Rarity::X, buy, sell)
    """
    m = re.search(
        r'build_consumable_\s*\('
        r'[^,]+,\s*'           # def_id
        r'\d+\s*,\s*'          # id
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # name
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # desc
        r'ItemType::(\w+)\s*,\s*'        # type
        r'Rarity::(\w+)\s*,\s*'          # rarity
        r'(\d+)\s*,\s*'        # buy
        r'(\d+)',              # sell
        block,
        re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': m.group(3),
        'slot': 'Thrown' if m.group(3) in ('Grenade', 'Mine') else '',
        'required_implant_slot': '',
        'rarity': m.group(4),
        'buy_value': int(m.group(5)),
        'sell_value': int(m.group(6)),
        'weapon_class': '',
        'description': m.group(2),
        'modifiers': {},
    }


def _parse_delegate_build_schematic(block: str) -> Optional[Dict]:
    """
    build_schematic_(DEF_ID, id, "Name", schematic_id, Rarity::X, buy, sell)
    """
    m = re.search(
        r'build_schematic_\s*\('
        r'[^,]+,\s*'
        r'\d+\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'  # name
        r'\d+\s*,\s*'
        r'Rarity::(\w+)\s*,\s*'         # rarity
        r'(\d+)\s*,\s*'                  # buy
        r'(\d+)',                         # sell
        block,
        re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': 'Schematic',
        'slot': '',
        'required_implant_slot': '',
        'rarity': m.group(2),
        'buy_value': int(m.group(3)),
        'sell_value': int(m.group(4)),
        'weapon_class': '',
        'description': 'A folded schematic. Read to permanently learn the recipe.',
        'modifiers': {},
    }


def _parse_delegate_solar_panel(block: str) -> Optional[Dict]:
    """build_solar_panel_(DEF, id, "Name", Rarity::X, buy, sell)"""
    m = re.search(
        r'build_solar_panel_\s*\('
        r'[^,]+,\s*'
        r'\d+\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'
        r'Rarity::(\w+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)',
        block, re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': 'CraftingMaterial',
        'slot': '',
        'required_implant_slot': '',
        'rarity': m.group(2),
        'buy_value': int(m.group(3)),
        'sell_value': int(m.group(4)),
        'weapon_class': '',
        'description': 'Photovoltaic mod. Slots into any energy item; recharges it while outdoors.',
        'modifiers': {},
    }


def _parse_delegate_energy_mod(block: str) -> Optional[Dict]:
    """build_energy_mod_(DEF, id, "Name", "Desc", Rarity::X, buy, sell)"""
    m = re.search(
        r'build_energy_mod_\s*\('
        r'[^,]+,\s*'
        r'\d+\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'
        r'Rarity::(\w+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)',
        block, re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': 'CraftingMaterial',
        'slot': '',
        'required_implant_slot': '',
        'rarity': m.group(3),
        'buy_value': int(m.group(4)),
        'sell_value': int(m.group(5)),
        'weapon_class': '',
        'description': m.group(2),
        'modifiers': {},
    }


def _parse_delegate_make_program(block: str) -> Optional[Dict]:
    """
    make_program_(DEF, inv_id, ProgramId::X, "name.exe", "desc", Rarity::X, buy, sell)
    """
    m = re.search(
        r'make_program_\s*\('
        r'[^,]+,\s*'
        r'\d+\s*,\s*'
        r'ProgramId::\w+\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # name
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # desc
        r'Rarity::(\w+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)',
        block, re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': 'Program',
        'slot': '',
        'required_implant_slot': '',
        'rarity': m.group(3),
        'buy_value': int(m.group(4)),
        'sell_value': int(m.group(5)),
        'weapon_class': '',
        'description': m.group(2),
        'modifiers': {},
    }


def _parse_delegate_compiled_program(block: str) -> Optional[Dict]:
    """
    build_compiled_program_(DEF, inv_id, "name", "desc", ..., Rarity::X, buy, sell)
    """
    m = re.search(
        r'build_compiled_program_\s*\('
        r'[^,]+,\s*'
        r'\d+\s*,\s*'
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # name
        r'"((?:[^"\\]|\\.)*?)"\s*,\s*'   # desc
        r'[\s\S]*?'
        r'Rarity::(\w+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)',
        block, re.DOTALL,
    )
    if not m:
        return None
    return {
        'name': m.group(1),
        'type': 'Program',
        'slot': '',
        'required_implant_slot': '',
        'rarity': m.group(3),
        'buy_value': int(m.group(4)),
        'sell_value': int(m.group(5)),
        'weapon_class': '',
        'description': m.group(2),
        'modifiers': {},
    }


# Ordered list of delegation parsers to try when direct field extraction fails
DELEGATE_PARSERS = [
    _parse_delegate_build_cell,
    _parse_delegate_build_consumable,
    _parse_delegate_build_schematic,
    _parse_delegate_solar_panel,
    _parse_delegate_energy_mod,
    _parse_delegate_make_program,
    _parse_delegate_compiled_program,
]


def parse_items(item_defs_path: str):
    with open(item_defs_path, encoding='utf-8', errors='replace') as fh:
        source = fh.read()

    items = []
    skipped = []

    for func_name, block in _collect_builder_blocks(source):
        # Skip the dispatch function itself and internal factory helpers
        if func_name in ('build_by_def_id', 'build_by_def_id_impl'):
            continue

        item: Dict[str, Any] = {
            'func': func_name,
            'name': '',
            'type': '',
            'slot': '',
            'required_implant_slot': '',
            'rarity': '',
            'buy_value': 0,
            'sell_value': 0,
            'weapon_class': '',
            'description': '',
            'modifiers': {},
        }

        try:
            # First try direct field extraction (fully-expanded builders)
            for field, pat in _FIELD_RE.items():
                fm = pat.search(block)
                if fm:
                    item[field] = fm.group(1)

            item['buy_value'] = int(item['buy_value']) if item['buy_value'] else 0
            item['sell_value'] = int(item['sell_value']) if item['sell_value'] else 0

            for mm in _MOD_RE.finditer(block):
                item['modifiers'][mm.group(1)] = mm.group(2).strip()

            item['description'] = _parse_description_from_block(block)

            # If direct extraction got a name we're done
            if item['name']:
                items.append(item)
                continue

            # Try delegation parsers
            delegated = None
            for parser in DELEGATE_PARSERS:
                delegated = parser(block)
                if delegated:
                    break

            if delegated:
                delegated['func'] = func_name
                # Overlay any direct-field values (e.g. bulwark_cell overrides description)
                if item['description']:
                    delegated['description'] = item['description']
                for k, v in item['modifiers'].items():
                    delegated['modifiers'][k] = v
                items.append(delegated)
            else:
                # build_battery is an alias — skip silently
                if 'build_battery' in func_name:
                    continue
                # build_standard_energy_cell delegate
                if 'return build_' in block and not 'it.' in block:
                    continue  # pure alias, skip silently
                print(f'WARNING: no name found in {func_name}, skipping', file=sys.stderr)
                skipped.append(func_name)

        except Exception as exc:
            print(f'WARNING: failed to parse {func_name}: {exc}', file=sys.stderr)
            skipped.append(func_name)

    return items, skipped


# ---------------------------------------------------------------------------
# program.cpp parser
# ---------------------------------------------------------------------------

def parse_programs(program_cpp_path: str):
    with open(program_cpp_path, encoding='utf-8', errors='replace') as fh:
        source = fh.read()

    m = re.search(
        r'static const std::vector<ProgramDef>\s+regs\s*=\s*\{(.*?)\}\s*;',
        source, re.DOTALL,
    )
    if not m:
        print('WARNING: could not find program_registry() vector in program.cpp', file=sys.stderr)
        return []

    body = m.group(1)
    entry_re = re.compile(
        r'\{\s*ProgramId\s*::\s*(\w+)\s*,\s*'
        r'ProgramKind\s*::\s*(\w+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'(\d+)\s*,\s*'
        r'"([^"]+)"\s*,\s*'
        r'"([^"]+)"\s*,\s*'
        r'"([^"]+)"',
        re.DOTALL,
    )
    programs = []
    for em in entry_re.finditer(body):
        programs.append({
            'id':          em.group(1),
            'kind':        em.group(2),
            'tier':        int(em.group(3)),
            'ram':         int(em.group(4)),
            'heat':        int(em.group(5)),
            'name':        em.group(6),
            'file':        em.group(7),
            'description': em.group(8).strip(),
        })
    return programs


# ---------------------------------------------------------------------------
# skill_defs.cpp parser
# ---------------------------------------------------------------------------

def _extract_desc_helper(source: str, func_name: str) -> str:
    """Find `static std::string <func_name>() { ... }` and extract plain text."""
    pat = re.compile(
        r'static\s+std::string\s+' + re.escape(func_name) + r'\s*\(\s*\)\s*\{',
    )
    fm = pat.search(source)
    if not fm:
        return ''
    start = fm.end()
    depth = 1
    pos = start
    while pos < len(source) and depth > 0:
        c = source[pos]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        pos += 1
    body = source[start:pos - 1]
    parts = []
    for sm in re.finditer(r'"((?:[^"\\]|\\.)*)"', body):
        raw = sm.group(1).replace('\\n', '\n').replace('\\"', '"').replace('\\t', '\t')
        parts.append(raw)
    text = ''.join(parts)
    text = strip_color_markers(text)
    text = re.sub(r'  +', ' ', text).strip()
    return text


def parse_skills(skill_defs_path: str):
    with open(skill_defs_path, encoding='utf-8', errors='replace') as fh:
        source = fh.read()

    m = re.search(
        r'const\s+std::vector<SkillCategory>&\s+skill_catalog\s*\(\s*\)\s*\{',
        source,
    )
    if not m:
        print('WARNING: could not find skill_catalog() in skill_defs.cpp', file=sys.stderr)
        return []

    start = m.end()
    depth = 1
    pos = start
    while pos < len(source) and depth > 0:
        c = source[pos]
        if c == '{':
            depth += 1
        elif c == '}':
            depth -= 1
        pos += 1
    catalog_body = source[start:pos - 1]

    cat_re = re.compile(
        r'\{\s*SkillId\s*::\s*(Cat_\w+)\s*,\s*"([^"]+)"\s*,\s*',
    )

    # Sub-skill: stat_name can be nullptr or "String"
    skill_re = re.compile(
        r'\{\s*SkillId\s*::\s*(\w+)\s*,\s*"([^"]+)"\s*,\s*'
        r'(\w+\s*\(\s*\)|"[^"]*")'
        r'\s*,\s*(true|false)\s*,'
        r'\s*(\d+)\s*,'
        r'\s*(\d+)\s*,'
        r'\s*(nullptr|"[^"]*")\s*\}',
        re.DOTALL,
    )

    categories = []
    for cm in cat_re.finditer(catalog_body):
        cat_id   = cm.group(1)
        cat_name = cm.group(2)
        fragment = catalog_body[cm.end():]

        # Description
        desc_text = ''
        dm = re.match(r'\s*(\w+)\s*\(', fragment)
        if dm and dm.group(1).endswith('_description'):
            desc_text = _extract_desc_helper(source, dm.group(1))
        else:
            parts = []
            tmp = fragment
            while True:
                sm2 = re.match(r'\s*"((?:[^"\\]|\\.)*)"', tmp)
                if not sm2:
                    break
                parts.append(sm2.group(1).replace('\\n', '\n').replace('\\"', '"'))
                tmp = tmp[sm2.end():]
            desc_text = ''.join(parts).strip()

        # Category SP cost
        cost_m = re.search(r',\s*(\d+)\s*,\s*\{', fragment)
        cat_cost = int(cost_m.group(1)) if cost_m else 0

        # Sub-skills block
        brace_m = re.search(r',\s*\d+\s*,\s*\{', fragment)
        sub_skills = []
        if brace_m:
            sub_start = brace_m.end()
            sub_end = sub_start
            depth2 = 1
            while sub_end < len(fragment) and depth2 > 0:
                c2 = fragment[sub_end]
                if c2 == '{':
                    depth2 += 1
                elif c2 == '}':
                    depth2 -= 1
                sub_end += 1
            sub_body = fragment[sub_start:sub_end - 1]

            for sm in skill_re.finditer(sub_body):
                sk_id        = sm.group(1)
                sk_name      = sm.group(2)
                sk_desc_tok  = sm.group(3).strip()
                sk_passive   = sm.group(4) == 'true'
                sk_cost      = int(sm.group(5))
                sk_stat_req  = sm.group(6)
                sk_stat_raw  = sm.group(7)
                sk_stat_name = (sk_stat_raw.strip('"')
                                if sk_stat_raw != 'nullptr' else None)

                if sk_desc_tok.endswith('()') or '_description' in sk_desc_tok:
                    fname = sk_desc_tok.rstrip('()').strip()
                    sk_desc = _extract_desc_helper(source, fname)
                else:
                    inner = re.match(r'"((?:[^"\\]|\\.)*)"', sk_desc_tok)
                    sk_desc = inner.group(1) if inner else sk_desc_tok

                sub_skills.append({
                    'id':        sk_id,
                    'name':      sk_name,
                    'description': sk_desc.strip(),
                    'passive':   sk_passive,
                    'sp_cost':   sk_cost,
                    'stat_req':  sk_stat_req,
                    'stat_name': sk_stat_name,
                })

        categories.append({
            'id':          cat_id,
            'name':        cat_name,
            'description': desc_text,
            'sp_cost':     cat_cost,
            'skills':      sub_skills,
        })

    return categories


# ---------------------------------------------------------------------------
# Classification helpers
# ---------------------------------------------------------------------------

IMPLANT_SLOT_ORDER = [
    'Eyes', 'Head', 'Spine', 'Chest', 'AnyHand', 'AnyArm', 'AnyLeg',
]

IMPLANT_SLOT_DISPLAY = {
    'Eyes':    'Eyes',
    'Head':    'Head',
    'Spine':   'Spine',
    'Chest':   'Chest',
    'AnyHand': 'Hand',
    'AnyArm':  'Arm',
    'AnyLeg':  'Leg',
}

WEAPON_CLASS_DISPLAY = {
    'Pistol':     'Pistol',
    'Rifle':      'Rifle',
    'ShortBlade': 'Short Blade',
    'LongBlade':  'Long Blade',
    'Blunt':      'Blunt',
}

ARMOR_SLOT_DISPLAY = {
    'Body':     'Body',
    'Head':     'Head',
    'Feet':     'Feet',
    'LeftArm':  'Arm',
    'RightArm': 'Arm',
    'Shield':   'Shield',
    'Face':     'Face',
    'Back':     'Back',
}

FOOTER = (
    "\n---\n\n"
    "_This page is generated from `src/item_defs.cpp` (and friends). "
    "Run `python3 tools/wiki/generate.py --out <wiki>` to regenerate._\n"
)

SKILLS_FOOTER = "\n---\n\n_Generated from `src/skill_defs.cpp`._\n"
PROGRAMS_FOOTER = "\n---\n\n_Generated from `src/program.cpp`._\n"


def _esc(s: str) -> str:
    return s.replace('|', '&#124;').replace('\n', ' ').strip()


def fmt_credits(val: int) -> str:
    return '—' if val == 0 else f'{val} cr'


# ---------------------------------------------------------------------------
# Page generators
# ---------------------------------------------------------------------------

def gen_weapons(items):
    melee  = [i for i in items if i['type'] == 'MeleeWeapon']
    ranged = [i for i in items if i['type'] == 'RangedWeapon']

    lines = ['# Weapons\n\nMelee and ranged weapons found throughout the galaxy.\n']

    for section_name, group in [('Ranged Weapons', ranged), ('Melee Weapons', melee)]:
        if not group:
            continue
        lines.append(f'\n## {section_name}\n')
        by_class = defaultdict(list)
        for it in group:
            wc = WEAPON_CLASS_DISPLAY.get(it['weapon_class'], it['weapon_class'] or 'Other')
            by_class[wc].append(it)
        for wc_name, wgroup in sorted(by_class.items()):
            lines.append(f'\n### {wc_name}\n\n')
            lines.append('| Name | Rarity | Buy | Sell | Effect |\n')
            lines.append('|---|---|---|---|---|\n')
            for it in sorted(wgroup, key=lambda x: x['buy_value']):
                lines.append(
                    f"| {_esc(it['name'])} "
                    f"| {it['rarity']} "
                    f"| {fmt_credits(it['buy_value'])} "
                    f"| {fmt_credits(it['sell_value'])} "
                    f"| {_esc(it['description'])} |\n"
                )

    lines.append(FOOTER)
    return ''.join(lines)


def gen_armor(items):
    armor_types = {'Armor', 'Shield', 'Accessory'}
    armor = [i for i in items if i['type'] in armor_types]

    lines = ['# Armor & Shields\n\nBody protection, head gear, shields, and accessories.\n']

    slot_order = ['Body', 'Head', 'Feet', 'Arm', 'Shield', 'Face', 'Back']
    slot_display = {
        'Body':   'Body Armor',
        'Head':   'Head Armor',
        'Feet':   'Boots',
        'Arm':    'Arm Guards',
        'Shield': 'Energy Shields',
        'Face':   'Visors & Goggles',
        'Back':   'Back Accessories',
    }

    by_slot = defaultdict(list)
    for it in armor:
        slot = ARMOR_SLOT_DISPLAY.get(it['slot'], it['slot'] or 'Other')
        by_slot[slot].append(it)

    rendered = set()
    for slot_key in slot_order:
        group = by_slot.get(slot_key, [])
        if not group or slot_key in rendered:
            continue
        rendered.add(slot_key)
        lines.append(f'\n## {slot_display.get(slot_key, slot_key)}\n\n')
        lines.append('| Name | Rarity | Buy | Sell | Effect |\n')
        lines.append('|---|---|---|---|---|\n')
        for it in sorted(group, key=lambda x: x['buy_value']):
            lines.append(
                f"| {_esc(it['name'])} "
                f"| {it['rarity']} "
                f"| {fmt_credits(it['buy_value'])} "
                f"| {fmt_credits(it['sell_value'])} "
                f"| {_esc(it['description'])} |\n"
            )

    # Remaining slots not in the order list
    for slot_key, group in sorted(by_slot.items()):
        if slot_key in rendered:
            continue
        lines.append(f'\n## {slot_key}\n\n')
        lines.append('| Name | Rarity | Buy | Sell | Effect |\n')
        lines.append('|---|---|---|---|---|\n')
        for it in sorted(group, key=lambda x: x['buy_value']):
            lines.append(
                f"| {_esc(it['name'])} "
                f"| {it['rarity']} "
                f"| {fmt_credits(it['buy_value'])} "
                f"| {fmt_credits(it['sell_value'])} "
                f"| {_esc(it['description'])} |\n"
            )

    lines.append(FOOTER)
    return ''.join(lines)


def gen_cyberdecks(items):
    decks = [i for i in items if i['type'] == 'Cyberdeck']
    lines = [
        '# Cyberdecks\n\n',
        'Cyberdecks are portable hacking rigs. Equip to the Utility slot to jack into the Relay Network.\n\n',
        '## Decks\n\n',
        '| Name | Rarity | Buy | Sell | Description |\n',
        '|---|---|---|---|---|\n',
    ]
    for it in sorted(decks, key=lambda x: x['buy_value']):
        lines.append(
            f"| {_esc(it['name'])} "
            f"| {it['rarity']} "
            f"| {fmt_credits(it['buy_value'])} "
            f"| {fmt_credits(it['sell_value'])} "
            f"| {_esc(it['description'])} |\n"
        )
    lines.append(FOOTER)
    return ''.join(lines)


def gen_implants(items):
    implant_types = {'Implant', 'RelayCortex'}
    implants = [i for i in items if i['type'] in implant_types]

    lines = [
        '# Implants\n\n',
        'Implants are installed into one of the anatomical slots via the PDA Implants tab.\n',
    ]

    by_slot = defaultdict(list)
    for it in implants:
        slot = it.get('required_implant_slot') or 'Other'
        by_slot[slot].append(it)

    seen = set()
    for slot_key in IMPLANT_SLOT_ORDER + sorted(
            set(by_slot.keys()) - set(IMPLANT_SLOT_ORDER)):
        group = by_slot.get(slot_key)
        if not group or slot_key in seen:
            continue
        seen.add(slot_key)
        display = IMPLANT_SLOT_DISPLAY.get(slot_key, slot_key)
        lines.append(f'\n## {display}\n\n')
        lines.append('| Name | Rarity | Buy | Effect |\n')
        lines.append('|---|---|---|---|\n')
        for it in sorted(group, key=lambda x: x['buy_value']):
            lines.append(
                f"| {_esc(it['name'])} "
                f"| {it['rarity']} "
                f"| {fmt_credits(it['buy_value'])} "
                f"| {_esc(it['description'])} |\n"
            )

    lines.append(FOOTER)
    return ''.join(lines)


def gen_programs_items(items):
    progs = [i for i in items if i['type'] == 'Program']
    lines = [
        '# Programs (Items)\n\n',
        "Cyberdeck programs are inventory items. Load them into a deck's program slots to use in the Grid.\n\n",
        '## All Programs\n\n',
        '| Name | Rarity | Buy | Sell | Description |\n',
        '|---|---|---|---|---|\n',
    ]
    for it in sorted(progs, key=lambda x: x['buy_value']):
        lines.append(
            f"| {_esc(it['name'])} "
            f"| {it['rarity']} "
            f"| {fmt_credits(it['buy_value'])} "
            f"| {fmt_credits(it['sell_value'])} "
            f"| {_esc(it['description'])} |\n"
        )
    lines.append(FOOTER)
    return ''.join(lines)


def gen_consumables(items):
    consumable_types = {'Food', 'Stim', 'Grenade', 'Mine', 'Turret', 'Battery'}
    cons = [i for i in items if i['type'] in consumable_types]

    lines = [
        '# Consumables\n\n',
        'Single-use items: rations, stims, grenades, mines, turrets, and energy cells.\n',
    ]

    type_display = {
        'Battery': 'Energy Cells',
        'Food':    'Food & Rations',
        'Stim':    'Stims & Medkits',
        'Grenade': 'Grenades',
        'Mine':    'Mines',
        'Turret':  'Deployable Turrets',
    }
    type_order = ['Battery', 'Food', 'Stim', 'Grenade', 'Mine', 'Turret']

    by_type = defaultdict(list)
    for it in cons:
        by_type[it['type']].append(it)

    for t in type_order:
        group = by_type.get(t, [])
        if not group:
            continue
        lines.append(f'\n## {type_display.get(t, t)}\n\n')
        lines.append('| Name | Rarity | Buy | Sell | Description |\n')
        lines.append('|---|---|---|---|---|\n')
        for it in sorted(group, key=lambda x: x['buy_value']):
            lines.append(
                f"| {_esc(it['name'])} "
                f"| {it['rarity']} "
                f"| {fmt_credits(it['buy_value'])} "
                f"| {fmt_credits(it['sell_value'])} "
                f"| {_esc(it['description'])} |\n"
            )

    lines.append(FOOTER)
    return ''.join(lines)


def gen_crafting_materials(items):
    mat_types = {'CraftingMaterial', 'Schematic', 'Cookbook', 'Ingredient', 'Junk'}
    mats = [i for i in items if i['type'] in mat_types]

    lines = [
        '# Crafting Materials & Schematics\n\n',
        'Raw materials, schematics (recipes), cookbooks, and junk salvage.\n',
    ]

    type_display = {
        'CraftingMaterial': 'Crafting Materials',
        'Schematic':        'Schematics',
        'Cookbook':         'Cookbooks',
        'Ingredient':       'Ingredients',
        'Junk':             'Junk & Salvage',
    }
    type_order = ['CraftingMaterial', 'Schematic', 'Cookbook', 'Ingredient', 'Junk']

    by_type = defaultdict(list)
    for it in mats:
        by_type[it['type']].append(it)

    for t in type_order:
        group = by_type.get(t, [])
        if not group:
            continue
        lines.append(f'\n## {type_display.get(t, t)}\n\n')
        lines.append('| Name | Rarity | Buy | Sell | Description |\n')
        lines.append('|---|---|---|---|---|\n')
        for it in sorted(group, key=lambda x: x['buy_value']):
            lines.append(
                f"| {_esc(it['name'])} "
                f"| {it['rarity']} "
                f"| {fmt_credits(it['buy_value'])} "
                f"| {fmt_credits(it['sell_value'])} "
                f"| {_esc(it['description'])} |\n"
            )

    lines.append(FOOTER)
    return ''.join(lines)


def gen_skills_page(categories):
    lines = [
        '# Skills\n\n',
        'Skills are purchased with SP at the PDA Skills tab. '
        'Categories (`Cat_*`) unlock the sub-skills beneath them.\n',
    ]

    for cat in categories:
        lines.append(f'\n## {cat["name"]}\n\n')
        cat_desc = cat['description'].replace('\n', ' ').strip()
        if cat_desc:
            lines.append(f'{cat_desc}\n\n')
        lines.append('| Skill | Cost | Prereq | Effect |\n')
        lines.append('|---|---|---|---|\n')
        lines.append(
            f"| **{cat['name']} (Category)** "
            f"| {cat['sp_cost']} SP "
            f"| — "
            f"| Unlocks sub-skills. |\n"
        )
        for sk in cat['skills']:
            prereq = (
                f"{sk['stat_req']}+ {sk['stat_name']}"
                if sk['stat_req'] and sk['stat_req'] != '0' and sk['stat_name']
                else '—'
            )
            desc = _esc(sk['description']) if sk['description'] else '—'
            lines.append(
                f"| {_esc(sk['name'])} "
                f"| {sk['sp_cost']} SP "
                f"| {prereq} "
                f"| {desc} |\n"
            )

    lines.append(SKILLS_FOOTER)
    return ''.join(lines)


def gen_programs_page(programs):
    lines = [
        '# Programs\n\n',
        'Cyberdeck programs grouped by kind. '
        "Load into a deck's program slots before jacking in.\n",
    ]

    kind_order = ['Atk', 'Utl', 'Stl', 'Qh']
    kind_display = {
        'Atk': 'ATK — Attack Programs',
        'Utl': 'UTL — Utility Programs',
        'Stl': 'STL — Stealth Programs',
        'Qh':  'QH — Quickhacks',
    }

    by_kind = defaultdict(list)
    for p in programs:
        by_kind[p['kind']].append(p)

    for kind in kind_order + sorted(set(by_kind.keys()) - set(kind_order)):
        group = by_kind.get(kind, [])
        if not group:
            continue
        lines.append(f'\n## {kind_display.get(kind, kind)}\n\n')
        lines.append('| Name | Tier | RAM | Heat | Effect |\n')
        lines.append('|---|---|---|---|---|\n')
        for p in sorted(group, key=lambda x: (x['tier'], x['name'])):
            lines.append(
                f"| `{_esc(p['file'])}` "
                f"| {p['tier']} "
                f"| {p['ram']} "
                f"| {p['heat']} "
                f"| {_esc(p['description'])} |\n"
            )

    lines.append(PROGRAMS_FOOTER)
    return ''.join(lines)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    p = argparse.ArgumentParser(
        description='Generate Astra wiki catalog pages from C++ source.')
    p.add_argument('--repo', required=True, help='Path to the Astra repository root.')
    p.add_argument('--out',  required=True, help='Output directory (must already exist).')
    args = p.parse_args()

    item_defs = os.path.join(args.repo, 'src', 'item_defs.cpp')
    if not os.path.isfile(item_defs):
        print(f'ERROR: {item_defs} not found. Is --repo correct?', file=sys.stderr)
        sys.exit(1)
    if not os.path.isdir(args.out):
        print(f'ERROR: --out directory {args.out!r} does not exist. Create it first.',
              file=sys.stderr)
        sys.exit(1)

    skill_defs  = os.path.join(args.repo, 'src', 'skill_defs.cpp')
    program_cpp = os.path.join(args.repo, 'src', 'program.cpp')

    print('Parsing item_defs.cpp...')
    items, item_skipped = parse_items(item_defs)
    print(f'  {len(items)} items parsed, {len(item_skipped)} skipped.')

    print('Parsing skill_defs.cpp...')
    if os.path.isfile(skill_defs):
        categories = parse_skills(skill_defs)
        skill_count = sum(len(c['skills']) for c in categories)
        print(f'  {len(categories)} categories, {skill_count} sub-skills.')
    else:
        print('  skill_defs.cpp not found — Skills.md will be a stub.', file=sys.stderr)
        categories = []

    print('Parsing program.cpp...')
    if os.path.isfile(program_cpp):
        programs = parse_programs(program_cpp)
        print(f'  {len(programs)} programs parsed.')
    else:
        print('  program.cpp not found — Programs.md will be a stub.', file=sys.stderr)
        programs = []

    pages = {
        'Items-Weapons.md':            gen_weapons(items),
        'Items-Armor.md':              gen_armor(items),
        'Items-Cyberdecks.md':         gen_cyberdecks(items),
        'Items-Implants.md':           gen_implants(items),
        'Items-Programs.md':           gen_programs_items(items),
        'Items-Consumables.md':        gen_consumables(items),
        'Items-Crafting-Materials.md': gen_crafting_materials(items),
        'Skills.md': (gen_skills_page(categories)
                      if categories else
                      '# Skills\n\n_TODO: could not parse skill_defs.cpp._\n'),
        'Programs.md': (gen_programs_page(programs)
                        if programs else
                        '# Programs\n\n_TODO: could not parse program.cpp._\n'),
    }

    written = 0
    for filename, content in pages.items():
        out_path = os.path.join(args.out, filename)
        with open(out_path, 'w', encoding='utf-8') as fh:
            fh.write(content)
        written += 1
        print(f'  Wrote {out_path}')

    total_items  = len(items) + len(programs) + sum(len(c['skills']) for c in categories)
    total_skip   = len(item_skipped)
    print(f'\nGenerated {written} pages with {total_items} items total; {total_skip} skipped.')


if __name__ == '__main__':
    main()

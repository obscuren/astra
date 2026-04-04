# Narrative Template System Design

**Date:** 2026-04-04  
**Status:** Approved

## Overview

A composable sentence-fragment system that generates multi-sentence lore records from event parameters. Each record is a discoverable in-game item (data crystal, memory engram, encoded transmission, archival fragment) that tells a story about a specific historical event, figure, or artifact.

## Record Structure

```cpp
struct LoreRecord {
    std::string title;          // "The Fall of Thexvorn", "Transmission 7-Kappa"
    std::string body;           // 2-12 sentences of generated narrative
    RecordStyle style;          // Official, Personal, Legend, Scientific, Transmission
    RecordReliability reliability;  // Verified, Disputed, Myth, Propaganda
    std::string source;         // "Aelithae Archival Authority", "Unknown survivor"
    int event_index = -1;       // which event this references (-1 = general)
    int figure_index = -1;      // which figure this references
    int artifact_index = -1;    // which artifact this references
    uint32_t system_id = 0;     // where this record can be found
};
```

## Five Narrative Styles

### Official Records
- **Voice:** Third-person, dry, authoritative
- **Tone:** Factual, bureaucratic, confident
- **Sources:** "[Civ] Archival Authority", "Central Command Dispatch", "[Civ] Historical Registry"
- **Example:** "By decree of the Central Authority, the colony at Thexvorn was established during the third expansion wave. Initial surveys confirmed viable atmospheric composition. Colonist manifest: 12,400 settlers across four generation ships."

### Personal Accounts
- **Voice:** First-person, emotional, intimate
- **Tone:** Reflective, fearful, awed, grieving
- **Sources:** "Recovered journal, author unknown", "Personal log of [Figure]", "Unsigned letter"
- **Example:** "I was there when the vault opened. The light that poured out was not light at all — it was something older, something that remembered being looked at. Three of us went in. I am the only one who came back, and I am not certain I came back whole."

### Legends/Myths
- **Voice:** Third-person omniscient, poetic, mythic
- **Tone:** Grand, reverent, ominous
- **Sources:** "Oral tradition, transcribed", "[Civ] creation myth, fragment", "The Song of [Figure]"
- **Example:** "In the age before silence, when the Aelithae still walked among stars, there came a Sage who looked into the heart of the void and did not look away. What they saw there changed them, and what they became changed everything."

### Scientific Reports
- **Voice:** Third-person, clinical, precise
- **Tone:** Detached, methodical, occasionally alarmed
- **Sources:** "[Place] Research Station, Lab Report [N]", "Xenoarchaeology Division", "[Figure], Chief Researcher"
- **Example:** "Sample analysis of Artifact XR-7 confirms operation on principles inconsistent with known physics. Energy output exceeds input by factor of 10^4. Temporal displacement detected at molecular level. Recommend immediate containment protocol Alpha."

### Transmissions
- **Voice:** Second-person or imperative, urgent, fragmented
- **Tone:** Desperate, cryptic, broken
- **Sources:** "Intercepted transmission, origin: [System]", "Emergency broadcast", "Final transmission from [Place]"
- **Example:** "...signal degrading... they breached level seven... the guardians are not what we thought... if anyone receives this: do NOT activate the beacon at [Place]... it doesn't call for help, it calls for—[END OF TRANSMISSION]"

## Template Composition

Each narrative is built from composable sentence fragments:

### Fragment Types

```
OPENER    — sets the scene (1 sentence)
CONTEXT   — provides background (1-2 sentences)
ACTION    — what happened (1-3 sentences)
OUTCOME   — consequences (1-2 sentences)
MYSTERY   — unanswered question or ominous hint (0-1 sentences)
```

### Slot Variables

Templates use `{var}` slots filled from event context:

```
{civ}       — civilization short name ("Aelithae")
{civ_full}  — full civilization name ("The Aelithae Convergence")
{place}     — place name ("Thexvorn Prime")
{figure}    — figure name ("Archon Ithaemund")
{title}     — figure title ("the Sage")
{pred}      — predecessor civilization name
{artifact}  — artifact name
{tech}      — technology style ("quantum-lattice")
{arch}      — architecture style ("crystalline")
{phil}      — philosophy ("contemplative")
{count}     — a generated number
{years}     — a time span
{collapse}  — collapse cause description
```

### Composition Rules

1. Pick a style (weighted by event type — battles favor Official/Personal, discoveries favor Scientific, ancient events favor Legend)
2. Select OPENER from the style's opener pool for this event type
3. Select CONTEXT (50% chance of inclusion for short records)
4. Select 1-3 ACTION fragments
5. Select OUTCOME
6. 30% chance of MYSTERY suffix
7. Concatenate with appropriate spacing

### Record Length

- **Short (2-3 sentences):** OPENER + ACTION — fragments, logs, brief transmissions
- **Medium (4-6 sentences):** OPENER + CONTEXT + ACTION + OUTCOME — reports, journal entries
- **Long (8-12 sentences):** Full composition with MYSTERY — legends, historical accounts

Distribution: 30% short, 50% medium, 20% long.

## Contradictory Records (15%)

When generating records for an event, 15% chance of producing a **paired contradiction**:

1. Generate the "official" version (winner's perspective)
2. Generate a counter-narrative (different style, different interpretation)
3. Mark both with the same event_index so they can be cross-referenced

### Contradiction Patterns

- **Battle:** Official says "decisive victory." Personal account says "massacre of civilians."
- **Figure:** Legend says "hero who saved the colony." Disputed record says "tyrant who burned the dissidents."
- **Discovery:** Scientific report says "artifact is safe." Transmission says "they're all dead."
- **Collapse:** Official says "orderly evacuation." Fragment says "they left us behind."

## Record Titles

Generated from templates per style:

- **Official:** "Decree of [event]", "Registry Entry: [place]", "Dispatch [N]-[letter]"
- **Personal:** "The Day [event happened]", "What I Saw at [place]", "Letter from [place]"
- **Legend:** "The Song of [figure]", "When [civ] [verbed]", "The [adjective] [noun]"
- **Scientific:** "Research Note: [subject]", "Analysis of [artifact/place]", "Lab Report [N]"
- **Transmission:** "Transmission [N]-[Greek letter]", "Emergency Broadcast from [place]", "Signal Fragment [N]"

## In-Game Representation

Records are discoverable items:
- **Data Crystal** — translucent, glowing item. Contains one record.
- **Memory Engram** — biological storage. Often personal accounts.
- **Encoded Transmission** — intercepted signal fragment.
- **Archival Fragment** — physical medium, partially degraded.

When examined (via inventory), the full text is displayed in the journal.

## Records Per Civilization

Each civilization generates **8-15 lore records** covering:
- Major events (battles, discoveries, collapses)
- Key figures (achievements, fates)
- Artifacts (creation, powers)
- General atmosphere (daily life, philosophy, culture)

With 4-8 civilizations, that's **32-120 lore records** scattered across the galaxy. Plus human-epoch records.

## Generation Performance

Template composition is string concatenation — fast. The bulk of generation time is in the number of records and the RNG calls. 20 seconds total for world generation (including lore + records) is acceptable.

## Integration Points

- `LoreRecord` stored in `WorldLore` alongside civilizations
- Records placed as ground items during overworld/dungeon generation (Phase 2)
- Readable via journal/inventory inspection
- Archaeology skills affect: Ruin Reader reveals more detail, Precursor Linguist translates alien scripts
- Contradictory records create natural quest hooks ("find the other side of this story")

## Implementation Structure

```
include/astra/narrative_templates.h  — NarrativeGenerator class
src/narrative_templates.cpp          — template pools and composition logic
```

The `NarrativeGenerator` takes a `Civilization`, event/figure/artifact context, and RNG, and produces a `LoreRecord`. Called by `LoreGenerator` after events/figures/artifacts are created.

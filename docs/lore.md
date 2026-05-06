# Astra — Lore

**Status:** Living document. Captures the full story arc of Astra: setting, deep history, the player's role, the macro-arc, and the deliberate mysteries we leave open. This is the *narrative* doc; design docs live in `docs/superpowers/specs/`. Mechanic numbers live in `docs/mechanics.md`. Item stats live in `docs/items.md`.

---

## 1. Premise

Astra is a sci-fi roguelike set in a far future where humanity has spread across the Milky Way and discovered it was never the first to do so. The galaxy is layered, dangerous, and *old.* Civilizations have risen and fallen for billions of years; the oldest ones left infrastructure nobody alive fully understands. The player is one tiny operator picking through that inheritance — a tinker, a marksman, a melee scrapper, a **Drifter**, or some combination. The path is theirs to choose; most viable runs blend two or more.

Every run begins on The Heavens Above (Jupiter station). Every run ends — if it ends well — at **Sagittarius A***, the galactic core, where the player is *touched* by the Substrate and reborn into a new life with fragments of the many travelers who came before.

---

## 2. The Heavens Above

The player wakes on **The Heavens Above** — Jupiter's great orbital, humanity's central station, the gateway between the Sol system and the wider galaxy. It's a port, a market, a refuge, a launching point. Vendors of all factions trade here (some don't even speak the same language). Shipwrights can fuel up, repair, refit. Tinkers can buy their first parts. Marksmen can pick up a sidearm. Aspiring Drifters can find their first **Resonator,** a starter **Sigil,** and the first hints of what lies further out.

The player's own starship is docked here. Its hold is small. Its Hyperspace Engine is the cheapest grade. Its Navi Computer can plot maybe one or two systems out before it loses confidence. From the Heavens Above the galaxy is a faint pinprick chart of possibility.

---

## 3. The galaxy and its peoples

The galaxy is real — not a simulation, not a procedural shape. Astra renders the actual map of the Milky Way, seeded into a generated state. About 80% of systems hold a station — most of them left, in some form, by an ancient civilization. Asteroid belts litter the spaces between. Asteroids can be landed on; many hold both surface terrain and underground dungeons.

### 3.1. Major factions (high-level)

- **Terran Federation** — humanity's largest political body. Bureaucratic, military, corporate-flavored, omnipresent in the inner sphere.
- **Kreth Mining Guild** — heavy-industry interstellar guild. Runs much of the resource economy.
- **Veldrani Accord** — old polity, semi-aristocratic, deep tradition.
- **Sylphari Wanderers** — nomadic culture, distrustful of fixed power.
- **Stellari Conclave** — alien. Reclusive. Their elders sometimes mutter the word "Substrate" to those who look like they might survive hearing it.
- **Xytomorph Hive** — alien. Collective intelligence, enormous in distributed scale.
- **Void Reavers** — pirate faction. Actively hostile.
- **Archon Remnants** — descendants (or fragments of descendants) of one of the pre-civilizations. They know more than they say. The few willing to speak speak in riddles.
- **The Drift Collective** — loose, decentralized, the cultural home of Drifters. Less an organization than a name people give themselves when they share enough of the same scars.
- **Feral** — wild, unaligned, often dangerous. Animals, reverted humans, mutated colonies.

(See `include/astra/faction.h` for the canonical faction list. Faction-specific lore expands in future docs.)

---

## 4. The Relay Network

What most people call it.

The Relay Network is an ancient, galaxy-spanning lattice of **Sites** — stations, nodes, conduits, Chambers — that connects systems across the disk of the Milky Way. It carries signals faster than light. It holds Caches of inscriptions and impressions left by long-dead minds. It supports the FTL navigation modern starships rely on. Most modern factions tap it for everyday utility (banking, archive lookups, communication) without ever realizing what they're tapping into.

It was built by **pre-civilizations** whose names survive only in fragments — Archons, Precursors, the Builders, others. They built it eons ago. Then they vanished.

The Network is **decaying.** Sites lie abandoned. Half-functional. Wardens — automated guardians from a forgotten era — still patrol some Chambers, on protocols they no longer understand. Many Caches are corrupted. Many Inner Gates won't open at all. Travelers say the Network is **going dark** — it's been growing dimmer for centuries, with no one able to explain why.

The Relay Network is the *surface* of the truth. Most of the galaxy never goes deeper.

---

## 5. The Substrate

Drifters who go deep enough hear whispers of it. From Stellari elders. From an Archon wanderer in a wayhouse. From a Drifter who's drifted too long, talking to themselves at a vendor's stall. They call it **the Substrate.**

The Substrate is the layer beneath the Network. It exists in dimensions the player cannot perceive — a 4th, 5th, or other dimension of *something sentient.* It is older than the Network, older than the pre-civilizations, possibly older than the galaxy. It is, in some manner the human mind cannot quite hold, **alive.**

The pre-civilizations did not *build* the Substrate. They built the Network *on top of it.* The Network is a technical scaffold — a way for finite beings to interface with the infinite-strange thing beneath. The Network's power comes from the Substrate; the Substrate sustains the Network with energies the modern factions still can't reproduce.

As the Substrate withdraws — or is consumed, or simply tires — the Network goes dark.

### 5.1. The Substrate's nature

The Substrate has one defining trait: **it assimilates.**

Not from malice. Not from will. The Substrate assimilates the way a river flows or a star burns — because that's what it does. A mind that couples deeply enough is *taken in,* its memories rearranged, its self rewritten. The Substrate does not ask permission and does not bear blame. It simply does what it is.

In exchange, the assimilated mind is *given.* Aeons of accumulated knowledge — fragments of every traveler who has ever been taken in — are layered into it. A new mind comes back: not the same person, not a stranger, something stitched between.

This is the deep mechanism behind the player's macro-arc. The Substrate has been doing this for as long as it has existed. The number of travelers folded into it is incomprehensible.

### 5.2. Drifters and the shallow end

Drifters operate at the *shallow* end of the Substrate. They couple briefly through their Resonator, do their work, decouple. They do not get fully assimilated.

But the Substrate still *touches* them. A long-time Drifter starts to notice things:
- Words in tongues they don't speak.
- Half-remembered places they've never been.
- Knowledge that wasn't in their head this morning.
- A growing sense that they have, in some sense, *always been here.*

These are minor brushes — fragments leaking through. The deeper a Drifter goes, the more frequent they become.

### 5.3. The "Substrate" as a name

"Substrate" is what people *call* it when they have to name it. It is acknowledged among those who know that the name is inadequate — a placeholder, a polite acknowledgment that the thing has no name we could pronounce. Some Stellari elders refuse to call it anything at all. Some Archon Remnants call it *the Inwards.* The Drift Collective tends to use *the Substrate* and leave it at that.

---

## 6. The pre-civilizations

Names whispered in the deepest Sites. The **Archons.** The **Precursors.** The **Builders.** Other names lost.

They built the Relay Network. Then they were gone. The galaxy holds their stations, their Chambers, their writing — and almost nothing else. No bodies. No active outposts. No clear records of what they were.

The Archon Remnants — modern descendants of one such pre-civ — survive in scattered enclaves, decayed in their own way. They speak only obliquely. When pressed, the kindest of them will say: "They went where everything goes." Most won't say even that.

There is a strong unspoken consensus among scholars and Drifters that the pre-civilizations *reached Sgr A***, and were assimilated. It is not provable. It is widely believed.

---

## 7. Sgr A* — the galactic core

The end of all paths.

**Sagittarius A*** sits at the heart of the Milky Way — a supermassive black hole, four million solar masses, a singularity older than human civilization by every meaningful margin. Every relay route, every starlane, every blackhole transit eventually points inward toward it.

Drifter rumor — and the more careful murmurs of Stellari elders — says Sgr A* is the **heart of the Substrate.** The place where the Substrate is most awake, most accessible, most assimilating. Reaching it is not death. Reaching it is being *fully assimilated and re-emitted* — the deepest version of what the Substrate does.

This is the canonical mechanism behind **rebirth.** A traveler who reaches Sgr A* — Drifter or not — enters the Substrate fully. Their body dies. Their mind enters the assimilation. Aeons of accumulated knowledge are layered in. A new mind is given back — in a new body, on a new run, with the prior runs and the older travelers' fragments stitched in. Drifters arrive better-prepared to interpret what's happening; non-Drifters are taken in just the same.

The traveler who comes out of Sgr A* is not the traveler who went in. They carry the same name, perhaps. They are not the same.

---

## 8. The player and the available paths

The player is a generic protagonist — a small operator out of The Heavens Above with a starship and an empty bag. **What the player *becomes*** is determined by the paths they pursue. Astra offers several **professions**; most viable runs blend two or more. None of them are mandatory. None of them are the player's identity by default.

The professions, in broad strokes (each has its own deeper lore in future docs):

- **Tinker.** Builds, modifies, deploys. Turrets, mines, drones, gear-crafting from schematics. The shop-floor mind. Reaches into ancient tech with a wrench.
- **Marksman.** Pistols, rifles, energy weapons. Range and precision. Stays out of the worst of it.
- **Brawler / Melee.** Close work. Blades, fists, augmented limbs. The shortest distance between you and a problem.
- **Drifter.** The one path this lore doc treats in depth, because it is the path that touches the Substrate. **Drifters couple to the Relay Network through a Resonator and run Sigils** — small operators who exploit the inheritance of the pre-civilizations without quite understanding what they're exploiting. Scavengers. Wildcatters. Not scientists, not priests. The path that goes *deepest.*
- **Other paths exist** and will be detailed in their own lore expansions — pilots, traders, settlers, scrapped-together hybrids. The roster grows.

### 8.1. The Drifter profession

A Drifter is **never just a Drifter.** Drifting pairs with another build. Tinker + Drift. Marksman + Drift. Melee + Drift. The Drifter contributes control, prep, sabotage, intel — never the kill. The kill comes from the rest of you.

A Drifter is **part of the wider population.** The galaxy holds traders, soldiers, tinkerers, marksmen, settlers, pirates — and a smattering of Drifters across the human, alien, and Remnant worlds. The Drifter path is one of many. It is the only path that goes *deep* into the Substrate.

The rest of this lore doc focuses on the Substrate side of Astra's setting; that's the angle Drifters care about. Tinkers have their own story (workshops, schematics, ancient tech they can rebuild). Marksmen have theirs (the gun-cultures of various factions). Melee fighters have theirs (martial traditions, augmentation, the close-distance ethic). Each gets its own deepening.

---

## 9. The macro arc — a single run

A typical run, in lore terms (path-neutral except where noted):

1. **Wake on The Heavens Above.** Pick up your starting kit — guns, gear, schematics, or a Resonator and a starter Sigil, depending on the path you're taking. A small starship.
2. **Travel.** Star to star, system to system, blackhole transit by blackhole transit. Asteroid belts, abandoned outposts, dungeon-rich worlds. Faction encounters. The galaxy is large.
3. **Build your run.** Profession-specific: Tinkers gather schematics and craft. Marksmen hunt for better weapons and ammo. Melee fighters chase combat skill and augmentations. **Drifters couple into local Relay Sites, walk Chambers, loot Caches, Bind targets, read Crystals from the dead, and build their Sigil library and Resonator capacity.** Most runs blend two or more.
4. **Go deeper.** The further out you travel, the older the inheritance gets. Stations get stranger. Enemies get stronger. For Drifters specifically: references to "the Substrate" begin to appear in NPC dialogue. The Network begins to feel *alive* in ways it didn't at the surface.
5. **Approach the core.** Long blackhole chains, dangerous transits, fewer maintained stations. The galactic core nears.
6. **Sgr A*.** The final approach. Whatever ritual or mechanic delivers the player into Sgr A*'s event horizon. **Rebirth.**
7. **Begin again.** A new run starts. The new traveler carries fragments — of the prior run, and of older travelers folded into the Substrate before. Each rebirth deepens the layering, regardless of which path the next run takes.

The game does *not* end at Sgr A*. The game **renews** there.

Note: rebirth happens to *anyone* who reaches Sgr A* — Drifter or not. Drifters arrive better-prepared to interpret what's happening, but the assimilation does not require their understanding. A pure tinker who reaches the core is also taken in, also re-emitted. The Substrate doesn't care.

---

## 10. The going dark — the open question

Why is the Network going dark?

This is a deliberately unresolved mystery. Several readings are compatible with what is known:

- **The Substrate is winding down.** Too few travelers in modern times compared to the pre-civ era. The energies that sustain the Network are thinning. Long, slow death.
- **Something is eating the Substrate.** Some other assimilator — same nature, different appetite — is pulling stratum *elsewhere.* The Network goes dark because its source is being drained by a rival.
- **The pre-civilizations took something with them.** They reached Sgr A*, were assimilated, and in doing so changed the Substrate's relationship to the Network. The Network has been bleeding out ever since.
- **The Substrate is intentionally withdrawing.** For reasons no living mind could grasp. A choice not made by any agent we'd recognize as choosing.

The Drifter is not required to solve this. The Drifter explores. Resolution, if any, lives in the deepest content — and may be left intentionally ambiguous even there.

---

## 11. Open mysteries — left open on purpose

Things this lore document deliberately does **not** resolve:

- What the Substrate *is*, in the dimensions it inhabits.
- Whether the Substrate has agency in any sense humans recognize.
- What happened to the pre-civilizations — did they ascend, were they consumed, did they choose Sgr A*, did they leave?
- Whether the Drifter's accumulated fragments include those of any *specific* pre-civ traveler — i.e., whether the player is, in some sense, *carrying an Archon* without knowing it.
- What waits at Sgr A* beyond the rebirth.
- Whether the Eater (if there is an Eater) is hostile, indifferent, or alien-natured in the same way the Substrate is.
- How Crystals were originally made, and whether modern Crystals are degraded copies.
- Why some species (Stellari, Archon Remnants) seem to know more than others.

These are content seams. Future story specs may or may not close them. Many *should* stay open — Astra's tone is *the unknown,* not *the explained.*

---

## 12. Tone notes for writers and designers

- **Sober, technical, archaeological** for surface description. Writers should describe Sites, Chambers, Wardens, Caches the way a careful investigator would describe a depressurized derelict — clean, observed, undramatic. Reference texture: *The Expanse*, *Roadside Picnic*, *Annihilation*'s clinical first half.
- **Spare mystical undertones** under the surface. When the Substrate or the deep Network surfaces in writing, allow a single line of strangeness. Don't over-flower it. Reverence is shown by *restraint*, not adjectives.
- **Not horror.** The Substrate is alien but not malevolent. The Wardens are dangerous but not cruel. The going dark is sad in the way a tide going out is sad — natural, not vindictive. Avoid horror tropes.
- **Drifter voice.** Drifters are scavengers, not sages. They don't lecture. When a Drifter narrates, they should sound like someone who has seen things they don't understand and gotten used to that.
- **Faction voices vary.** Stellari speak in measured, ceremonial language. Archon Remnants speak in incomplete sentences and old idioms. Terran Federation officials speak like middle managers. Drifters speak like anyone who has been on the road too long.

---

## 13. Cross-references

- **Manifesto** (technical roadmap for Drift / Relay design): `docs/superpowers/specs/2026-05-05-grid-loop-manifesto.md`
- **Spec 1** (Marks, Drifter combat, XP): `docs/superpowers/specs/2026-05-05-grid-spine-design.md`
- **Mechanics** (numbers, formulas): `docs/mechanics.md`
- **Items** (gear, Sigils, Crystals): `docs/items.md`
- **Factions** (canonical list): `include/astra/faction.h`

---

## 14. Status

Living document. Expand as design touches new pieces of the universe. Story-arc expansions, faction deepening, named pre-civ characters, named Sites, and named Eater hypotheses all welcome here.

#include "astra/narrative_templates.h"

#include <algorithm>
#include <sstream>

namespace astra {

// ── Helpers ────────────────────────────────────────────────────────────────

static int pick(std::mt19937& rng, int n) {
    return static_cast<int>(rng() % static_cast<unsigned>(n));
}

static float uniform_f(std::mt19937& rng, float lo, float hi) {
    std::uniform_real_distribution<float> d(lo, hi);
    return d(rng);
}

// Replace all occurrences of {key} in text with value.
static std::string fill(std::string text, const std::string& key, const std::string& val) {
    std::string token = "{" + key + "}";
    size_t pos = 0;
    while ((pos = text.find(token, pos)) != std::string::npos) {
        text.replace(pos, token.size(), val);
        pos += val.size();
    }
    return text;
}

static std::string fill_all(std::string text, const NarrativeContext& ctx) {
    text = fill(text, "civ", ctx.civ);
    text = fill(text, "civ_full", ctx.civ_full);
    text = fill(text, "pred", ctx.pred);
    text = fill(text, "tech", ctx.tech);
    text = fill(text, "arch", ctx.arch);
    text = fill(text, "phil", ctx.phil);
    text = fill(text, "collapse", ctx.collapse);
    text = fill(text, "place", ctx.place);
    text = fill(text, "place2", ctx.place2);
    text = fill(text, "figure", ctx.figure);
    text = fill(text, "figure_name", ctx.figure_name);
    text = fill(text, "artifact", ctx.artifact);
    text = fill(text, "count", std::to_string(ctx.count));
    text = fill(text, "years", std::to_string(ctx.years));
    return text;
}

// ── Style-appropriate openers ──────────────────────────────────────────────

// Each array: first index = event category (0=emergence/colony, 1=conflict,
// 2=discovery/science, 3=construction, 4=collapse/decline, 5=general)

static int event_category(LoreEventType t) {
    switch (t) {
    case LoreEventType::Emergence:
    case LoreEventType::ColonyFounded:
    case LoreEventType::Colonization:
    case LoreEventType::Terraforming:
        return 0;
    case LoreEventType::CivilWar:
    case LoreEventType::ResourceWar:
    case LoreEventType::SystemBattle:
    case LoreEventType::BorderConflict:
    case LoreEventType::ArtifactWar:
    case LoreEventType::LastStand:
    case LoreEventType::WeaponTestSite:
        return 1;
    case LoreEventType::RuinDiscovery:
    case LoreEventType::Decipherment:
    case LoreEventType::ReverseEngineering:
    case LoreEventType::SgrADetection:
    case LoreEventType::ConvergenceDiscovery:
    case LoreEventType::ScientificBreakthrough:
    case LoreEventType::PrecursorBreakthrough:
    case LoreEventType::AlienBiology:
    case LoreEventType::VaultDiscovered:
        return 2;
    case LoreEventType::MegastructureBuilt:
    case LoreEventType::OrbitalConstruction:
    case LoreEventType::HyperspaceRoute:
    case LoreEventType::UndergroundCity:
        return 3;
    case LoreEventType::Fragmentation:
    case LoreEventType::SelfDestruction:
    case LoreEventType::Transcendence:
    case LoreEventType::Consumption:
    case LoreEventType::CollapseUnknown:
    case LoreEventType::AbandonedOutpost:
    case LoreEventType::Plague:
    case LoreEventType::SurfaceScared:
    case LoreEventType::CrashSite:
        return 4;
    default:
        return 5;
    }
}

// ── OFFICIAL style fragments ───────────────────────────────────────────────

static const char* official_openers[] = {
    "By decree of the {civ} Central Authority, the following is entered into the permanent record.",
    "This document constitutes the official {civ} account of the events described herein.",
    "Per standard archival protocol, the following events have been verified and catalogued.",
    "The {civ} Historical Registry presents the following certified account.",
    "Classified as Priority-{count} by the {civ} Archival Division.",
};
static constexpr int official_opener_count = 5;

static const char* official_context_colony[] = {
    "Preliminary surveys of {place} confirmed viable conditions for permanent settlement. Resource indices exceeded minimum thresholds by a factor of {count}.",
    "The colonization of {place} was authorized following a {years}-cycle feasibility assessment. Projected population capacity: {count} million.",
    "Strategic analysis identified {place} as critical to the {civ} expansion corridor. Settlement was prioritized accordingly.",
};

static const char* official_context_conflict[] = {
    "Hostilities commenced at {place} following a breakdown in negotiations between the opposing factions. Casualty projections were deemed acceptable.",
    "Intelligence reports indicated a significant military buildup in the {place} system. Preemptive action was authorized by Central Command.",
    "The situation at {place} had deteriorated beyond diplomatic resolution. Military intervention was the only remaining option.",
};

static const char* official_context_discovery[] = {
    "Research team designation Sigma-{count} was deployed to {place} following anomalous sensor readings.",
    "Routine geological survey of {place} revealed structures of non-natural origin at depth level {count}.",
    "The {place} Observatory reported data inconsistent with known physical models. A formal investigation was initiated.",
};

static const char* official_context_construction[] = {
    "The {place} project was approved following a {years}-cycle planning phase. Resource allocation: {count} billion standard units.",
    "Engineering Corps Division {count} was assigned primary responsibility for the {place} construction initiative.",
    "Construction commenced at {place} with a projected completion timeline of {years} standard cycles.",
};

static const char* official_context_collapse[] = {
    "Emergency protocols were enacted across all {civ} territories. The following is the final authorized dispatch.",
    "Communication with {count} systems has been lost. The following is compiled from the last verified transmissions.",
    "This record constitutes the final entry in the {civ} Historical Registry. All subsequent archives are unverified.",
};

static const char* official_actions[] = {
    "Operations proceeded according to established parameters. Resistance was encountered but contained within acceptable margins.",
    "The operation was completed within the projected timeline. All primary objectives were achieved.",
    "{count} personnel were deployed. Losses were within acceptable parameters.",
    "The situation was resolved through decisive application of {civ} protocol. Full compliance was restored.",
    "Analysis confirmed initial assessments. Recommendations for follow-up action were submitted to Central Authority.",
    "Resources were allocated and deployed. The initiative achieved {count}% of projected outcomes.",
};

static const char* official_outcomes[] = {
    "The {place} operation has been classified as a success by the {civ} Central Authority.",
    "All objectives were met. The {place} initiative is hereby closed and archived.",
    "Subsequent monitoring confirmed the stability of outcomes. No further action is required.",
    "The events at {place} have been incorporated into standard {civ} training protocols.",
    "This matter is considered resolved. Further inquiries should be directed to the Archival Division.",
};

// ── PERSONAL style fragments ──────────────────────────────────────────────

static const char* personal_openers[] = {
    "I was there. I need someone to know what actually happened.",
    "I've carried this with me for longer than I should have. It's time to put it down.",
    "If you're reading this, then the vault held. Good. Someone should know the truth.",
    "I don't know who will find this. I don't know if it matters anymore. But I remember.",
    "They'll tell you it happened differently. They'll tell you it was clean, and orderly, and necessary. They're lying.",
    "I keep dreaming about {place}. Every night, the same dream. So I'm writing it down, all of it.",
};
static constexpr int personal_opener_count = 6;

static const char* personal_context_colony[] = {
    "We arrived at {place} after {years} cycles in cryo. The planet was nothing like the surveys promised — it was better. Green valleys stretching to the horizon, air that tasted like rain.",
    "The generation ship shuddered as we entered orbit around {place}. {count} of us pressed against the viewports. Half were crying. The other half were praying.",
    "They told us {place} was our new home. They didn't tell us about the things already living there.",
};

static const char* personal_context_conflict[] = {
    "We'd heard the rumors for weeks — tensions at {place}, ships being repositioned, comm channels going dark. Nobody wanted to believe it would actually come to this.",
    "I was stationed on the outer ring when the first shots hit. The whole station shook — not like an earthquake, but like something alive flinching.",
    "My unit was deployed to {place} three days before it all fell apart. We thought we were there for a routine patrol.",
};

static const char* personal_context_discovery[] = {
    "The dig at {place} had been going on for months with nothing to show for it. Then Kira's drill broke through into the chamber below, and everything changed.",
    "I wasn't supposed to be on the expedition to {place}. My clearance didn't cover xenoarchaeology. But they needed bodies, and I needed credits.",
    "The sensors had been showing anomalous readings from {place} for weeks. We all assumed it was instrument error. We were wrong.",
};

static const char* personal_context_construction[] = {
    "They said the {place} project would take {years} cycles. It took three times that, and cost more than anyone will ever admit.",
    "I worked on the {place} ring for eleven cycles. Watched it grow from scaffolding to something that blocked out the stars.",
    "Building at {place} was unlike anything I'd experienced. The scale of it — you couldn't hold it in your mind all at once.",
};

static const char* personal_context_collapse[] = {
    "The evacuation order came at third watch. By then, half the comm network was already dark.",
    "I watched the last transport leave {place} from the observation deck. They said there would be another. There wasn't.",
    "When it started to fall apart, it happened faster than anyone predicted. One day we were a civilization. The next, we were survivors.",
};

static const char* personal_actions_conflict[] = {
    "I watched through the viewport as two cruisers broke apart in silence, their wreckage spinning toward the planet below. There were escape pods. Hundreds of them, tumbling like seeds.",
    "The fighting lasted for {years} cycles. By the end, nobody could remember what we were fighting for. We just knew how to fight.",
    "They called it a battle. It wasn't. It was a slaughter, and we were on the wrong side of it.",
    "We held {place} for {count} days. On the last day, the commander told us help was coming. Her voice cracked when she said it.",
};

static const char* personal_actions_discovery[] = {
    "The light that poured out of the vault was not light at all — it was something older, something that remembered being looked at. Three of us went in. I am the only one who came back.",
    "When we translated the first inscription, the room went quiet. Not because of what it said, but because of what it implied — they knew. They knew about the center, about what waits there.",
    "I touched the artifact. Just for a moment. And in that moment, I saw the galaxy as they saw it — not as points of light, but as a map. A map with a destination.",
    "The data crystal hummed when I picked it up. The readings on my scanner went haywire — temperature, radiation, temporal displacement. All off the charts.",
};

static const char* personal_actions_general[] = {
    "It changed everything. Not all at once — slowly, like a crack spreading through ice. By the time we understood what had happened, it was too late to go back.",
    "I can still see it when I close my eyes. I don't think I'll ever stop seeing it.",
    "We did what we thought was right. Looking back, I'm not sure that matters.",
    "The thing nobody tells you is how quiet it was. You'd think something that important would be loud.",
};

static const char* personal_outcomes[] = {
    "I keep telling myself it wasn't my fault. Some nights, I almost believe it.",
    "I left {place} three cycles later. I haven't been back. I won't go back.",
    "They gave me a commendation. I threw it out the airlock the next morning.",
    "If you find this, know that we tried. We tried so hard. It wasn't enough.",
    "I'm writing this from a ship with no destination. Wherever I end up, I hope it's far from what I left behind.",
    "I don't know if any of this matters anymore. But the truth has to live somewhere, even if it's just in these words.",
};

// ── LEGEND style fragments ────────────────────────────────────────────────

static const char* legend_openers[] = {
    "In the age before silence, when {civ} still walked among the stars, there came a moment that would echo through eternity.",
    "Listen well, for this is the tale of {place}, and what was found there in the deep time.",
    "The old songs speak of an age when {civ} reached further than any who came before — and paid the price.",
    "Before the void reclaimed what {civ} had built, before the ruins grew cold, there was a time of wonders.",
    "They say the stars themselves dimmed when it happened. Whether that is truth or poetry, the story remains.",
    "This is the song of {civ}, as it has been passed from memory to memory across the long silence.",
};
static constexpr int legend_opener_count = 6;

static const char* legend_bodies_conflict[] = {
    "Star fought star above the world called {place}, and the sky burned for {count} days. When the fires died, the wreckage fell like a crown of thorns upon the planet below. They say the craters still glow at night.",
    "Two fleets met at {place}, and neither would yield. The battle lasted until the ammunition was spent, and then they fought with the hulls of their broken ships. When dawn came to that system, there was nothing left but dust and silence.",
    "The war at {place} was not fought with weapons alone. It was fought with ideas, with betrayals, with the slow poison of distrust that turns allies into enemies. By the time the last shot was fired, no one remembered who had fired the first.",
};

static const char* legend_bodies_discovery[] = {
    "Deep beneath the ice of {place}, they found what the ancients had left behind — not a tomb, but a message. A message written in mathematics, in geometry, in the very structure of matter. It said: you are not the first, and you will not be the last.",
    "When the vault at {place} opened, light poured forth — not the light of stars, but something older. Something that had been waiting. Those who entered emerged changed, speaking of a convergence at the center of all things.",
    "The artifact found at {place} was small enough to hold in one hand. But its weight — not physical weight, but the weight of what it contained — was enough to bend the arc of {civ} history forever.",
};

static const char* legend_bodies_construction[] = {
    "They built it to last forever. The great structure at {place} rose like a prayer made solid, reaching toward the stars that had always called to {civ}. Even now, in the long silence, it endures — a monument to what was, and what might have been.",
    "It took {count} generations to complete. Entire lifetimes were spent in its construction, whole families born and dying in its shadow. When it was finished, those who stood beneath it wept, for they had built something greater than themselves.",
};

static const char* legend_bodies_collapse[] = {
    "And then the silence came. Not with thunder, not with fire, but with a whisper — a slow exhalation, as if {civ} had simply grown tired of being. One by one, the lights went out. The great works fell quiet. The void reclaimed what had always been its own.",
    "In the end, {civ} did not fall — they chose to leave. Whether they ascended or simply ceased, no one who remained could say. They left behind their cities, their machines, their songs carved in crystal. They left behind everything except answers.",
    "The last of {civ} gathered at {place} as the end drew near. What they did there, in those final hours, is known only to the ruins. But the walls still hum with something — not power, not memory, but something between the two.",
};

static const char* legend_bodies_general[] = {
    "What happened at {place} became a legend before the echoes faded. In time, the legend grew larger than the truth, and the truth was extraordinary enough.",
    "The {civ} who witnessed these events tried to record what they saw, but language failed them. Some things exist beyond the reach of words, and this was one of them.",
    "This much is known: after the events at {place}, nothing was ever quite the same. The stars looked different. The void felt closer. And somewhere, deep in the dark between galaxies, something ancient took notice.",
};

// ── SCIENTIFIC style fragments ────────────────────────────────────────────

static const char* scientific_openers[] = {
    "Research Station {place}, Lab Report {count}: the following observations are classified Priority Alpha.",
    "Xenoarchaeology Division, Field Report {count}. Site: {place}. Classification: Anomalous.",
    "The following data was collected during Survey Mission {count} to the {place} system.",
    "Preliminary analysis complete. Findings exceed initial projections by several orders of magnitude.",
    "Note: the following observations have been independently verified by {count} separate research teams.",
};
static constexpr int scientific_opener_count = 5;

static const char* scientific_bodies[] = {
    "Spectral analysis confirms {arch} structural composition consistent with {civ} construction methods. Carbon dating places the site at approximately {count} megacycles before present. Structural integrity: {years}%.",
    "Energy output from the artifact exceeds input by a factor of 10^{count}. Temporal displacement detected at the molecular level. The mechanism operates on principles inconsistent with known {tech} physics.",
    "Biological samples recovered from {place} exhibit cellular structures unlike anything in the {civ} genetic database. Silicon-carbon hybrid biochemistry. Possibly engineered. Recommend containment protocol Alpha.",
    "Gravitational anomalies at {place} correlate with predicted {pred} beacon signatures. Signal periodicity: {count} standard cycles. Directional analysis confirms origin vector toward galactic center.",
    "Atmospheric reconstruction of {place} indicates deliberate terraforming {count} megacycles ago. Chemical signatures match {civ} industrial processes. The entire biosphere appears to be an artifact.",
    "Seismic mapping reveals sub-surface structures extending to a depth of {count} kilometers. Architecture is consistent with {civ} design principles but predates all known {civ} construction by {years} megacycles.",
    "The device activates in the presence of biological neural patterns. It appears to function as an information transfer mechanism, though the information itself resists quantification. Three researchers report vivid perceptual experiences during exposure.",
};
static constexpr int scientific_body_count = 7;

static const char* scientific_outcomes[] = {
    "Recommend immediate containment and further study under controlled conditions.",
    "Further research is warranted. Requesting additional funding and personnel allocation.",
    "Findings submitted to the {civ} Academy of Sciences for peer review. Initial response: skepticism.",
    "NOTE: Research team Sigma-{count} has not reported in for {years} cycles. Status: unknown.",
    "Conclusion: the implications of these findings extend beyond our current theoretical framework. A paradigm shift may be required.",
    "All data has been archived under encryption key {civ}-{count}-ALPHA. Access restricted to Level 7 and above.",
};

// ── TRANSMISSION style fragments ──────────────────────────────────────────

static const char* transmission_openers[] = {
    "...signal degrading... emergency broadcast on all frequencies...",
    "This is {place} station to any {civ} vessel in range... we need immediate assistance...",
    "...partial signal recovered from debris field near {place}... origin unknown... timestamp corrupted...",
    "PRIORITY ALPHA — all {civ} channels — do not approach {place} system — repeat — DO NOT APPROACH...",
    "...automated distress beacon... no life signs detected... recording begins...",
    "If anyone receives this... please... we're still here... coordinates follow...",
};
static constexpr int transmission_opener_count = 6;

static const char* transmission_bodies[] = {
    "...they're inside the station... level {count} is breached... we sealed the bulkheads but they're cutting through...",
    "...the artifact activated on its own... readings are off every scale we have... the walls are... I don't know how to describe what's happening to the walls...",
    "...{count} survivors out of twelve hundred... escape pods launched but the nav systems are scrambled... we're drifting...",
    "...it wasn't an attack... it came from inside... from below the surface... the whole colony is...[static]...can't get a clear signal...",
    "...they told us {place} was safe... they told us the {pred} ruins were dormant... they were wrong about that... they were wrong about everything...",
    "...engines failing... hull breach on decks {count} through {count}... if you find this transmission know that the crew of the {civ} vessel fought until the end...",
    "...don't trust the official reports... what happened at {place} wasn't what they're saying... I have the real data... coordinates encrypted in this signal...",
    "...the beacon lit up... it's broadcasting toward the center... we can't shut it off... and something... something is responding...",
};
static constexpr int transmission_body_count = 8;

static const char* transmission_endings[] = {
    "...[SIGNAL LOST]",
    "...[END OF TRANSMISSION]",
    "...[RECORDING CORRUPTED — 73% DATA LOSS]",
    "...tell them we—[SIGNAL LOST]",
    "...[AUTOMATED LOOP — NO FURTHER CONTENT]",
    "...coordinates follow: [DATA CORRUPTED]...[END]",
};

// ── Mystery suffixes (30% chance, any style) ──────────────────────────────

static const char* mystery_suffixes[] = {
    "What happened next has never been satisfactorily explained.",
    "No subsequent expedition has been able to locate the site.",
    "The survivors refused to speak of what they saw. All of them, without exception.",
    "Decades later, a signal was detected from the same coordinates. It matched no known {civ} frequency.",
    "The official record was sealed shortly after. The reasons given were classified.",
    "To this day, the question remains: was it an ending, or a beginning?",
    "Some say they're still out there. In the dark between stars. Waiting.",
    "The last entry in the log is a single word, repeated: convergence.",
};
static constexpr int mystery_count = 8;

// ── Composition ───────────────────────────────────────────────────────────

RecordStyle NarrativeGenerator::pick_style(std::mt19937& rng, LoreEventType type) {
    int cat = event_category(type);
    // Weight styles by event category
    switch (cat) {
    case 1: // conflict
        switch (pick(rng, 5)) {
            case 0: return RecordStyle::Official;
            case 1: case 2: return RecordStyle::Personal;
            case 3: return RecordStyle::Legend;
            default: return RecordStyle::Transmission;
        }
    case 2: // discovery
        switch (pick(rng, 5)) {
            case 0: case 1: return RecordStyle::Scientific;
            case 2: return RecordStyle::Personal;
            case 3: return RecordStyle::Legend;
            default: return RecordStyle::Official;
        }
    case 3: // construction
        switch (pick(rng, 4)) {
            case 0: return RecordStyle::Official;
            case 1: return RecordStyle::Personal;
            case 2: return RecordStyle::Legend;
            default: return RecordStyle::Scientific;
        }
    case 4: // collapse
        switch (pick(rng, 5)) {
            case 0: case 1: return RecordStyle::Transmission;
            case 2: return RecordStyle::Personal;
            case 3: return RecordStyle::Legend;
            default: return RecordStyle::Official;
        }
    default:
        return static_cast<RecordStyle>(pick(rng, 5));
    }
}

std::string NarrativeGenerator::generate_title(
    std::mt19937& rng, RecordStyle style,
    const NarrativeContext& ctx, const std::string& subject) {

    std::string title;
    switch (style) {
    case RecordStyle::Official:
        switch (pick(rng, 4)) {
            case 0: title = "Registry Entry: {place}"; break;
            case 1: title = "Dispatch {count}-Alpha: {place}"; break;
            case 2: title = "Official Account of the {place} Incident"; break;
            default: title = "{civ} Archival Record {count}"; break;
        }
        break;
    case RecordStyle::Personal:
        switch (pick(rng, 5)) {
            case 0: title = "What I Saw at {place}"; break;
            case 1: title = "Letter from {place}"; break;
            case 2: title = "The Day Everything Changed"; break;
            case 3: title = "Personal Log — {place}"; break;
            default: title = "I Remember {place}"; break;
        }
        break;
    case RecordStyle::Legend:
        switch (pick(rng, 5)) {
            case 0: title = "The Song of {place}"; break;
            case 1: title = "When {civ} Touched the Void"; break;
            case 2: title = "The Tale of {place}"; break;
            case 3: title = "Echoes of {place}"; break;
            default: title = "The {civ} and the Silence"; break;
        }
        break;
    case RecordStyle::Scientific:
        switch (pick(rng, 4)) {
            case 0: title = "Research Note: {place} Anomaly"; break;
            case 1: title = "Analysis of {place} Site"; break;
            case 2: title = "Lab Report {count}: {place}"; break;
            default: title = "Field Survey: {place} System"; break;
        }
        break;
    case RecordStyle::Transmission:
        switch (pick(rng, 4)) {
            case 0: title = "Transmission {count}-Kappa"; break;
            case 1: title = "Emergency Broadcast from {place}"; break;
            case 2: title = "Signal Fragment {count}"; break;
            default: title = "Last Transmission — {place}"; break;
        }
        break;
    }
    return fill_all(title, ctx);
}

std::string NarrativeGenerator::generate_source(
    std::mt19937& rng, RecordStyle style, const NarrativeContext& ctx) {

    std::string src;
    switch (style) {
    case RecordStyle::Official:
        switch (pick(rng, 3)) {
            case 0: src = "{civ} Archival Authority"; break;
            case 1: src = "{civ} Central Command"; break;
            default: src = "{civ} Historical Registry"; break;
        }
        break;
    case RecordStyle::Personal:
        switch (pick(rng, 4)) {
            case 0: src = "Recovered journal, author unknown"; break;
            case 1: src = "Personal log of {figure_name}"; break;
            case 2: src = "Unsigned letter, found at {place}"; break;
            default: src = "Anonymous {civ} survivor"; break;
        }
        break;
    case RecordStyle::Legend:
        switch (pick(rng, 3)) {
            case 0: src = "Oral tradition, transcribed"; break;
            case 1: src = "{civ} creation myth, fragment"; break;
            default: src = "The Song of {figure_name}"; break;
        }
        break;
    case RecordStyle::Scientific:
        switch (pick(rng, 3)) {
            case 0: src = "{place} Research Station"; break;
            case 1: src = "{civ} Xenoarchaeology Division"; break;
            default: src = "{civ} Academy of Sciences"; break;
        }
        break;
    case RecordStyle::Transmission:
        switch (pick(rng, 3)) {
            case 0: src = "Intercepted signal, origin: {place}"; break;
            case 1: src = "Automated distress beacon"; break;
            default: src = "Fragment recovered from debris field"; break;
        }
        break;
    }
    return fill_all(src, ctx);
}

std::string NarrativeGenerator::compose_body(
    std::mt19937& rng, RecordStyle style, LoreEventType type,
    const NarrativeContext& ctx, const std::string& event_desc,
    int length_class) {

    int cat = event_category(type);
    std::string body;

    auto append = [&](const std::string& s) {
        if (!body.empty() && body.back() != ' ' && body.back() != '\n')
            body += " ";
        body += s;
    };

    switch (style) {
    case RecordStyle::Official: {
        append(fill_all(official_openers[pick(rng, official_opener_count)], ctx));
        // Context
        const char** ctx_pool;
        int ctx_count;
        switch (cat) {
            case 0: ctx_pool = official_context_colony; ctx_count = 3; break;
            case 1: ctx_pool = official_context_conflict; ctx_count = 3; break;
            case 2: ctx_pool = official_context_discovery; ctx_count = 3; break;
            case 3: ctx_pool = official_context_construction; ctx_count = 3; break;
            case 4: ctx_pool = official_context_collapse; ctx_count = 3; break;
            default: ctx_pool = official_context_colony; ctx_count = 3; break;
        }
        append(fill_all(ctx_pool[pick(rng, ctx_count)], ctx));
        if (length_class >= 1) {
            append(fill_all(official_actions[pick(rng, 6)], ctx));
        }
        if (length_class >= 1) {
            append(fill_all(official_outcomes[pick(rng, 5)], ctx));
        }
        if (length_class >= 2) {
            append(fill_all(official_actions[pick(rng, 6)], ctx));
        }
        break;
    }
    case RecordStyle::Personal: {
        append(fill_all(personal_openers[pick(rng, personal_opener_count)], ctx));
        // Context
        const char** ctx_pool;
        int ctx_count;
        switch (cat) {
            case 0: ctx_pool = personal_context_colony; ctx_count = 3; break;
            case 1: ctx_pool = personal_context_conflict; ctx_count = 3; break;
            case 2: ctx_pool = personal_context_discovery; ctx_count = 3; break;
            case 3: ctx_pool = personal_context_construction; ctx_count = 3; break;
            case 4: ctx_pool = personal_context_collapse; ctx_count = 3; break;
            default: ctx_pool = personal_context_colony; ctx_count = 3; break;
        }
        append(fill_all(ctx_pool[pick(rng, ctx_count)], ctx));
        // Actions
        if (cat == 1) {
            append(fill_all(personal_actions_conflict[pick(rng, 4)], ctx));
        } else if (cat == 2) {
            append(fill_all(personal_actions_discovery[pick(rng, 4)], ctx));
        } else {
            append(fill_all(personal_actions_general[pick(rng, 4)], ctx));
        }
        if (length_class >= 1) {
            append(fill_all(personal_actions_general[pick(rng, 4)], ctx));
        }
        // Outcome
        append(fill_all(personal_outcomes[pick(rng, 6)], ctx));
        if (length_class >= 2) {
            append(fill_all(personal_actions_general[pick(rng, 4)], ctx));
            append(fill_all(personal_outcomes[pick(rng, 6)], ctx));
        }
        break;
    }
    case RecordStyle::Legend: {
        append(fill_all(legend_openers[pick(rng, legend_opener_count)], ctx));
        // Body
        const char** body_pool;
        int body_count;
        switch (cat) {
            case 1: body_pool = legend_bodies_conflict; body_count = 3; break;
            case 2: body_pool = legend_bodies_discovery; body_count = 3; break;
            case 3: body_pool = legend_bodies_construction; body_count = 2; break;
            case 4: body_pool = legend_bodies_collapse; body_count = 3; break;
            default: body_pool = legend_bodies_general; body_count = 3; break;
        }
        append(fill_all(body_pool[pick(rng, body_count)], ctx));
        if (length_class >= 1) {
            append(fill_all(legend_bodies_general[pick(rng, 3)], ctx));
        }
        if (length_class >= 2) {
            append(fill_all(legend_bodies_general[pick(rng, 3)], ctx));
        }
        break;
    }
    case RecordStyle::Scientific: {
        append(fill_all(scientific_openers[pick(rng, scientific_opener_count)], ctx));
        append(fill_all(scientific_bodies[pick(rng, scientific_body_count)], ctx));
        if (length_class >= 1) {
            append(fill_all(scientific_bodies[pick(rng, scientific_body_count)], ctx));
        }
        append(fill_all(scientific_outcomes[pick(rng, 6)], ctx));
        if (length_class >= 2) {
            append(fill_all(scientific_bodies[pick(rng, scientific_body_count)], ctx));
        }
        break;
    }
    case RecordStyle::Transmission: {
        append(fill_all(transmission_openers[pick(rng, transmission_opener_count)], ctx));
        int msg_count = 1 + length_class;
        for (int i = 0; i < msg_count; ++i) {
            append(fill_all(transmission_bodies[pick(rng, transmission_body_count)], ctx));
        }
        append(fill_all(transmission_endings[pick(rng, 6)], ctx));
        break;
    }
    }

    // 30% chance of mystery suffix
    if (pick(rng, 10) < 3) {
        append(fill_all(mystery_suffixes[pick(rng, mystery_count)], ctx));
    }

    return body;
}

// ── Record generation ─────────────────────────────────────────────────────

LoreRecord NarrativeGenerator::generate_event_record(
    std::mt19937& rng, const NarrativeContext& base_ctx,
    const LoreEvent& event, int event_index, RecordStyle style) {

    // Build context with event-specific data
    NarrativeContext ctx = base_ctx;
    ctx.count = 1 + pick(rng, 200);
    ctx.years = 1 + pick(rng, 500);

    // Pick record length: 30% short, 50% medium, 20% long
    int len_roll = pick(rng, 10);
    int length_class = (len_roll < 3) ? 0 : (len_roll < 8) ? 1 : 2;

    LoreRecord rec;
    rec.style = style;
    rec.event_index = event_index;
    rec.system_id = event.system_id;

    // Reliability based on style
    switch (style) {
        case RecordStyle::Official:
            rec.reliability = RecordReliability::Verified;
            break;
        case RecordStyle::Personal:
            rec.reliability = (pick(rng, 3) == 0) ? RecordReliability::Disputed : RecordReliability::Verified;
            break;
        case RecordStyle::Legend:
            rec.reliability = RecordReliability::Myth;
            break;
        case RecordStyle::Scientific:
            rec.reliability = RecordReliability::Verified;
            break;
        case RecordStyle::Transmission:
            rec.reliability = RecordReliability::Disputed;
            break;
    }

    std::string subject = event.description;
    rec.title = generate_title(rng, style, ctx, subject);
    rec.source = generate_source(rng, style, ctx);
    rec.body = compose_body(rng, style, event.type, ctx, event.description, length_class);

    return rec;
}

LoreRecord NarrativeGenerator::generate_figure_record(
    std::mt19937& rng, const NarrativeContext& base_ctx,
    const KeyFigure& figure, int figure_index,
    const Civilization& civ) {

    NarrativeContext ctx = base_ctx;
    ctx.figure = figure.name + " " + figure.title;
    ctx.figure_name = figure.name;
    ctx.count = 1 + pick(rng, 100);
    ctx.years = 1 + pick(rng, 300);

    // Figures are usually legends or personal accounts
    RecordStyle style = (pick(rng, 3) == 0) ? RecordStyle::Personal : RecordStyle::Legend;
    int length_class = (pick(rng, 3) == 0) ? 2 : 1; // Figures get longer records

    LoreRecord rec;
    rec.style = style;
    rec.figure_index = figure_index;
    rec.system_id = figure.system_id;
    rec.reliability = (style == RecordStyle::Legend) ? RecordReliability::Myth : RecordReliability::Disputed;

    // Title
    switch (pick(rng, 4)) {
        case 0: rec.title = "The Song of " + figure.name; break;
        case 1: rec.title = figure.name + " " + figure.title + " — A Chronicle"; break;
        case 2: rec.title = "Remembering " + figure.name; break;
        default: rec.title = "The Legend of " + figure.name + " " + figure.title; break;
    }
    rec.source = generate_source(rng, style, ctx);

    // Build body about the figure
    std::string body;
    if (style == RecordStyle::Legend) {
        body = fill_all(legend_openers[pick(rng, legend_opener_count)], ctx);
        body += " " + figure.name + " " + figure.title + " — " + figure.achievement + ".";
        body += " " + fill_all(legend_bodies_general[pick(rng, 3)], ctx);
        body += " In the end, " + figure.name + "'s fate was this: " + figure.fate + ".";
    } else {
        body = fill_all(personal_openers[pick(rng, personal_opener_count)], ctx);
        body += " I served under " + figure.name + " " + figure.title + " for " +
                std::to_string(ctx.years) + " cycles. " + figure.achievement + ".";
        body += " " + fill_all(personal_actions_general[pick(rng, 4)], ctx);
        body += " " + figure.name + "'s fate: " + figure.fate + ".";
        body += " " + fill_all(personal_outcomes[pick(rng, 6)], ctx);
    }
    rec.body = body;
    return rec;
}

LoreRecord NarrativeGenerator::generate_artifact_record(
    std::mt19937& rng, const NarrativeContext& base_ctx,
    const LoreArtifact& artifact, int artifact_index,
    const Civilization& civ) {

    NarrativeContext ctx = base_ctx;
    ctx.artifact = artifact.name;
    ctx.count = 1 + pick(rng, 50);

    // Artifacts are usually scientific or legend
    RecordStyle style = (pick(rng, 3) == 0) ? RecordStyle::Legend : RecordStyle::Scientific;

    LoreRecord rec;
    rec.style = style;
    rec.artifact_index = artifact_index;
    rec.system_id = artifact.system_id;
    rec.reliability = (style == RecordStyle::Legend) ? RecordReliability::Myth : RecordReliability::Verified;

    // Title
    switch (pick(rng, 3)) {
        case 0: rec.title = "Analysis of " + artifact.name; break;
        case 1: rec.title = "The " + artifact.name + " — Origins"; break;
        default: rec.title = "On the Nature of " + artifact.name; break;
    }
    rec.source = generate_source(rng, style, ctx);

    // Body
    std::string body;
    if (style == RecordStyle::Scientific) {
        body = fill_all(scientific_openers[pick(rng, scientific_opener_count)], ctx);
        body += " " + artifact.origin_text;
        body += " " + fill_all(scientific_bodies[pick(rng, scientific_body_count)], ctx);
        body += " Documented effect: " + artifact.effect_text + ".";
        body += " " + fill_all(scientific_outcomes[pick(rng, 6)], ctx);
    } else {
        body = fill_all(legend_openers[pick(rng, legend_opener_count)], ctx);
        body += " " + artifact.origin_text;
        body += " " + fill_all(legend_bodies_general[pick(rng, 3)], ctx);
        body += " It is said the " + artifact.name + " holds this power: " + artifact.effect_text + ".";
    }
    rec.body = body;
    return rec;
}

std::pair<LoreRecord, LoreRecord> NarrativeGenerator::generate_contradiction(
    std::mt19937& rng, const NarrativeContext& base_ctx,
    const LoreEvent& event, int event_index) {

    NarrativeContext ctx = base_ctx;
    ctx.count = 1 + pick(rng, 200);
    ctx.years = 1 + pick(rng, 500);

    // Victor's version — official
    LoreRecord victor = generate_event_record(rng, ctx, event, event_index, RecordStyle::Official);
    victor.reliability = RecordReliability::Propaganda;

    // Counter-narrative — personal or transmission
    RecordStyle counter_style = (pick(rng, 2) == 0) ? RecordStyle::Personal : RecordStyle::Transmission;
    LoreRecord counter = generate_event_record(rng, ctx, event, event_index, counter_style);
    counter.reliability = RecordReliability::Disputed;
    counter.title = "The Other Side — " + counter.title;

    return {victor, counter};
}

// ── Main generation ───────────────────────────────────────────────────────

void NarrativeGenerator::generate(
    std::mt19937& rng, Civilization& civ,
    const NameGenerator& namer,
    const std::vector<Civilization>& predecessors) {

    civ.records.clear();

    // Build base context
    NarrativeContext ctx;
    ctx.civ = civ.short_name;
    ctx.civ_full = civ.name;
    ctx.pred = predecessors.empty() ? "precursor" : predecessors.back().short_name;
    // Name lookups - reuse the static functions from lore_generator
    static const char* arch_names[] = {"crystalline", "organic", "geometric", "void-carved", "light-woven"};
    static const char* tech_names[] = {"gravitational", "bio-mechanical", "quantum-lattice", "harmonic-resonance", "phase-shifting"};
    static const char* phil_names[] = {"expansionist", "contemplative", "predatory", "symbiotic", "transcendent"};
    static const char* collapse_names[] = {"war", "transcendence", "plague", "resource exhaustion", "Sgr A* obsession", "unknown"};
    ctx.arch = arch_names[static_cast<int>(civ.architecture)];
    ctx.tech = tech_names[static_cast<int>(civ.tech_style)];
    ctx.phil = phil_names[static_cast<int>(civ.philosophy)];
    ctx.collapse = collapse_names[static_cast<int>(civ.collapse_cause)];

    // Pick a default figure name for source attribution
    ctx.figure_name = civ.figures.empty() ? "Unknown" : civ.figures[0].name;
    ctx.figure = ctx.figure_name;

    // Generate 8-15 event records
    int record_count = 8 + pick(rng, 8);
    // Pick interesting events (skip generic ones)
    std::vector<int> event_indices;
    for (int i = 0; i < static_cast<int>(civ.events.size()); ++i) {
        event_indices.push_back(i);
    }
    // Shuffle and take up to record_count
    for (int i = static_cast<int>(event_indices.size()) - 1; i > 0; --i) {
        int j = pick(rng, i + 1);
        std::swap(event_indices[i], event_indices[j]);
    }
    int event_records = std::min(record_count, static_cast<int>(event_indices.size()));

    for (int i = 0; i < event_records; ++i) {
        int ei = event_indices[i];
        const auto& event = civ.events[ei];

        // Generate place name for this record's context
        ctx.place = namer.place(rng);
        ctx.place2 = namer.place(rng);
        ctx.count = 1 + pick(rng, 200);
        ctx.years = 1 + pick(rng, 500);

        // 15% chance of contradiction
        if (pick(rng, 100) < 15) {
            auto [victor, counter] = generate_contradiction(rng, ctx, event, ei);
            civ.records.push_back(std::move(victor));
            civ.records.push_back(std::move(counter));
        } else {
            RecordStyle style = pick_style(rng, event.type);
            civ.records.push_back(generate_event_record(rng, ctx, event, ei, style));
        }
    }

    // Generate 1-3 figure records
    int fig_records = std::min(1 + pick(rng, 3), static_cast<int>(civ.figures.size()));
    for (int i = 0; i < fig_records; ++i) {
        ctx.place = namer.place(rng);
        civ.records.push_back(generate_figure_record(rng, ctx, civ.figures[i], i, civ));
    }

    // Generate 1 record per artifact
    for (int i = 0; i < static_cast<int>(civ.artifacts.size()); ++i) {
        ctx.place = namer.place(rng);
        civ.records.push_back(generate_artifact_record(rng, ctx, civ.artifacts[i], i, civ));
    }
}

} // namespace astra

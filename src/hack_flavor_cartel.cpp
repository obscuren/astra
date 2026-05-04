// Plan 7 Phase B — Cartel flavor pack.
//
// Voice: profane, personal, busted-but-functional, sticky notes from "mikko."
// Spec §10 Cartel.

#include "astra/hack_flavor.h"

namespace astra {

namespace {

const char* k_motd[] = {
    "if u read this and ur not mikko ur fired",
    "DO NOT UPDATE - last guy bricked it",
    "wifi password: cartel123 dont tell denis",
    "the relay is held together w/ tape. dont touch it. seriously.",
    "if it beeps three times KICK IT. once is fine.",
    "denis owes me 200c. tell him.",
    "tomas left these creds on a sticky note thanks tomas",
    "this device used to be a vending machine. it kinda still is.",
};

const char* k_logs[] = {
    "04/12 8:14a - auth ok mikko frm vending closet",
    "04/12 9:02a - denis tried to login. lol no.",
    "04/12 11:30a - relay reset (sparks again - again)",
    "04/11 11:55p - mikko reboot (third time this week)",
    "04/11 6:14p - tomas patched the patch w/ another patch",
    "04/10 2:00p - somebody plugged a coffee maker into me. WHY",
};

const char* k_users[] = {
    "mikko", "denis", "tomas", "anya", "kasimir", "rec",
    "vladek", "naima", "boris", "olek", "inka", "jakov",
};

const char* k_files[] = {
    "the turret in 4-A is on default creds. dont tell denis.",
    "todo:\n  - fix the SECOND relay (the one held w/ tape)\n  - kick the vending machine\n  - tell denis to STOP",
    "ssh key for the back office is taped under the desk.\nyes literally taped, no it's not encrypted.",
    "if anyone asks the cargo manifest is 'imported synthetic textiles'\nit is not. dont ask.",
    "RECIPE - the good vodka:\n  - hydroponic potatoes (steal from greenhouse)\n  - chems (mikko has the keys)\n  - 14 days. dont touch it.",
    "denis WHERE is the spare battery you had ONE JOB",
    "im keeping a tally of how many times this thing reboots\n  apr 8: 4\n  apr 9: 7\n  apr 10: STOPPED COUNTING",
    "VLADEK if you read this you owe me 4 packs of stims dont play dumb",
    "operational note: dont let kasimir touch the comms panel.\nhe means well but no.",
    "if you find a dead body in 3-C: it was already there. probably.",
};

const char* k_chrome[] = {
    "  %FACTION%/%FIXTURE_NAME% v%VERSION% (modified - do not update)\n"
    "  THIS UNIT BELONGS TO MIKKO. DO NOT FUCK WITH IT.\n"
    "  if u read this and ur not mikko ur fired\n"
    "    -- mikko",
    "###  %FACTION% / %FIXTURE_NAME%  ###\n"
    "###  v%VERSION% (held together w/ tape)  ###\n"
    "###  -- mikko was here  ###",
    " ~~ %FACTION% ~~ %FIXTURE_NAME% ~~ %VERSION% ~~\n"
    " ~~ DO NOT UPDATE seriously ~~ tomas patched the patch ~~",
};

const HackFlavorPack k_cartel{
    "Cartel",
    "ROOT_USER (mikko)",
    std::span<const char* const>(k_motd),
    std::span<const char* const>(k_logs),
    std::span<const char* const>(k_users),
    std::span<const char* const>(k_files),
    std::span<const char* const>(k_chrome),
};

} // namespace

const HackFlavorPack& cartel_flavor_pack() { return k_cartel; }

} // namespace astra

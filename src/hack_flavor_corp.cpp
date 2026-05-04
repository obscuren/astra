// Plan 7 Phase B — Corp flavor pack (Kaguya / Helion / generic Corporate).
//
// Voice: legalistic, sterile, version-numbered, threats of prosecution.
// Spec §10 Corp.

#include "astra/hack_flavor.h"

namespace astra {

namespace {

const char* k_motd[] = {
    "Unauthorized access prosecuted under Sec. 12 of the Combined Charter.",
    "This system is monitored. By accessing it you agree to the EULA.",
    "KHI Asset 0x4477 - do not modify.",
    "All sessions logged. Tampering is grounds for immediate termination.",
    "Property of Kaguya Heavy Industries. Compliance is non-optional.",
    "Compliance Notice: firmware downgrades void the operations warranty.",
    "Authorization Tier 3 required for configuration changes. Lower tiers monitored.",
    "Helion Combined Systems: Reliability through Compliance.",
};

const char* k_logs[] = {
    "2476-04-12.08:14:33 INFO auth.success user=admin from=10.0.4.1",
    "2476-04-12.08:14:50 INFO config.read user=svc-monitor from=10.0.4.2",
    "2476-04-12.09:02:11 WARN telemetry.dropped reason=link-flap",
    "2476-04-12.09:11:03 INFO firmware.verify checksum=ok",
    "2476-04-12.10:00:00 INFO scheduled.audit pass=12 result=clean",
    "2476-04-11.23:58:44 ERR  auth.fail user=root from=10.0.4.99 attempt=3",
};

const char* k_users[] = {
    "admin", "svc-monitor", "svc-relay", "tech-3", "tech-4",
    "audit", "compliance", "ops-lead", "shift-a", "shift-b",
    "khi-corporate", "helion-relay",
};

const char* k_files[] = {
    "Compliance Note: All firmware updates must be cleared via Helion ticket.\n"
    "Failure to comply is grounds for termination, see Charter Sec. 41.",
    "ROUTINE OPS LOG\n  - Daily checksum verified.\n  - Heat ceiling within nominal.\n  - Review Sec. 4(b) for handover.",
    "MEMO: All site personnel must badge in via the south entrance.\n"
    "Repeated badge-out without re-badge counts as an event of record.",
    "Sec. 12, Subsec. (c): Unauthorized intrusion into KHI assets is\n"
    "prosecuted under the Combined Charter; minimum sentence eight years.",
    "INVENTORY DELTA: 4x .50cal box magazine assembly transferred from\n"
    "warehouse C to range B. Approval: ops-lead. Reference: REQ-44871.",
    "Helion Standard Practice: rotate authorization tokens monthly.\n"
    "Last rotation: 2476-03-14. Next scheduled: 2476-04-14.",
    "Override request denied. Resubmit through proper channels with\n"
    "approval from a designated Tier-3 representative.",
    "ATTENTION: Visitor badges expire at 18:00 SLT. Escort required.",
    "Audit Note: Site B compliance score 0.94 (within tolerance).",
    "Disciplinary file 22-G: see HR. Subject: tech-3 (warning issued).",
};

const char* k_chrome[] = {
    "╔══════════════════════════════════════════════╗\n"
    "║  %FACTION% — %FIXTURE_NAME% %VERSION%\n"
    "║  Property of Kaguya Heavy Industries.\n"
    "║  Unauthorized access prosecuted under\n"
    "║  Sec. 12 of the Combined Charter.\n"
    "╚══════════════════════════════════════════════╝",
    "[ KHI / %FACTION% ] :: %FIXTURE_NAME% rev %VERSION%\n"
    "  Authorized personnel only.  Sessions logged.",
    "── %FACTION% / %FIXTURE_NAME% %VERSION% ──\n"
    "── All access is monitored. Compliance is non-optional. ──",
};

const HackFlavorPack k_corp{
    "Kaguya Heavy Industries",
    "root",
    std::span<const char* const>(k_motd),
    std::span<const char* const>(k_logs),
    std::span<const char* const>(k_users),
    std::span<const char* const>(k_files),
    std::span<const char* const>(k_chrome),
};

} // namespace

const HackFlavorPack& corp_flavor_pack() { return k_corp; }

} // namespace astra

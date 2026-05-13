#include "astra/grammars/gen_camera_netspace.h"

#include "astra/net_room.h"
#include "astra/net_theme.h"
#include "astra/netspace_layout.h"

#include <cstdio>
#include <string>

namespace astra {

namespace {

// Layout — five 5w × 3h lens rooms in a horizontal row, draining into a
// 9w × 4h FEED room which fans right to ARCHIVE (11w × 4h) and down to
// DVR (9w × 3h). Numbers are chosen so connect()'s L-routing produces
// clean drops; see commentary inline.
constexpr int kLensW       = 5;
constexpr int kLensH       = 3;
constexpr int kLensGap     = 3;     // empty cells between adjacent lens borders → pipe lives here
constexpr int kLensCount   = 5;

constexpr int kFeedW       = 9;
constexpr int kFeedH       = 4;
constexpr int kArchiveW    = 11;
constexpr int kArchiveH    = 4;
constexpr int kFeedArcGap  = 3;     // horizontal pipe gap between FEED and ARCHIVE
constexpr int kDvrW        = 9;
constexpr int kDvrH        = 3;

constexpr int kMargX       = 3;
constexpr int kLensY       = 2;
constexpr int kLensToFeed  = 4;     // rows of clearance under the lens row
constexpr int kFeedToDvr   = 2;     // empty rows between FEED bottom and DVR top
constexpr int kMargBot     = 2;

// "CAM_NN" id from the seed — two decimal digits per the design sample.
std::string camera_id(uint32_t seed) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "CAM_%02d", static_cast<int>(seed % 100));
    return buf;
}

}  // namespace

Netspace gen_camera_netspace(const TargetDescriptor& desc) {
    // Width: lens row dominates. Lens row spans
    //   kLensCount × kLensW + (kLensCount - 1) × kLensGap
    // FEED is centered under lens 3 (the middle lens), ARCHIVE sits to
    // FEED's right and must fit inside the same playfield.
    const int lens_row_w = kLensCount * kLensW + (kLensCount - 1) * kLensGap;
    const int lens_x0    = kMargX;

    // Middle-lens centre column → FEED's centre column. lens_i x = lens_x0 + i * (kLensW + kLensGap).
    const int mid_lens_cx = lens_x0 + 2 * (kLensW + kLensGap) + kLensW / 2;
    int feed_x = mid_lens_cx - kFeedW / 2;
    if (feed_x < kMargX) feed_x = kMargX;
    const int feed_y = kLensY + kLensH + kLensToFeed;

    const int archive_x = feed_x + kFeedW + kFeedArcGap;
    const int archive_y = feed_y;

    const int dvr_x = feed_x + (kFeedW - kDvrW) / 2;
    const int dvr_y = feed_y + kFeedH + kFeedToDvr;

    const int right_edge = archive_x + kArchiveW;
    const int left_edge  = lens_x0 + lens_row_w;
    const int max_edge   = right_edge > left_edge ? right_edge : left_edge;
    const int width      = max_edge + kMargX;
    const int height     = dvr_y + kDvrH + kMargBot;

    NetspaceBuilder b(width, height, NetTile::Void);
    b.set_target(desc);

    char title_buf[64];
    std::snprintf(title_buf, sizeof title_buf,
                  "OPTIC ARRAY :: %s :: WATCHING",
                  camera_id(desc.seed).c_str());
    b.set_title(title_buf);

    // Reserve up-front: connect() takes NetRoom references and
    // ns.rooms is a vector — any reallocation invalidates them.
    b.ns.rooms.reserve(static_cast<size_t>(kLensCount + 3));

    // ── LENS BANK ──────────────────────────────────────────────────
    // Five 5w × 3h boxes in a row. Each shows "(o)" on the single
    // interior row (h=3 collapses to label only).
    NetRoom* lenses[kLensCount] = {nullptr};
    for (int i = 0; i < kLensCount; ++i) {
        const int lx = lens_x0 + i * (kLensW + kLensGap);
        NetRoom& lens = b.add_room(lx, kLensY, kLensW, kLensH, "(o)",
                                   NetRoom::Border::Thin);
        lens.label_color = net_theme::box_thin_color;
        lenses[i] = &lens;
    }

    // ── FEED ───────────────────────────────────────────────────────
    NetRoom& feed = b.add_room(feed_x, feed_y, kFeedW, kFeedH, "FEED",
                               NetRoom::Border::Thin);
    feed.label_color    = net_theme::box_thin_color;
    feed.bottom_color   = net_theme::avatar;
    // bottom_content stays empty — set_jack_in() places JackIn here and
    // the avatar overlays at render time.

    // ── ARCHIVE ────────────────────────────────────────────────────
    NetRoom& archive = b.add_room(archive_x, archive_y, kArchiveW, kArchiveH,
                                  "ARCHIVE", NetRoom::Border::Thin);
    archive.label_color    = net_theme::box_thin_color;
    archive.bottom_content = "\xe2\x96\x93 \xc2\xa7\xc2\xa7\xc2\xa7";  // ▓ §§§
    archive.bottom_color   = net_theme::data_node;

    // ── DVR ────────────────────────────────────────────────────────
    NetRoom& dvr = b.add_room(dvr_x, dvr_y, kDvrW, kDvrH, "DVR",
                              NetRoom::Border::Thin);
    dvr.label_color = net_theme::box_thin_color;

    // ── Connections ────────────────────────────────────────────────
    // Horizontal pipe chain across the lens bank — drawn at lens y+1
    // (the (o) row), which is where connect() anchors for h-dominant L.
    for (int i = 0; i + 1 < kLensCount; ++i) {
        b.connect(*lenses[i], *lenses[i + 1], NetPipe::Style::Thin);
    }

    // Five vertical drops from each lens into FEED. v-dominant L: drops
    // from lens bottom border down to FEED top, then a horizontal
    // segment along FEED's top row to FEED's centre column. Border
    // tiles are border-skipped by stamp_*, so FEED's frame survives.
    for (int i = 0; i < kLensCount; ++i) {
        b.connect(*lenses[i], feed, NetPipe::Style::Thin);
    }

    // FEED → ARCHIVE: horizontal pipe at FEED's y+1 row.
    b.connect(feed, archive, NetPipe::Style::Double);

    // FEED → DVR: vertical pipe down FEED's centre column.
    b.connect(feed, dvr, NetPipe::Style::Double);

    b.set_jack_in(feed);
    b.set_exit(dvr);

    return b.finalize();
}

}  // namespace astra

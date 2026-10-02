#include "VideoExportPauseOverlay.h"
#include <QPainter>
#include <QLinearGradient>
#include <QtMath>

namespace miacode::video_export::detail {

// Paint a semi-transparent two-bar pause glyph centred over the export
// frame. Called once per frame for the lead-in / pre-range window so the
// viewer immediately sees that the playfield is held in a stationary state
// (chart frozen at segmentStart, no playback yet). The glyph disappears
// the moment the lead-in ends and the chart starts to advance.
//
// SHIPPED STYLE: DarkRing (flat) — a solid dark disc with an even white
// hairline ring and square-cut white bars. Chosen 2026-08-16 after rendering
// all eight candidates below over both a bright PV and a dark playfield: the
// flat disc keeps the previous version's silhouette, and the ring is what
// stops a plain dark disc from disappearing into a dark background.
//
// The other seven candidates are kept as real code, not comments, so the
// choice can be revisited by editing ONE line (kLeadInPauseGlyphStyle) and
// re-rendering. They compile with the switch below, so they cannot rot.
//
// Every dimension derives from the shared PauseGlyphGeometry, which scales
// from the frame's short side — one knob resizes the whole badge.
//
// `Format_RGBA8888` (straight alpha) is what the export pipeline feeds
// FFmpeg — QPainter on this format composes correctly through Qt's
// internal premultiplied path; the final straight-alpha output is what
// FFmpeg's `overlay=…:alpha=straight` filter expects.
namespace {

enum class LeadInPauseGlyphStyle {
    GlassBadge,        // A — halo + gradient scrim + sheen + bar shadow (skeuomorphic)
    SolidDark,         // B — flat opaque dark disc, no ring
    SolidLight,        // C — flat white disc with dark bars
    OutlineRing,       // D — thick white ring over a lightly dimmed interior
    BarsOnly,          // E — no disc; deeper full-frame dim behind bare bars
    KnockoutBars,      // F — white disc with the bars knocked out of it
    DarkRing,          // G — flat dark disc + white hairline ring  << shipped
    TranslucentLight,  // H — 22% white disc + white ring, all-white palette
};

inline constexpr LeadInPauseGlyphStyle kLeadInPauseGlyphStyle = LeadInPauseGlyphStyle::DarkRing;

struct PauseGlyphGeometry {
    QPointF center;
    qreal diameter = 0.0;
    qreal radius = 0.0;
    QRectF badge;
    QRectF leftBar;
    QRectF rightBar;
};

PauseGlyphGeometry pauseGlyphGeometry(const QSize& frameSize)
{
    PauseGlyphGeometry geometry;
    const int shortSide = qMin(frameSize.width(), frameSize.height());
    geometry.center = QPointF(frameSize.width() / 2.0, frameSize.height() / 2.0);
    geometry.diameter = qMax<qreal>(28.0, shortSide * 0.17);
    geometry.radius = geometry.diameter / 2.0;
    geometry.badge = QRectF(
        geometry.center.x() - geometry.radius,
        geometry.center.y() - geometry.radius,
        geometry.diameter,
        geometry.diameter
    );
    // 0.40 x 0.115 of the diameter reads as the standard pause mark, with a
    // comfortable margin left inside the disc.
    const qreal barHeight = geometry.diameter * 0.40;
    const qreal barWidth = geometry.diameter * 0.115;
    const qreal gap = barWidth * 0.85;
    const qreal totalWidth = barWidth * 2.0 + gap;
    const qreal x0 = geometry.center.x() - totalWidth / 2.0;
    const qreal y0 = geometry.center.y() - barHeight / 2.0;
    geometry.leftBar = QRectF(x0, y0, barWidth, barHeight);
    geometry.rightBar = QRectF(x0 + barWidth + gap, y0, barWidth, barHeight);
    return geometry;
}

// Full-frame dim. SourceOver onto an opaque chart frame pulls its perceived
// brightness down; onto a transparent base it leaves a dark wash that FFmpeg's
// overlay filter then composites over the chart background. Either way the
// playfield reads paused.
void drawPauseFrameDim(QPainter& painter, const QRect& frameRect, int alpha)
{
    painter.fillRect(frameRect, QColor(0, 0, 0, alpha));
}

void drawPauseBars(
    QPainter& painter,
    const PauseGlyphGeometry& geometry,
    const QColor& color,
    qreal cornerFactor
)
{
    const qreal radius = geometry.leftBar.width() * cornerFactor;
    painter.setPen(Qt::NoPen);
    painter.setBrush(color);
    painter.drawRoundedRect(geometry.leftBar, radius, radius);
    painter.drawRoundedRect(geometry.rightBar, radius, radius);
}

void strokePauseRing(
    QPainter& painter,
    const QRectF& badge,
    const QColor& color,
    qreal width
)
{
    painter.setBrush(Qt::NoBrush);
    painter.setPen(QPen(color, width));
    painter.drawEllipse(badge.adjusted(width / 2.0, width / 2.0, -width / 2.0, -width / 2.0));
    painter.setPen(Qt::NoPen);
}

// A — frosted glass: radial halo, vertical scrim, top sheen, dropped bars.
void drawPauseGlyphGlassBadge(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);

    const qreal haloRadius = g.radius * 1.45;
    QRadialGradient halo(g.center, haloRadius);
    halo.setColorAt(0.0, QColor(0, 0, 0, 120));
    halo.setColorAt(g.radius / haloRadius, QColor(0, 0, 0, 96));
    halo.setColorAt(1.0, QColor(0, 0, 0, 0));
    painter.setBrush(halo);
    painter.drawEllipse(g.center, haloRadius, haloRadius);

    QLinearGradient scrim(g.badge.topLeft(), g.badge.bottomLeft());
    scrim.setColorAt(0.0, QColor(12, 14, 20, 132));
    scrim.setColorAt(1.0, QColor(4, 5, 8, 168));
    painter.setBrush(scrim);
    painter.drawEllipse(g.badge);

    QLinearGradient sheen(g.badge.topLeft(), g.badge.bottomLeft());
    sheen.setColorAt(0.0, QColor(255, 255, 255, 54));
    sheen.setColorAt(0.5, QColor(255, 255, 255, 10));
    sheen.setColorAt(1.0, QColor(255, 255, 255, 0));
    painter.setBrush(sheen);
    painter.drawEllipse(g.badge);

    strokePauseRing(painter, g.badge, QColor(255, 255, 255, 110), qMax<qreal>(1.0, g.diameter * 0.018));

    const qreal barRadius = g.leftBar.width() * 0.42;
    const qreal shadowOffset = qMax<qreal>(1.0, g.diameter * 0.012);
    painter.setBrush(QColor(0, 0, 0, 90));
    painter.drawRoundedRect(g.leftBar.translated(0.0, shadowOffset), barRadius, barRadius);
    painter.drawRoundedRect(g.rightBar.translated(0.0, shadowOffset), barRadius, barRadius);
    drawPauseBars(painter, g, QColor(255, 255, 255, 242), 0.42);
}

// B — flat opaque dark disc. Nearly vanishes on a dark playfield; kept for
// reference, use DarkRing instead.
void drawPauseGlyphSolidDark(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(17, 19, 26, 214));
    painter.drawEllipse(g.badge);
    drawPauseBars(painter, g, QColor(255, 255, 255, 255), 0.0);
}

// C — flat white disc with dark bars. Highest contrast, but the disc becomes
// the brightest object in frame over a bright PV.
void drawPauseGlyphSolidLight(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(255, 255, 255, 235));
    painter.drawEllipse(g.badge);
    drawPauseBars(painter, g, QColor(20, 22, 30, 255), 0.0);
}

// D — thick white ring, interior only lightly dimmed.
void drawPauseGlyphOutlineRing(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(0, 0, 0, 96));
    painter.drawEllipse(g.badge);
    strokePauseRing(painter, g.badge, QColor(255, 255, 255, 235), qMax<qreal>(2.0, g.diameter * 0.05));
    drawPauseBars(painter, g, QColor(255, 255, 255, 255), 0.0);
}

// E — bars alone over a deeper dim. The most restrained, and the weakest read.
void drawPauseGlyphBarsOnly(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 96);
    drawPauseBars(painter, g, QColor(255, 255, 255, 240), 0.5);
}

// F — white disc with the bars knocked back out of it, so the bars show the
// dimmed chart through them.
void drawPauseGlyphKnockoutBars(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(255, 255, 255, 216));
    painter.drawEllipse(g.badge);
    painter.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    painter.setBrush(QColor(0, 0, 0, 255));
    painter.drawRect(g.leftBar);
    painter.drawRect(g.rightBar);
    painter.setCompositionMode(QPainter::CompositionMode_SourceOver);
}

// G — SHIPPED. Flat dark disc plus an even white hairline ring: the flat read
// of B, with the ring keeping the silhouette on a dark playfield.
void drawPauseGlyphDarkRing(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(17, 19, 26, 214));
    painter.drawEllipse(g.badge);
    strokePauseRing(painter, g.badge, QColor(255, 255, 255, 200), qMax<qreal>(1.5, g.diameter * 0.028));
    drawPauseBars(painter, g, QColor(255, 255, 255, 255), 0.0);
}

// H — all-white palette: 22% white disc, white ring, white bars.
void drawPauseGlyphTranslucentLight(QPainter& painter, const QRect& frameRect, const PauseGlyphGeometry& g)
{
    drawPauseFrameDim(painter, frameRect, 70);
    painter.setBrush(QColor(255, 255, 255, 56));
    painter.drawEllipse(g.badge);
    strokePauseRing(painter, g.badge, QColor(255, 255, 255, 170), qMax<qreal>(1.5, g.diameter * 0.022));
    drawPauseBars(painter, g, QColor(255, 255, 255, 255), 0.0);
}

}  // namespace

void drawLeadInPauseOverlay(QImage* frame)
{
    if (frame == nullptr || frame->isNull()) {
        return;
    }
    const QSize frameSize = frame->size();
    if (qMin(frameSize.width(), frameSize.height()) <= 0) {
        return;
    }

    QPainter painter(frame);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(Qt::NoPen);

    const PauseGlyphGeometry geometry = pauseGlyphGeometry(frameSize);
    switch (kLeadInPauseGlyphStyle) {
    case LeadInPauseGlyphStyle::GlassBadge:
        drawPauseGlyphGlassBadge(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::SolidDark:
        drawPauseGlyphSolidDark(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::SolidLight:
        drawPauseGlyphSolidLight(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::OutlineRing:
        drawPauseGlyphOutlineRing(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::BarsOnly:
        drawPauseGlyphBarsOnly(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::KnockoutBars:
        drawPauseGlyphKnockoutBars(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::DarkRing:
        drawPauseGlyphDarkRing(painter, frame->rect(), geometry);
        break;
    case LeadInPauseGlyphStyle::TranslucentLight:
        drawPauseGlyphTranslucentLight(painter, frame->rect(), geometry);
        break;
    }
}


} // namespace miacode::video_export::detail

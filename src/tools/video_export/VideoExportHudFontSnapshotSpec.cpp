#include "app/services/HudFontPreferences.h"
#include "common/PreferenceProvider.h"
#include "export/video_export/VideoExportSnapshot.h"
#include "export/session/PreviewQuickExportSession.h"
#include "export/session/PreviewQuickD3D11ExportSession.h"
#include "preview/runtime/PreviewRuntime.h"
#include "preview/quick_scene/PreviewQuickHudLayer.h"

#include <QGuiApplication>
#include <QImage>
#include <QPainter>
#include <QTextStream>

using namespace miacode::preview::scene;

namespace {
bool require(bool condition, const char* message, QTextStream& err)
{
    if (!condition) err << "FAIL: " << message << Qt::endl;
    return condition;
}

class AmbientPreferences final : public miacode::preferences::PreferenceProvider {
public:
    QJsonObject values;
    QString resolvedLanguageToken() const override { return {}; }
};

QImage paint(const PreviewFrameState& state)
{
    QImage image(800, 800, QImage::Format_ARGB32_Premultiplied);
    image.fill(Qt::transparent);
    QPainter painter(&image);
    miacode::preview::hud::paintPreviewHudOverlay(painter, state, image.size());
    return image;
}
}

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QTextStream err(stderr);
    bool ok = true;
    VideoExportSnapshot source;
    source.chartTextUtf8 = QStringLiteral("&title=HUD Spec\n&first=0\n&lv_5=12\n&inote_5=(120){4}1,\n");
    source.difficultyId = 5;
    source.originalChartPath = QStringLiteral("C:/charts/hud/maidata.txt");
    source.outputPath = QStringLiteral("C:/charts/hud/out.mp4");
    source.contentDurationSeconds = 1.0;
    for (int area = 0; area < 5; ++area) source.hudFontSettings.paths[area] = QStringLiteral("C:/fonts/area-%1.ttf").arg(area);
    VideoExportSnapshot restored;
    QString error;
    const bool restoredOk = VideoExportSnapshot::fromJson(source.toJson(), &restored, &error);
    if (!restoredOk) err << "snapshot error: " << error << Qt::endl;
    ok &= require(restoredOk, "snapshot restores", err);
    ok &= require(restored.hudFontSettings == source.hudFontSettings, "all five area paths round-trip", err);
    VideoExportTask task;
    const bool taskOk = buildVideoExportTaskFromSnapshot(restored, &task, &error);
    if (!taskOk) err << "worker task error: " << error << Qt::endl;
    ok &= require(taskOk, "worker task reconstructs", err);
    ok &= require(task.hudFontSettings == source.hudFontSettings, "worker receives captured fonts", err);

    AmbientPreferences ambient;
    miacode::preferences::installPreferenceProvider(&ambient);
    ambient.values.insert(QStringLiteral("hud_font_path"), QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf"));
    QJsonObject legacyJson = source.toJson();
    legacyJson.remove(QStringLiteral("hud_fonts"));
    VideoExportSnapshot legacy;
    const bool legacyOk = VideoExportSnapshot::fromJson(legacyJson, &legacy, &error);
    if (!legacyOk) err << "old snapshot error: " << error << Qt::endl;
    ok &= require(legacyOk, "old snapshot restores", err);
    ok &= require(legacy.hudFontSettings == PreviewHudFontSettings{}, "old snapshot uses bundled defaults regardless of ambient preferences", err);

    const QString fontA = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/XiaolaiMono-Regular.subset.ttf");
    const QString fontB = QStringLiteral(MIACODE_SOURCE_ROOT "/assets/fonts/MapleMonoNormalNL-CN-Regular.ttf");
    PreviewFrameState frame;
    frame.render.showTimestamp = true;
    frame.playheadSeconds = 12.345;
    frame.hudFontSettings.paths[static_cast<int>(PreviewHudFontArea::Timestamp)] = fontA;
    PreviewQuickExportSession gl;
    PreviewQuickD3D11ExportSession d3d;
    gl.setFrameState(frame);
    d3d.setFrameState(frame);
    const QImage before = paint(gl.frameState());
    const QFont fontBefore = previewHudTimestampFontForArea(frame.hudFontSettings, PreviewHudFontArea::Timestamp, 20);
    PreviewRuntime live;
    live.setHudFontSettings(frame.hudFontSettings);
    const auto oldLive = live.frameStateSnapshot();
    frame.hudFontSettings.paths[static_cast<int>(PreviewHudFontArea::Timestamp)] = fontB;
    live.setHudFontSettings(frame.hudFontSettings);
    ambient.values.insert(QStringLiteral("hud_font_paths"), QJsonObject{{QStringLiteral("timestamp"), fontB}});
    gl.applyExportFrameTick(12.345, true, false, false, 0, 60);
    d3d.applyExportFrameTick(12.345, true, false, false, 0, 60);
    ok &= require(gl.frameState().hudFontSettings == oldLive->hudFontSettings
        && d3d.frameState().hudFontSettings == oldLive->hudFontSettings, "both render sessions retain captured fonts after live changes and frame ticks", err);
    ok &= require(live.frameStateSnapshot()->hudFontSettings == frame.hudFontSettings, "live preview accepts new settings independently", err);
    ok &= require(paint(gl.frameState()) == before && paint(d3d.frameState()) == before, "HUD output ignores ambient preference and live scene changes", err);
    ok &= require(fontBefore.family() != previewHudTimestampFontForArea(frame.hudFontSettings, PreviewHudFontArea::Timestamp, 20).family(), "different explicit font configurations resolve independently", err);
    ok &= require(paint(frame) != before, "explicit font changes affect rendered HUD", err);
    miacode::preferences::installPreferenceProvider(nullptr);
    if (ok) QTextStream(stdout) << "video_export_hud_font_snapshot_spec ok" << Qt::endl;
    return ok ? 0 : 1;
}

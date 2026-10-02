#pragma once
#include "MobileExportComposition.h"
#include <QQmlApplicationEngine>
#include <QGuiApplication>
#include <QQuickWindow>
#include <QQuickItem>
#include <QFileInfo>
#include <QDir>
#include <QTimer>
#include <QDebug>
#include <QSaveFile>
#include <QJsonObject>
#include <QJsonDocument>
#include <cstdio>

namespace miacode::android {
inline void startExportUiSmoke(QGuiApplication& app, QQmlApplicationEngine& engine,
    AndroidDocumentSession& document, MobilePreview& preview, MobileVideoExport& exporter,
    MobileExportComposition& composition) {
    if (engine.rootObjects().isEmpty()) { app.exit(40); return; }
    auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    auto* session = qobject_cast<ui::ExportSession*>(composition.session());
    auto* settings = qobject_cast<ui::PreviewSettingsModel*>(composition.settings());
    const QString directory = QFileInfo(document.currentFilePath()).absolutePath() + "/export-ui-proof";
    QDir().mkpath(directory);
    QObject::connect(&exporter, &MobileVideoExport::finished, &app, [&app, directory](bool ok, const QString& path, const QString& error) {
        const bool passed = ok && QFileInfo(path).size() > 44;
        QSaveFile report(directory + "/verification.json");
        if (report.open(QIODevice::WriteOnly)) {
            report.write(QJsonDocument(QJsonObject{{"success", passed}, {"output", path}, {"error", error},
                {"runtimeSettingsVerified", true}, {"rangePlaybackVerified", true}, {"rangeStart", 10}, {"rangeEnd", 15},
                {"source", "actual v2 ExportSession"}}).toJson());
            report.commit();
        }
        std::fprintf(stderr, "Export UI smoke: %s; output=%s; error=%s\n", passed ? "passed" : "failed", qPrintable(path), qPrintable(error));
        app.exit(passed ? 0 : 41);
    });
    QTimer::singleShot(500, &app, [window] { window->setProperty("activePage", "export"); });
    QTimer::singleShot(1500, &app, [window, session, settings, &app, &preview, &document, &composition, directory] {
        if (!session->pageSessionActive() || !session->unavailableReason().isEmpty()
            || session->selectedDifficultyId() != document.activeDifficulty()) {
            qCritical() << "Export UI session failed to enter" << session->unavailableReason(); app.exit(42); return;
        }
        settings->setValue("brightnessOuter", 37);
        settings->setValue("tapFlowSpeed", 6);
        settings->setValue("scaleMode", 1);
        settings->setValue("layoutSquareScale", 85);
        settings->setValue("judgeEffectSlide", false);
        settings->setValue("judgeEffectTap", false);
        settings->setValue("judgeEffectBreak", true);
        settings->setValue("judgeEffectTouch", true);
        settings->setSkinIndex(0);
        settings->setSkinJudgeEffectIndex(1);
        settings->setOutlineIndex(0);
        session->setShowChartInfoHud(true);
        session->setFps(30);
        session->setResolutionIndex(0);
        session->setExportRangeSeconds(10, 15);
        session->setOutputPath(directory + "/chart-ui.wav");
        const auto& state = preview.sceneRuntime().frameState();
        const auto task = composition.buildSeedTask(document.activeDifficulty());
        if (qAbs(state.render.backgroundBrightnessOuter - 0.37) > 0.001
            || qAbs(state.render.tapFlowSpeed - 6) > 0.001 || !state.render.showChartInfoHud
            || preview.backgroundScaleMode() != 1 || qAbs(preview.layoutSquareScale() - 0.85) > 0.001
            || task.muriRenderOptions.showChartReviewSlideJudgeOverlay || task.muriRenderOptions.showChartReviewTapJudgeOverlay
            || !task.muriRenderOptions.showChartReviewBreakJudgeOverlay || !task.muriRenderOptions.showChartReviewTouchJudgeOverlay
            || task.outlineVariant != PreviewOutlineVariant::Point || task.judgeEffectStyle != PreviewJudgeEffectStyle::Starry
            || QFileInfo(task.skinDirectory).fileName() != "skinSD"
            || !composition.resolveCustomOutlineDir().endsWith("background/outlines")
            || qAbs(session->exportStartSeconds() - 10) > 0.001 || qAbs(session->exportEndSeconds() - 15) > 0.001) {
            std::fprintf(stderr, "Export settings mismatch: brightness=%.3f flow=%.2f scaleMode=%d scale=%.2f skin=%s range=%.2f..%.2f\n",
                state.render.backgroundBrightnessOuter, state.render.tapFlowSpeed, preview.backgroundScaleMode(),
                preview.layoutSquareScale(), qPrintable(task.skinDirectory), session->exportStartSeconds(), session->exportEndSeconds());
            qCritical() << "Export UI settings did not reach runtime/range"; app.exit(43); return;
        }
        QQuickItem* rangeButton = nullptr;
        for (auto* item : window->findChildren<QQuickItem*>("exportRangeModeButton"))
            if (item->isVisible() && item->isEnabled()) { rangeButton = item; break; }
        if (!rangeButton || !QMetaObject::invokeMethod(rangeButton, "clicked")) { app.exit(47); return; }
        preview.setPositionSeconds(9);
        preview.setPlaying(true);
        if (qAbs(preview.positionSeconds() - 10) > 0.001) { app.exit(48); return; }
        preview.setPositionSeconds(14.75);
        QTimer::singleShot(1200, &app, [window, session, rangeButton, &preview, &app, directory] {
            if (preview.playing() || qAbs(preview.positionSeconds() - 15) > 0.001) {
                std::fprintf(stderr, "Export range playback did not stop at its endpoint: %.3f\n", preview.positionSeconds());
                app.exit(49); return;
            }
            QMetaObject::invokeMethod(rangeButton, "clicked");
            session->setExportRangeSeconds(10, 15);
            if (!window->grabWindow().save(directory + "/export-page.png")) {
                qCritical() << "Cannot capture actual export UI"; app.exit(44); return;
            }
            session->startExport();
            if (!session->exportRunning()) { qCritical() << "Actual export UI failed to launch"; app.exit(45); }
        });
    });
    QTimer::singleShot(90000, &app, [&app, &exporter] {
        exporter.cancel(); qCritical() << "Export UI smoke timeout"; app.exit(46);
    });
}
}

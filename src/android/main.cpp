#include "AndroidDocumentSession.h"
#include "AndroidEditorTools.h"
#include "app/ui/editor/EditorController.h"
#include "app/services/EditorSyncController.h"
#include "MobilePreview.h"
#include "MobileTimeline.h"
#include "MobileVideoExport.h"
#include "MobileExportComposition.h"
#include "AndroidFileRequests.h"
#include "app/ui/document/AnalysisModel.h"
#include "app/ui/chrome/ShortcutModel.h"
#include "timeline/quick/TimelineQuickItem.h"
#include "preview/quick_scene/PreviewQuickSceneRoot.h"
#include "preview/quick_scene/PreviewQuickHudLayer.h"
#include "timeline/TimelineNoteAssets.h"
#include "common/AssetPaths.h"
#include <QQuickImageProvider>
#include <QTranslator>
#ifdef Q_OS_ANDROID
#include "AndroidPlatformBridge.h"
#endif
#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQmlProperty>
#include <QQuickStyle>
#include <QStandardPaths>
#include <QQuickWindow>
#include <QTimer>
#include <QFontDatabase>
#include <QSurfaceFormat>
#include <QTextCursor>
#include <cstdio>
#ifndef Q_OS_ANDROID
#include "HostVideoInspection.h"
#include "ExportUiSmoke.h"
#endif

class MobileNoteImages final : public QQuickImageProvider {
public:
    MobileNoteImages() : QQuickImageProvider(Image), icons_(miacode::timeline::loadTimelineNoteAssets(
        miacode::assets::assetPath("skin/skinDX"))) {}
    QImage requestImage(const QString& id, QSize* size, const QSize& requested) override {
        const auto type = id.section('?', 0, 0) == "break" ? QStringLiteral("tap_break") : id.section('?', 0, 0);
        const auto image = icons_.noteIcons.value(type).toImage();
        if (size) *size = image.size();
        return requested.isValid() ? image.scaled(requested, Qt::KeepAspectRatio, Qt::SmoothTransformation) : image;
    }
private:
    miacode::timeline::TimelineNoteAssetSet icons_;
};

int main(int argc, char* argv[])
{
#ifdef Q_OS_ANDROID
    QSurfaceFormat format;
    format.setRenderableType(QSurfaceFormat::OpenGLES);
    format.setVersion(3, 0);
    QSurfaceFormat::setDefaultFormat(format);
#endif
    QGuiApplication app(argc, argv);
    app.setOrganizationName("MiaCode");
    app.setApplicationName("MiaCodeAndroid");
    QFont uiFont = app.font();
    uiFont.setPixelSize(13);
    app.setFont(uiFont);
    QQuickStyle::setStyle("Basic");
    QTranslator translator;
    if (translator.load(":/i18n/zh_CN.qm")) app.installTranslator(&translator);
    const QStringList args = app.arguments();
#ifndef Q_OS_ANDROID
    if (args.contains("--export-ui-smoke")) app.setApplicationName("MiaCodeMobileExportUiSpec");
#endif
    std::fprintf(stderr, "Mobile startup: chartExportSmoke=%s\n", args.contains("--export-smoke") ? "true" : "false");
#ifndef Q_OS_ANDROID
    const int inspectionIndex = args.indexOf("--inspect-video");
    if (inspectionIndex >= 0 && inspectionIndex + 2 < args.size())
        return inspectExportVideo(app, args.at(inspectionIndex + 1), args.at(inspectionIndex + 2));
#endif
#ifdef Q_OS_WIN
    if (args.contains("--trace-crash")) {
        extern void enableHostCrashTrace();
        enableHostCrashTrace();
    }
#endif
#ifndef Q_OS_ANDROID
    // The offscreen Windows QPA has no system font discovery. Use a local
    // system font for host layout captures; Android keeps its native fallback.
    if (args.contains("--capture")) {
        const int font = QFontDatabase::addApplicationFont("C:/Windows/Fonts/msyh.ttc");
        const auto families = QFontDatabase::applicationFontFamilies(font);
        if (!families.isEmpty()) {
            uiFont.setFamily(families.first());
            app.setFont(uiFont);
        }
    }
#endif
    const int storageIndex = args.indexOf("--storage-root");
    miacode::android::AndroidDocumentSession session(storageIndex >= 0 && storageIndex + 1 < args.size()
        ? args.at(storageIndex + 1) : QStandardPaths::writableLocation(QStandardPaths::AppDataLocation));
    if (args.contains("--export-smoke") && session.recoveryAvailable()) session.recover();
#ifndef Q_OS_ANDROID
    const int fixtureIndex = args.indexOf("--fixture");
    if (fixtureIndex >= 0 && fixtureIndex + 1 < args.size() && !session.loadHostFixture(args.at(fixtureIndex + 1))) return 6;
#endif
#ifdef Q_OS_ANDROID
    miacode::android::AndroidPlatformBridge bridge(&session);
#else
    QObject::connect(&session, &miacode::android::AndroidDocumentSession::ioRequested,
        &session, [&session](const QString& kind, const QString&, const QString&) {
            session.completeIo(QJsonObject{{"kind", kind}, {"ok", false},
                {"error", "系统文件选择和媒体探针需在安卓设备运行"}});
        });
#endif
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &session,
        [&session](Qt::ApplicationState state) {
            if (state != Qt::ApplicationActive && !session.recoveryAvailable()) session.flushRecovery();
        });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &session, [&session] {
        if (!session.recoveryAvailable()) session.flushRecovery();
    });
    miacode::android::AndroidEditorTools editorTools;
    miacode::ui::EditorController controller;
    controller.setAutoCompletionEnabled(true);
    miacode::EditorSyncController editorSync;
    miacode::ui::ShortcutModel shortcuts;
    const int codeFontId = QFontDatabase::addApplicationFont(miacode::assets::assetPath("fonts/MapleMonoNormalNL-CN-Regular.ttf"));
    const auto codeFamilies = QFontDatabase::applicationFontFamilies(codeFontId);
    miacode::android::MobilePreview preview(&session);
    miacode::android::MobileVideoExport chartExport(session, preview);
    miacode::android::MobileExportComposition exportComposition(session, preview, chartExport);
#ifdef Q_OS_ANDROID
    miacode::android::AndroidFileRequests fileRequests(*qobject_cast<miacode::UiRequestService*>(exportComposition.requests()));
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::uiFileResult,
        &fileRequests, &miacode::android::AndroidFileRequests::deliver);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::exportPublicationUpdate,
        &chartExport, &miacode::android::MobileVideoExport::publicationUpdate);
    QObject::connect(&bridge, &miacode::android::AndroidPlatformBridge::chartExportCancelled,
        &chartExport, &miacode::android::MobileVideoExport::cancel);
#endif
    miacode::AnalysisService analysis(session.workspace(), SimaiNativeValidationLocale::Chinese);
    miacode::ui::AnalysisModel analysisModel(session.workspace(), analysis);
    miacode::android::MobileTimeline timeline(session, preview, editorSync, analysis);
    QObject::connect(&preview, &miacode::android::MobilePreview::muriParametersChanged, &analysis, [&] {
        analysis.setMuriParameters(preview.muriRenderOptions(), preview.muriTapOnSlideThresholdMs() / 1000.0);
    });
    const int secondIndex = args.indexOf("--preview-second");
    if (secondIndex >= 0 && secondIndex + 1 < args.size()) preview.setPositionSeconds(args.at(secondIndex + 1).toDouble());
    if (args.contains("--muri")) preview.setMuriCheckEnabled(true);
    qmlRegisterType<PreviewQuickSceneRoot>("MiaCode.Preview", 1, 0, "PreviewQuickSceneRoot");
    qmlRegisterType<PreviewQuickHudLayer>("MiaCode.Preview", 1, 0, "PreviewQuickHudLayer");
    qmlRegisterType<TimelineQuickItem>("MiaCode.Timeline", 1, 0, "TimelineQuickItem");
    QQmlApplicationEngine engine;
    engine.addImageProvider("noteicon", new MobileNoteImages);
    engine.rootContext()->setContextProperty("androidSession", &session);
    engine.rootContext()->setContextProperty("editorTools", &editorTools);
    engine.rootContext()->setContextProperty("v2EditorController", &controller);
    engine.rootContext()->setContextProperty("editorSync", &editorSync);
    engine.rootContext()->setContextProperty("mobilePreview", &preview);
    engine.rootContext()->setContextProperty("mobileTimeline", &timeline);
    engine.rootContext()->setContextProperty("mobileAnalysis", &analysisModel);
    engine.rootContext()->setContextProperty("mobileShortcuts", &shortcuts);
    engine.rootContext()->setContextProperty("mobileChartExport", &chartExport);
    engine.rootContext()->setContextProperty("mobileExport", &exportComposition);
    engine.rootContext()->setContextProperty("mobileCodeFontFamily", codeFamilies.isEmpty() ? app.font().family() : codeFamilies.first());
    QObject::connect(&app, &QGuiApplication::applicationStateChanged, &preview,
        [&preview](Qt::ApplicationState state) { if (state != Qt::ApplicationActive) preview.setPlaying(false); });
    QObject::connect(&engine, &QQmlApplicationEngine::objectCreationFailed,
        &app, [] { QCoreApplication::exit(1); }, Qt::QueuedConnection);
    engine.loadFromModule("MiaCode.UI", "AndroidMain");
#ifndef Q_OS_ANDROID
    if (args.contains("--export-ui-smoke"))
        miacode::android::startExportUiSmoke(app, engine, session, preview, chartExport, exportComposition);
    if (args.contains("--export-smoke") && !engine.rootObjects().isEmpty()) {
        if (auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first())) window->setVisible(false);
    }
#endif
    if (args.contains("--export-smoke")) {
        QObject::connect(&chartExport, &miacode::android::MobileVideoExport::finished, &app,
            [&app](bool success, const QString& output, const QString& error) {
                std::fprintf(stderr, "Chart export smoke: %s; output=%s; error=%s\n",
                    success ? "passed" : "failed", qPrintable(output), qPrintable(error));
                app.exit(success ? 0 : 30);
            });
        QTimer::singleShot(1000, &app, [&chartExport, &app, args] {
            auto task = chartExport.buildTask();
            task.outputWidth = 720; task.outputHeight = 720; task.fps = 30;
            task.exportStartSeconds = 10; task.contentDurationSeconds = 5; task.fullRangeExport = false;
            const int outputIndex = args.indexOf("--export-output");
            if (outputIndex >= 0 && outputIndex + 1 < args.size()) task.outputPath = args.at(outputIndex + 1);
            if (args.contains("--export-intro")) {
                task.exportStartSeconds = 0; task.contentDurationSeconds = 5;
                task.fullRangeExport = true; task.intro.enabled = true;
            }
            if (args.contains("--export-static")) task.backgroundMediaPath = chartExport.buildTask().intro.jacketPath;
            QString error;
            if (!chartExport.start(task, &error)) { qCritical() << "Chart export launch failed:" << error; app.exit(31); }
        });
    }
    if (args.contains("--layout-smoke") && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        window->resize(807, 345);
        QTimer::singleShot(700, &app, [window, &app, &preview] {
            auto* pane = window->findChild<QQuickItem*>("v2PreviewPane");
            auto* stats = window->findChild<QQuickItem*>("previewNoteStatistics");
            if (!pane || !stats) { app.exit(20); return; }
            const double baseline = window->property("workbenchScale").toDouble();
            auto* timer = new QTimer(window);
            auto step = std::make_shared<int>(0);
            auto seenRows = std::make_shared<int>(0);
            preview.setPlaying(true);
            QObject::connect(timer, &QTimer::timeout, window, [pane, stats, window, timer, step, seenRows, baseline, &app] {
                const double scale = window->property("workbenchScale").toDouble();
                const int rows = stats->property("rows").toInt();
                const int capacity = stats->property("columns").toInt() * rows;
                if (rows == 1 || rows == 2) *seenRows |= 1 << (rows - 1);
                if (qAbs(scale - baseline) > 0.000001 || capacity < 6) {
                    std::fprintf(stderr, "Layout smoke: preview resize changed workbench scale or invalidated the grid\n");
                    timer->stop(); app.exit(21); return;
                }
                if (*step == 18) {
                    if (*seenRows != 3) {
                        std::fprintf(stderr, "Layout smoke: did not exercise both statistics layouts\n");
                        timer->stop(); app.exit(23); return;
                    }
                    std::printf("v2 Android layout: 18 resizes across statistics breakpoints: passed\n");
                    timer->stop(); app.exit(0); return;
                }
                const double widths[] = {520, 536, 420, 610, 527, 529};
                QQmlProperty preferredWidth(pane, "SplitView.preferredWidth", qmlContext(pane));
                if (!preferredWidth.write(widths[(*step)++ % 6])) {
                    timer->stop(); app.exit(22);
                }
            });
            timer->start(100);
        });
    }
    const int captureIndex = args.indexOf("--capture");
    if (captureIndex >= 0 && captureIndex + 1 < args.size() && !engine.rootObjects().isEmpty()) {
        auto* window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
        const int sizeIndex = args.indexOf("--size");
        if (window && sizeIndex >= 0 && sizeIndex + 1 < args.size()) {
            const auto size = args.at(sizeIndex + 1).split('x');
            if (size.size() == 2) window->resize(size.at(0).toInt(), size.at(1).toInt());
        }
        QTimer::singleShot(1000, &app, [window, &app, args, captureIndex] {
            const bool ok = window && window->grabWindow().save(args.at(captureIndex + 1));
            app.exit(ok ? 0 : 2);
        });
    }
    if (args.contains("--ui-smoke") && !engine.rootObjects().isEmpty()) {
        QTimer::singleShot(500, &app, [&engine, &session, &app, &controller] {
            auto* editor = engine.rootObjects().first()->findChild<QQuickItem*>("sourceArea");
            auto* quick = editor ? editor->property("textDocument").value<QQuickTextDocument*>() : nullptr;
            auto* document = quick ? quick->textDocument() : nullptr;
            if (!document || controller.canUndo()) {
                std::fprintf(stderr, "UI smoke: initial document has invalid undo state\n");
                app.exit(3);
                return;
            }
            editor->forceActiveFocus();
            QTextCursor cursor(document);
            cursor.movePosition(QTextCursor::End);
            cursor.insertText(",7");
            if (!session.dirty() || !session.chartText().endsWith(",7")) {
                std::fprintf(stderr, "UI smoke: editor did not publish the edit\n");
                app.exit(4);
                return;
            }
            const int originalDifficulty = session.activeDifficulty();
            session.addDifficulty(6);
            session.selectDifficulty(6);
            QTimer::singleShot(100, &app, [&engine, &session, &app, &controller, originalDifficulty] {
                if (controller.canUndo() || !session.chartText().isEmpty()) { app.exit(5); return; }
                session.selectDifficulty(originalDifficulty);
                QTimer::singleShot(100, &app, [&engine, &session, &app, &controller] {
                    auto* view = engine.rootObjects().first()->findChild<QQuickItem*>("v2SourceEditor");
                    const bool restored = controller.canUndo() && session.chartText().endsWith(",7");
                    if (restored && view) QMetaObject::invokeMethod(view, "undo");
                    const bool ok = restored && !session.chartText().endsWith(",7");
                    std::printf("v2 Android editor: typing, difficulty isolation, retained undo: %s\n", ok ? "passed" : "failed");
                    app.exit(ok ? 0 : 7);
                });
            });
        });
    }
    return app.exec();
}

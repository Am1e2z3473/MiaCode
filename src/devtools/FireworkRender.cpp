#include "export/cover_export/SceneFrameRenderer.h"
#include "export/video_export/VideoExportController.h"
#include "core/scene/PreviewSceneConstants.h"

#include <QCommandLineParser>
#include <QDir>
#include <QElapsedTimer>
#include <QGuiApplication>
#include <QPainter>
#include <QPainterPath>
#include <QThread>
#include <QTextStream>
#include <QtQml/qqmlextensionplugin.h>

Q_IMPORT_QML_PLUGIN(MiaCode_PreviewPlugin)

int main(int argc, char** argv)
{
    QGuiApplication app(argc, argv);
    QCommandLineParser parser;
    parser.addHelpOption();
    parser.addOption({"background", "Background image.", "path"});
    parser.addOption({"skin", "Note skin directory; outline uses Line.", "path"});
    parser.addOption({"output", "Frame output directory.", "path"});
    parser.addOption({"size", "Square side in pixels.", "pixels", "720"});
    parser.addOption({"fps", "Frames per second.", "fps", "60"});
    parser.process(app);
    const int side = parser.value("size").toInt();
    const int fps = parser.value("fps").toInt();
    const QString output = parser.value("output");
    QImage background(parser.value("background"));
    if (background.isNull() || side <= 0 || fps <= 0 || output.isEmpty()
        || !QDir().mkpath(output)) return 1;

    VideoExportTask task;
    task.skinDirectory = parser.value("skin");
    task.outlineVariant = PreviewOutlineVariant::Line;
    task.contentDurationSeconds = 2.0;
    TimelineNoteMarker marker;
    marker.type = QStringLiteral("touch");
    marker.second = 0.5;
    marker.endSecond = marker.second;
    marker.isFirework = true;
    marker.touchPoint = QPointF(miacode::preview::scene::kLogicalCanvasCenter,
                                miacode::preview::scene::kLogicalCanvasCenter);
    task.noteMarkers.append(marker);
    miacode::cover_export::SceneFrameRenderer renderer;
    QString error;
    if (!renderer.bootstrap(task, &error) || !renderer.prepareCaptureWindow(side, 0.0, &error)) {
        QTextStream(stderr) << error << Qt::endl;
        return 1;
    }
    QElapsedTimer timer;
    timer.start();
    while (!renderer.captureReady() && timer.elapsed() < 10000) {
        QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
        QThread::msleep(5);
    }
    const QImage fitted = background.scaled(side, side, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
    const QRect source((fitted.width() - side) / 2, (fitted.height() - side) / 2, side, side);
    for (int frame = 0; frame < 2 * fps; ++frame) {
        const QImage overlay = renderer.renderAt(double(frame) / fps, side, &error);
        if (overlay.isNull()) {
            QTextStream(stderr) << error << Qt::endl;
            return 1;
        }
        QImage image(side, side, QImage::Format_RGB32);
        image.fill(QColor(18, 18, 18));
        QPainter painter(&image);
        QPainterPath disk;
        const double diameter = renderer.playfieldDiskDiameterFraction() * side;
        disk.addEllipse(QPointF(side / 2.0, side / 2.0), diameter / 2.0, diameter / 2.0);
        painter.setClipPath(disk);
        painter.drawImage(QRect(0, 0, side, side), fitted, source);
        painter.fillRect(image.rect(), QColor(0, 0, 0, 150));
        painter.setClipping(false);
        painter.drawImage(QRect(0, 0, side, side), overlay);
        painter.end();
        if (!image.save(QDir(output).filePath(QStringLiteral("%1.png").arg(frame, 4, 10, QLatin1Char('0'))))) return 1;
    }
    QTextStream(stdout) << output << Qt::endl;
    return 0;
}

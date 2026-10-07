#include "app/services/ProjectPreferences.h"
#include "app/services/PreferenceJsonFile.h"
#include "audio/PreviewAudioSettings.h"

#include <QCoreApplication>
#include <QProcess>
#include <QTemporaryDir>
#include <QTextStream>

namespace {
bool put(const QString& path, const QByteArray& value)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(value) == value.size();
}
QByteArray bytes(const QString& path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}
QString chart(const QString& root, const QString& name)
{
    const QString directory = QDir(root).filePath(name);
    if (!QDir().mkpath(directory)) return {};
    const QString path = QDir(directory).filePath(QStringLiteral("maidata.txt"));
    return put(path, "&title=Read recovery fixture\n&inote_1={4}1,E\n") ? path : QString();
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    using namespace miacode::project_preferences;
    if (app.arguments().size() == 3 && app.arguments().at(1) == "--restart-check") {
        const QString chartPath = app.arguments().at(2);
        miacode::debug_log::setSessionProjectLogDirectory(QFileInfo(chartPath).absolutePath());
        QJsonObject root = load(chartPath);
        const bool originalLoaded = root.value("vendor").toObject().value("keep") == 42
            && root.value("preview_audio").toObject().value("track_volume").toDouble() == 0.23
            && root.value("preview_audio").toObject().value("tap_volume").toDouble() == 0.41;
        root.insert("restart_edit", true);
        const bool persisted = originalLoaded && save(chartPath, root);
        miacode::debug_log::shutdownAsyncLogWriter();
        return persisted ? 0 : 1;
    }
    QTemporaryDir temporary;
    if (!temporary.isValid()) return 1;
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    const QString firstChart = chart(temporary.path(), "blocked-project");
    const QString path = projectPreferencesFilePath(firstChart);
    expect(!firstChart.isEmpty() && QDir().mkpath(path), "real chart fixture with unreadable preference file");
    expect(load(firstChart).isEmpty(), "unreadable project falls back without changing runtime contract");
    PreviewAudioSettings fallback;
    fallback.setTrackPercent(67);
    const QJsonObject pendingAudio = fallback.toJson();
    const QByteArray original("{\"vendor\":{\"keep\":42},\"preview_audio\":{\"track_volume\":0.23,\"tap_volume\":0.41,\"answer_volume\":0.37}}");
    expect(QDir().rmdir(path) && put(path, original), "restore complete original mixer after IO recovers");
    QJsonObject fresh = load(firstChart);
    expect(fresh.value("vendor").toObject().value("keep") == 42, "fresh read observes recovered original root");
    fresh.insert("preview_audio", pendingAudio);
    expect(!save(firstChart, fresh) && bytes(path) == original,
           "fallback compound mixer cannot overwrite original channels after IO recovery");
    const QString alias = QFileInfo(firstChart).absolutePath() + QStringLiteral("/../blocked-project/maidata.txt");
    expect(!save(alias, fresh) && bytes(path) == original, "normalized path aliases retain the same write block");

    const QString otherChart = chart(temporary.path(), "other-project");
    const QJsonObject other{{QStringLiteral("other"), 73}};
    expect(load(otherChart).isEmpty() && save(otherChart, other) && load(otherChart) == other,
           "an unreadable project does not block another project");
    const QString missingChart = chart(temporary.path(), "missing-project");
    const QString missingPath = projectPreferencesFilePath(missingChart);
    const QString parent = QFileInfo(missingPath).absolutePath();
    expect(put(parent, "obstacle") && load(missingChart).isEmpty() && !save(missingChart, other),
           "missing preference with unwritable parent remains a write failure");
    expect(QFile::remove(parent) && save(missingChart, other) && load(missingChart) == other,
           "missing-file write failure remains retryable after its parent recovers");

    miacode::debug_log::shutdownAsyncLogWriter();
    expect(bytes(miacode::debug_log::runtimeLogPath()).contains("project-read-unavailable save-blocked-for-session"),
           "silent refusal leaves log evidence");
    QProcess restarted;
    restarted.start(QCoreApplication::applicationFilePath(), {QStringLiteral("--restart-check"), firstChart});
    expect(restarted.waitForFinished(15000) && restarted.exitStatus() == QProcess::NormalExit && restarted.exitCode() == 0,
           "independent process reads original mixer and resumes persistence");
    const QJsonObject afterRestart = load(firstChart);
    expect(afterRestart.value("restart_edit").toBool() && afterRestart.value("vendor").toObject().value("keep") == 42
               && afterRestart.value("preview_audio").toObject().value("tap_volume").toDouble() == 0.41,
           "restart edits preserve original unrelated values");
    expect(!save(firstChart, fresh), "another process does not clear this lifetime's original read block");
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}

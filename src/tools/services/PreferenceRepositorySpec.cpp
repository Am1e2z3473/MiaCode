#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceJsonFile.h"

#include <QCoreApplication>
#include <QStandardPaths>
#include <QSemaphore>
#include <QTemporaryDir>
#include <QTextStream>
#include <QThread>

#include <atomic>

namespace {
bool put(const QString& path, const QByteArray& bytes)
{
    QFile file(path);
    return file.open(QIODevice::WriteOnly) && file.write(bytes) == bytes.size();
}
QJsonObject withUi(QJsonObject root, const QString& key, const QJsonValue& value)
{
    QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    ui.insert(key, value);
    root.insert(QStringLiteral("ui"), ui);
    return root;
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTemporaryDir temporary;
    QTextStream err(stderr);
    bool ok = true;
    const auto expect = [&](bool condition, const char* message) {
        if (!condition) { err << "FAIL: " << message << '\n'; ok = false; }
    };
    using PreferenceDocument::Repository;
    using namespace miacode::preference_json_file;
    expect(temporary.isValid(), "temporary preferences directory");
    miacode::debug_log::setSessionProjectLogDirectory(temporary.path());
    const QString path = temporary.filePath(QStringLiteral("preferences.json"));
    Repository repository(path);
    const QJsonObject initial = repository.snapshot();
    expect(!repository.isDirty() && read(path).object == initial, "first load persists initial defaults");
    expect(put(path, "{\"schema\":\"old\",\"ui\":{\"theme\":\"light\"}}"), "external update fixture");
    expect(repository.snapshot() == initial, "ordinary reads retain the lifetime snapshot");
    expect(repository.replace(initial) && read(path).object != initial,
           "unchanged successfully persisted snapshot skips another write");
    Repository nextLifetime(path);
    expect(nextLifetime.snapshot().value("ui").toObject().value("theme") == "light"
               && nextLifetime.storedSchema() == "old", "new repository observes file and original schema");

    expect(QFile::remove(path) && QDir().mkdir(path), "block persistence at existing path");
    QJsonObject pending = withUi(repository.snapshot(), QStringLiteral("theme"), QStringLiteral("light"));
    expect(!repository.replace(pending) && repository.isDirty() && repository.snapshot() == pending,
           "failed save accepts runtime value and remains dirty");
    pending.insert(QStringLiteral("updates"), QJsonObject{{QStringLiteral("ignored"), true}});
    expect(!repository.replace(pending) && repository.snapshot() == pending,
           "next edit preserves previously unsaved sections");
    expect(QDir().rmdir(path), "remove persistence obstacle");
    expect(repository.replace(repository.snapshot()) && !repository.isDirty() && read(path).object == pending,
           "equal dirty snapshot retries and persists all pending edits");

    std::atomic<bool> stop{false};
    std::atomic<bool> consistent{true};
    QSemaphore started;
    QThread* reader = QThread::create([&] {
        started.release();
        while (!stop.load()) {
            const QJsonObject snapshot = repository.snapshot();
            if (snapshot.value("sequence_a") != snapshot.value("sequence_b")) {
                consistent.store(false);
            }
        }
    });
    reader->start();
    started.acquire();
    for (int sequence = 0; sequence < 12; ++sequence) {
        QJsonObject next = repository.snapshot();
        next.insert(QStringLiteral("sequence_a"), sequence);
        next.insert(QStringLiteral("sequence_b"), sequence);
        expect(repository.replace(next), "concurrent writer persists a complete snapshot");
    }
    stop.store(true);
    reader->wait();
    delete reader;
    expect(consistent.load(), "worker reads remain coherent during whole-document updates");

    const QString blocked = temporary.filePath(QStringLiteral("blocked"));
    expect(put(blocked, "parent"), "create first-load obstacle");
    const QString missingPath = QDir(blocked).filePath(QStringLiteral("preferences.json"));
    Repository firstFailure(missingPath);
    const QJsonObject firstPending = firstFailure.snapshot();
    expect(firstFailure.isDirty() && !firstFailure.replace(firstPending),
           "first-load persistence failure cannot be deduplicated");
    expect(QFile::remove(blocked) && QDir().mkdir(blocked), "resolve first-load obstacle");
    expect(firstFailure.replace(firstPending) && read(missingPath).object == firstPending,
           "first-load pending defaults retry successfully");

    const QString legacyPath = temporary.filePath(QStringLiteral("legacy-unreadable"));
    const QString migratedPath = temporary.filePath(QStringLiteral("migrated.json"));
    expect(QDir().mkdir(legacyPath), "block legacy preference reading");
    Repository legacyFailure(migratedPath, legacyPath);
    QJsonObject legacyPending = legacyFailure.snapshot();
    expect(legacyFailure.isDirty() && !QFile::exists(migratedPath),
           "unreadable legacy source cannot be hidden by new default primary");
    legacyPending.insert(QStringLiteral("session_edit"), true);
    expect(!legacyFailure.replace(legacyPending) && !QFile::exists(migratedPath),
           "save remains pending while legacy source is unreadable");
    expect(QDir().rmdir(legacyPath) && put(legacyPath, "{\"old_setting\":42}"),
           "resolve legacy read error");
    expect(!QFile::exists(migratedPath) && legacyFailure.snapshot() == legacyPending,
           "source recovery alone does not reload or overwrite the current session");
    expect(!legacyFailure.replace(legacyPending) && !QFile::exists(migratedPath),
           "unread legacy migration remains blocked for this repository lifetime");
    Repository recoveredLegacy(migratedPath, legacyPath);
    expect(recoveredLegacy.snapshot().value("old_setting") == 42
               && read(migratedPath).object.value("old_setting") == 42
               && read(legacyPath).object.value("old_setting") == 42,
           "new lifetime migrates readable legacy data and preserves its source");

    const QJsonObject legacy{{QStringLiteral("ui"), QJsonObject{{QStringLiteral("editor_auto_close_brackets"), false}}}};
    const QJsonObject migrated = PreferenceDocument::normalizePreferencesObject(legacy);
    expect(!migrated.value("ui").toObject().value("editor_auto_completion").toBool(true)
               && !migrated.value("ui").toObject().contains("editor_auto_close_brackets"),
           "legacy editor setting migrates at the shared document boundary");
    const QJsonObject canonical = PreferenceDocument::normalizePreferencesObject(
        withUi(legacy, QStringLiteral("editor_auto_completion"), true));
    expect(canonical.value("ui").toObject().value("editor_auto_completion").toBool(),
           "canonical editor setting wins over legacy value");

    // Facade consumers must agree even when a whole-document writer changes
    // settings after the typed accessors have already been used.
    QStandardPaths::setTestModeEnabled(true);
    QCoreApplication::setApplicationName(QStringLiteral("PreferenceRepositorySpec-%1")
                                           .arg(QCoreApplication::applicationPid()));
    const auto previousTheme = PreferenceDocument::preferredTheme();
    Q_UNUSED(previousTheme);
    QJsonObject facade = PreferenceDocument::loadPreferencesObject();
    facade = withUi(facade, QStringLiteral("theme"), QStringLiteral("light"));
    facade = withUi(facade, QStringLiteral("language"), QStringLiteral("ja"));
    facade = withUi(facade, QStringLiteral("dark_theme"), QStringLiteral("legacy"));
    expect(PreferenceDocument::savePreferencesObject(facade), "whole-document facade save");
    expect(PreferenceDocument::preferredTheme() == PreferenceDocument::ThemePreference::Light
               && PreferenceDocument::preferredLanguage() == PreferenceDocument::LanguagePreference::Japanese
               && PreferenceDocument::preferredDarkTheme() == PreferenceDocument::ThemePalette::Legacy,
           "typed getters read the shared document instead of independent caches");
    const QString facadePath = PreferenceDocument::preferencesFilePath();
    expect(QFile::remove(facadePath) && QDir().mkdir(facadePath), "block facade save");
    PreferenceDocument::setPreferredLanguageToken(QStringLiteral("en"));
    expect(PreferenceDocument::preferredLanguageToken() == "en"
               && PreferenceDocument::preferredLanguage() == PreferenceDocument::LanguagePreference::English,
           "typed and token getters agree after persistence failure");
    expect(QDir().rmdir(facadePath), "remove facade obstacle");
    expect(PreferenceDocument::savePreferencesObject(PreferenceDocument::loadPreferencesObject()),
           "facade retries pending write");
    QFile::remove(facadePath);
    miacode::debug_log::shutdownAsyncLogWriter();
    return ok ? 0 : 1;
}

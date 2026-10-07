#include "app/services/PreferenceDocument.h"
#include "app/services/PreferenceJsonFile.h"

#include <QCoreApplication>
#include <QJsonArray>
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
    expect(put(path, "{\"schema\":\"miacode_preferences_v1\",\"ui\":{\"theme\":\"light\"}}"), "external update fixture");
    expect(repository.snapshot() == initial, "ordinary reads retain the lifetime snapshot");
    expect(repository.replace(initial) && read(path).object != initial,
           "unchanged successfully persisted snapshot skips another write");
    Repository nextLifetime(path);
    expect(nextLifetime.snapshot().value("ui").toObject().value("theme") == "light"
               && nextLifetime.storedSchema() == "miacode_preferences_v1", "new repository observes file and original schema");

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

    int futureIndex = 0;
    for (const QJsonValue token : {QJsonValue(99), QJsonValue(0), QJsonValue(1.5), QJsonValue(QStringLiteral("miacode_preferences_v5")), QJsonValue(QStringLiteral("future")), QJsonValue(true), QJsonValue(QJsonValue::Null), QJsonValue(QJsonArray{1}), QJsonValue(QJsonObject{{QStringLiteral("v"), 2}})}) {
        QJsonObject raw{{QStringLiteral("schema"), token},
            {QStringLiteral("extensions"), QJsonObject{{QStringLiteral("future"), true}}},
            {QStringLiteral("ui_theme"), QStringLiteral("light")}};
        const QString futurePath = temporary.filePath(QStringLiteral("future-%1.json").arg(futureIndex++));
        const QByteArray original = QJsonDocument(raw).toJson();
        expect(put(futurePath, original), "unknown root fixture");
        Repository future(futurePath);
        expect(future.snapshot() == raw && PreferenceDocument::normalizePreferencesObject(raw) == raw,
               "unknown schema preserves raw keys and types without normalization");
        QJsonObject edited = withUi(raw, QStringLiteral("theme"), QStringLiteral("dark"));
        edited.insert(QStringLiteral("schema"), PreferenceDocument::currentPreferencesSchema());
        expect(!future.replace(edited) && future.isDirty()
                   && future.snapshot().value("schema") == token
                   && future.snapshot().value("ui").toObject().value("theme") == "dark",
               "unknown root accepts runtime edits but preserves schema and remains pending");
        QFile originalFile(futurePath);
        expect(originalFile.open(QIODevice::ReadOnly), "read protected root bytes");
        expect(!future.flush() && originalFile.readAll() == original,
               "unknown root cannot be overwritten or relabeled by a current-version caller");
    }
    QJsonObject futureNamed{{QStringLiteral("schema"), QStringLiteral("miacode_preferences_v5")},
                           {QStringLiteral("extensions"), 42}};
    expect(PreferenceDocument::normalizePreferencesObject(futureNamed) == futureNamed,
           "future named schema remains raw");

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

    const QString primaryPath = temporary.filePath(QStringLiteral("primary-unreadable.json"));
    const QByteArray primaryOriginal("{\"schema\":\"miacode_preferences_v4\",\"vendor\":{\"keep\":42},\"ui\":{\"theme\":\"light\"},\"app\":{\"other\":73}}");
    expect(QDir().mkdir(primaryPath), "block initial primary preference reading");
    Repository primaryFailure(primaryPath);
    QJsonObject primaryPending = withUi(primaryFailure.snapshot(), QStringLiteral("language"), QStringLiteral("en"));
    expect(!primaryFailure.replace(primaryPending) && primaryFailure.isDirty(),
           "unread primary accepts runtime edits while persistence stays dirty");
    expect(QDir().rmdir(primaryPath) && put(primaryPath, primaryOriginal), "restore unread primary original");
    expect(!primaryFailure.flush() && !primaryFailure.replace(primaryPending)
               && primaryFailure.isDirty() && primaryFailure.snapshot() == primaryPending,
           "recovered primary cannot be overwritten by a lifetime fallback snapshot");
    QFile primaryFile(primaryPath);
    expect(primaryFile.open(QIODevice::ReadOnly) && primaryFile.readAll() == primaryOriginal,
           "blocked save preserves complete original primary bytes");
    primaryFile.close();
    Repository recoveredPrimary(primaryPath);
    const QJsonObject recoveredRoot = recoveredPrimary.snapshot();
    expect(recoveredRoot.value("vendor").toObject().value("keep") == 42
               && recoveredRoot.value("app").toObject().value("other") == 73
               && recoveredRoot.value("ui").toObject().value("theme") == "light",
           "new repository loads unread original fields after IO recovery");
    expect(recoveredPrimary.replace(withUi(recoveredRoot, QStringLiteral("language"), QStringLiteral("ja")))
               && read(primaryPath).object.value("vendor").toObject().value("keep") == 42,
           "new repository can persist edits without losing original fields");

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

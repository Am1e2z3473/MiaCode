#include "app/services/ProjectPreferences.h"
#include "app/services/PreferenceJsonFile.h"

#include "audio/WaveformCache.h"

#include <QDir>
#include <QFileInfo>
#include <QMutex>
#include <QMutexLocker>
#include <QSet>

namespace miacode::project_preferences {
namespace {
QMutex unreadPathsMutex;
QSet<QString> unreadPaths;

QString normalizedStoragePath(const QString& chartFilePath)
{
    const QString path = projectPreferencesFilePath(chartFilePath);
    return path.isEmpty() ? QString() : QDir::cleanPath(QFileInfo(path).absoluteFilePath());
}
}

QString projectPreferencesFilePath(const QString& chartFilePath)
{
    const QString projectDataDir = miacode::waveform::projectDataDirectoryPathForFile(chartFilePath);
    if (projectDataDir.isEmpty()) {
        return QString();
    }
    return QDir(projectDataDir).filePath(QStringLiteral("preferences.json"));
}

QJsonObject load(const QString& chartFilePath)
{
    const QString path = normalizedStoragePath(chartFilePath);
    const auto result = preference_json_file::read(path);
    if (result.status == preference_json_file::ReadStatus::ReadError) {
        const QMutexLocker lock(&unreadPathsMutex);
        unreadPaths.insert(path);
    } else if (result.status == preference_json_file::ReadStatus::Corrupt) {
        preference_json_file::write(path, {});
    }
    return result.object;
}

bool save(const QString& chartFilePath, const QJsonObject& preferences)
{
    const QString path = normalizedStoragePath(chartFilePath);
    const QMutexLocker lock(&unreadPathsMutex);
    if (unreadPaths.contains(path)) {
        // Callers can retain fallback compound values (e.g. the project mixer)
        // even after a later read succeeds. Preserve the unread original until
        // the next process lifetime rather than saving those partial values.
        preference_json_file::log(path, QStringLiteral("project-read-unavailable save-blocked-for-session"));
        return false;
    }
    return !path.isEmpty() && preference_json_file::write(path, preferences);
}

}  // namespace miacode::project_preferences

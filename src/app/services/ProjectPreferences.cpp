#include "app/services/ProjectPreferences.h"
#include "app/services/PreferenceJsonFile.h"

#include "audio/WaveformCache.h"

#include <QDir>

namespace miacode::project_preferences {

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
    return preference_json_file::load(projectPreferencesFilePath(chartFilePath));
}

bool save(const QString& chartFilePath, const QJsonObject& preferences)
{
    const QString path = projectPreferencesFilePath(chartFilePath);
    return !path.isEmpty() && preference_json_file::write(path, preferences);
}

}  // namespace miacode::project_preferences

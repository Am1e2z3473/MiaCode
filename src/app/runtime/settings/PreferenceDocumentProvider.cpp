#include "app/runtime/settings/PreferenceDocumentProvider.h"

#include "app/services/PreferenceDocument.h"
#include "app/runtime/settings/EncoderProbeCache.h"

#include <QDir>
#include <QFileInfo>

namespace miacode::runtime {
namespace {

constexpr auto kAppSectionKey = "app";

}  // namespace

QJsonObject PreferenceDocumentProvider::appSection(const QString& name) const
{
    const QJsonObject root = PreferenceDocument::loadPreferencesObject();
    const QJsonObject app = root.value(QLatin1String(kAppSectionKey)).toObject();
    return app.value(name).toObject();
}

bool PreferenceDocumentProvider::setAppSection(const QString& name, const QJsonObject& section)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject app = root.value(QLatin1String(kAppSectionKey)).toObject();
    app.insert(name, section);
    root.insert(QLatin1String(kAppSectionKey), app);
    return PreferenceDocument::savePreferencesObject(root);
}

QString PreferenceDocumentProvider::preferencesDirectoryPath() const
{
    return QFileInfo(PreferenceDocument::preferencesFilePath()).absoluteDir().absolutePath();
}

QString PreferenceDocumentProvider::resolvedLanguageToken() const
{
    return PreferenceDocument::resolvedLanguageToken();
}

void installPreferenceDocumentProvider()
{
    static PreferenceDocumentProvider provider;
    miacode::video_export::installEncoderProbeCache(
        std::make_shared<SettingsEncoderProbeCache>());
    miacode::preferences::installPreferenceProvider(&provider);
}

}  // namespace miacode::runtime

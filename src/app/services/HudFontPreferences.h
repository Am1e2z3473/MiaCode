#pragma once

#include "app/services/PreferenceDocument.h"
#include "core/scene/PreviewHudState.h"

#include <QFileInfo>

namespace miacode::hud_preferences {

inline QString areaKey(preview::scene::PreviewHudFontArea area)
{
    static const char* keys[] = {"chart_info", "timestamp", "center_display", "object_stats", "debug_info"};
    return QString::fromLatin1(keys[static_cast<int>(area)]);
}

inline QString validPath(const QString& path)
{
    if (path.isEmpty()) return {};
    const QFileInfo file(path);
    const QString suffix = file.suffix().toLower();
    return file.isFile() && (suffix == QLatin1String("ttf") || suffix == QLatin1String("otf"))
        ? file.absoluteFilePath() : QString();
}

inline preview::scene::PreviewHudFontSettings fromSection(const QJsonObject& section)
{
    preview::scene::PreviewHudFontSettings settings;
    const QJsonObject paths = section.value(QStringLiteral("hud_font_paths")).toObject();
    const QString legacy = validPath(section.value(QStringLiteral("hud_font_path")).toString());
    for (const auto& choice : preview::scene::previewHudFontAreaChoices()) {
        const QString key = areaKey(choice.area);
        // A present empty value explicitly selects this area's bundled default.
        settings.paths[static_cast<int>(choice.area)] = paths.contains(key)
            ? validPath(paths.value(key).toString()) : legacy;
    }
    return settings;
}

inline QJsonObject section()
{
    return PreferenceDocument::loadPreferencesObject().value(QStringLiteral("app")).toObject()
        .value(QStringLiteral("video_export")).toObject();
}

inline preview::scene::PreviewHudFontSettings load()
{
    return fromSection(section());
}

inline bool setPath(preview::scene::PreviewHudFontArea area, const QString& path)
{
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject app = root.value(QStringLiteral("app")).toObject();
    QJsonObject video = app.value(QStringLiteral("video_export")).toObject();
    QJsonObject paths = video.value(QStringLiteral("hud_font_paths")).toObject();
    paths.insert(areaKey(area), path.isEmpty() ? QString() : QFileInfo(path).absoluteFilePath());
    video.insert(QStringLiteral("hud_font_paths"), paths);
    app.insert(QStringLiteral("video_export"), video);
    root.insert(QStringLiteral("app"), app);
    return PreferenceDocument::savePreferencesObject(root);
}

} // namespace miacode::hud_preferences

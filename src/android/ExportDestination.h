#pragma once
#include <QCryptographicHash>
#include <QDir>
#include <QFileInfo>
#include <QJsonObject>
#include <QSettings>

namespace miacode::android {
inline QString exportDestinationKey(const QString& path) {
    const auto absolute = QDir::cleanPath(QFileInfo(path).absoluteFilePath());
    return QStringLiteral("mobile/export-destinations/")
        + QString::fromLatin1(QCryptographicHash::hash(absolute.toUtf8(), QCryptographicHash::Sha256).toHex());
}
inline void rememberExportDestination(const QString& localPath, const QString& uri, bool tree, const QString& displayPath = {}) {
    QSettings settings;
    settings.setValue(exportDestinationKey(localPath), QVariantMap{{"uri", uri}, {"tree", tree}, {"displayPath", displayPath}});
}
inline QJsonObject exportDestination(const QString& localPath) {
    QSettings settings;
    const QString absolute = QDir::cleanPath(QFileInfo(localPath).absoluteFilePath());
    QString cursor = absolute;
    for (;;) {
        const auto target = settings.value(exportDestinationKey(cursor)).toMap();
        if (!target.isEmpty() && (cursor == absolute || target.value("tree").toBool())) {
            auto result = QJsonObject::fromVariantMap(target);
            result.insert("relativePath", target.value("tree").toBool() ? QDir(cursor).relativeFilePath(absolute) : QString());
            return result;
        }
        const auto parent = QFileInfo(cursor).absolutePath();
        if (parent == cursor) return {};
        cursor = parent;
    }
}
inline QString exportDestinationDisplayPath(const QString& localPath) {
    const auto target = exportDestination(localPath);
    if (target.isEmpty()) return localPath;
    const auto label = target.value("displayPath").toString();
    if (label.isEmpty()) return QFileInfo(localPath).fileName();
    if (!target.value("tree").toBool()) return label;
    const auto relative = target.value("relativePath").toString();
    return relative == QStringLiteral(".") ? label : label + QLatin1Char('/') + relative;
}
}

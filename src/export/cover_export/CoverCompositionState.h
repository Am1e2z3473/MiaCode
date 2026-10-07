#pragma once

#include <QJsonObject>
#include <QList>
#include <QSize>
#include <QStringList>

namespace miacode::cover_export {

class CoverLayoutModel;

struct CoverCompositionState {
    static constexpr int kCurrentVersion = 3;

    QSize size;
    QJsonObject background;
    QJsonObject card;
    QJsonObject layout;
    // Where the rendered cover is written. Remembered like the rest of the
    // composition so switching difficulty (which re-seeds size and card inputs
    // from the chart) cannot silently drop the folder the user picked. Empty
    // means "not chosen yet" and writes no key — presets stay machine-agnostic.
    QString outputDirectory;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& root, CoverCompositionState* out, QString* errorMessage = nullptr);
    static bool supportsVersion(const QJsonObject& root);
    static QJsonObject migrateToCurrent(const QJsonObject& root);

};

}  // namespace miacode::cover_export

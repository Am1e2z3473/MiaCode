#pragma once

#include <QJsonObject>
#include <QList>
#include <QSize>
#include <QStringList>

namespace miacode::cover_export {

class CoverLayoutModel;

struct CoverCompositionState {
    static constexpr int kCurrentVersion = 3;
    static constexpr char kDefaultOutputFile[] = "card.jpg";

    QSize size;
    QJsonObject background;
    QJsonObject card;
    QJsonObject layout;
    // The file the rendered cover is written to: a bare name ("card.jpg"), a
    // path relative to the chart folder, or an absolute path. Remembered like
    // the rest of the composition so switching difficulty (which re-seeds size
    // and card inputs from the chart) cannot silently drop what the user typed.
    // Empty means "not chosen yet" and writes no key — presets stay
    // machine-agnostic.
    QString outputFile;

    QJsonObject toJson() const;
    static bool fromJson(const QJsonObject& root, CoverCompositionState* out, QString* errorMessage = nullptr);
    static bool supportsVersion(const QJsonObject& root);
    static QJsonObject migrateToCurrent(const QJsonObject& root);

};

}  // namespace miacode::cover_export

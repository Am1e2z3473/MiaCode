#pragma once

#include <QJsonObject>

#include "common/PreferenceProvider.h"

namespace miacode::video_export {

// Bump when a stored preference needs to be reinterpreted. Absent means a
// pre-versioned (v1) object. Migrations run in migrateDialogPreferences().
inline constexpr int kDialogPreferencesSchemaVersion = 2;

// Bring a loaded preferences object up to the current schema. Kept centralized
// so every load site (single dialog + embedded batch settings panel) migrates
// identically. In-memory only; the bumped version is persisted on the next
// saveDialogPreferences().
inline QJsonObject migrateDialogPreferences(QJsonObject preferences)
{
    const int version = preferences.value(QStringLiteral("schema_version")).toInt(1);

    // v1 -> v2: the intro card type gained an "Auto" default that detects SD/DX
    // per chart. Legacy objects could only persist a concrete "Standard"/"DX",
    // which would otherwise pin the new default. Drop it so the combo falls back
    // to its "auto" construction default.
    if (version < 2) {
        preferences.remove(QStringLiteral("intro_card_type"));
    }

    preferences.insert(QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion);
    return preferences;
}

inline QJsonObject loadDialogPreferences()
{
    return migrateDialogPreferences(
        miacode::preferences::appPreferenceSection(QStringLiteral("video_export")));
}

inline bool saveDialogPreferences(const QJsonObject& preferences)
{
    QJsonObject stamped = preferences;
    stamped.insert(QStringLiteral("schema_version"), kDialogPreferencesSchemaVersion);
    return miacode::preferences::setAppPreferenceSection(QStringLiteral("video_export"), stamped);
}

}  // namespace miacode::video_export

#pragma once

#include <QJsonObject>
#include <QString>

namespace miacode::preferences {

// Port through which library code reaches user preferences. Libraries never
// open preferences.json themselves: they read and replace the `app.<section>`
// object they own (for example "video_export" or "cover_export") and receive
// plain values such as the resolved UI language. The file location, schema
// normalization and the write itself belong to the provider the application
// installs at process start (see src/app/runtime/settings/).
class PreferenceProvider
{
public:
    virtual ~PreferenceProvider() = default;

    // The persisted `app.<name>` object, or an empty object.
    virtual QJsonObject appSection(const QString& name) const = 0;
    // Replace `app.<name>` and persist the document. False when the write failed.
    virtual bool setAppSection(const QString& name, const QJsonObject& section) = 0;
    // Directory that holds the preferences document; user data such as the
    // export font library lives beside it.
    virtual QString preferencesDirectoryPath() const = 0;
    // Effective UI language token ("en_US", "zh_CN", "ja_JP", ...).
    virtual QString resolvedLanguageToken() const = 0;

protected:
    PreferenceProvider() = default;
    PreferenceProvider(const PreferenceProvider&) = default;
    PreferenceProvider& operator=(const PreferenceProvider&) = default;
};

// Installs the process-wide provider (not owned). Call once from each process
// entry point before library code runs; nullptr uninstalls.
void installPreferenceProvider(PreferenceProvider* provider);
PreferenceProvider* preferenceProvider();

// Convenience accessors. Without an installed provider they return empty
// values and report failed writes, so library code stays usable in tests and
// in hosts that keep no preferences.
QJsonObject appPreferenceSection(const QString& name);
bool setAppPreferenceSection(const QString& name, const QJsonObject& section);
QString preferencesDirectoryPath();
QString resolvedLanguageToken();

}  // namespace miacode::preferences

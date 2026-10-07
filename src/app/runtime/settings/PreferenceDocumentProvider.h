#pragma once

#include "common/PreferenceProvider.h"

namespace miacode::runtime {

// Production preference port: every library read and write goes through
// PreferenceDocument, so preferences.json keeps its location, normalization
// and write timing.
class PreferenceDocumentProvider final : public miacode::preferences::PreferenceProvider
{
public:
    QJsonObject appSection(const QString& name) const override;
    bool setAppSection(const QString& name, const QJsonObject& section) override;
    QString preferencesDirectoryPath() const override;
    QString resolvedLanguageToken() const override;
};

// Installs the process-wide PreferenceDocumentProvider. The GUI entry and the
// video export entries (CLI export and export worker) call it before any
// library code reads preferences.
void installPreferenceDocumentProvider();

}  // namespace miacode::runtime

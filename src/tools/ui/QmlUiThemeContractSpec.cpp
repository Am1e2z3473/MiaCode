// Contract regression for how a theme change reaches the shell.
//
// Reported symptom: changing 主题 in 偏好设置 only repainted the timeline.
//
// Cause: there are two ways the resolved theme can change, and only one of them
// told QML about it.
//
//   * the OS changes its colour scheme -> QStyleHints::colorSchemeChanged ->
//     WorkbenchSettings::reloadTheme() -> darkTheme_ updated -> themeChanged()
//     -> every QML component rebinds.
//   * the user picks a theme -> WorkbenchSettings::setThemeModeToken()/setLightThemeToken()/setDarkThemeToken() ->
//     PreferenceDocument::setPreferredTheme() ... and nothing else. PreferenceDocument only stores and
//     persists the preference; it notifies no one.
//
// The timeline still followed the user's choice because it is a C++ QSG item
// that reads UiTheme::colors() — derived live from the stored preference — on
// its next repaint. It never goes through the QML darkTheme property. So the one
// surface that bypassed the notification was the only one that updated.
//
// This is a source contract rather than a behavioural one: WorkbenchSettings pulls
// in MainWindowShared for its editor font metrics, so it cannot be linked into a
// Core-only spec. The check is narrow on purpose — it pins that the
// user-initiated path ends in the same notification the OS-initiated path uses.

#include <QCoreApplication>
#include <QFile>
#include <QString>
#include <QTextStream>

#ifndef MIACODE_SOURCE_ROOT
#error "MIACODE_SOURCE_ROOT must be defined (repo root absolute path)"
#endif

namespace {

bool require(bool condition, const QString& message, QTextStream& err)
{
    if (!condition) {
        err << "FAIL: " << message << Qt::endl;
    }
    return condition;
}

QString readFile(const QString& relativePath)
{
    QFile file(QStringLiteral(MIACODE_SOURCE_ROOT) + QLatin1Char('/') + relativePath);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return QString();
    }
    return QString::fromUtf8(file.readAll());
}

// The body of one function, from its definition to the next one.
QString functionBody(const QString& source, const QString& signature)
{
    const qsizetype start = source.indexOf(signature);
    if (start < 0) {
        return QString();
    }
    const qsizetype end = source.indexOf(QStringLiteral("\n}\n"), start);
    return source.mid(start, (end >= 0 ? end : source.size()) - start);
}

}  // namespace

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QTextStream err(stderr);

    const QString settings = readFile(QStringLiteral("src/app/ui/layout/WorkbenchSettings.cpp"));
    const QString settingsHeader = readFile(QStringLiteral("src/app/ui/layout/WorkbenchSettings.h"));
    const QString themeQml = readFile(QStringLiteral("src/app/ui/theme/Theme.qml"));
    const QString preferencesStore = readFile(QStringLiteral("src/app/services/PreferenceDocument.cpp"));
    bool ok = require(!settings.isEmpty() && !settingsHeader.isEmpty() && !themeQml.isEmpty()
                          && !preferencesStore.isEmpty(),
                      QStringLiteral("the settings, Theme.qml and PreferenceDocument sources are readable"), err);
    if (!ok) {
        return 1;
    }

    // The premise: PreferenceDocument stores the preference and notifies nobody. If that
    // ever changes, this whole contract can be reconsidered — but while it
    // holds, every writer of the preference owes the shell a notification.
    const QString setPreferredTheme =
        functionBody(preferencesStore, QStringLiteral("void PreferenceDocument::setPreferredTheme("));
    ok &= require(!setPreferredTheme.isEmpty()
                      && !setPreferredTheme.contains(QStringLiteral("emit ")),
                  QStringLiteral("PreferenceDocument::setPreferredTheme still only stores and persists, "
                                 "which is why its callers must publish the change"), err);

    // The OS-initiated path.
    ok &= require(settings.contains(QStringLiteral("QStyleHints::colorSchemeChanged"))
                      && settings.contains(QStringLiteral("&WorkbenchSettings::reloadTheme")),
                  QStringLiteral("an OS colour-scheme change still reloads the theme"), err);

    const QString reloadTheme =
        functionBody(settings, QStringLiteral("void WorkbenchSettings::reloadTheme()"));
    ok &= require(reloadTheme.contains(QStringLiteral("darkTheme_ = next"))
                      && reloadTheme.contains(QStringLiteral("emit themeChanged()")),
                  QStringLiteral("reloadTheme updates the bound appearance and publishes it"), err);

    // Changing a palette within the same appearance must also notify QML.
    const struct { const char* published; const char* next; } tokenSlots[] = {
        {"publishedThemeModeToken_", "nextModeToken"},
        {"publishedLightThemeToken_", "nextLightThemeToken"},
        {"publishedDarkThemeToken_", "nextDarkThemeToken"},
    };
    for (const auto& slot : tokenSlots) {
        const QString published = QString::fromLatin1(slot.published);
        const QString next = QString::fromLatin1(slot.next);
        ok &= require(reloadTheme.contains(published + QStringLiteral(" == ") + next)
                          && reloadTheme.contains(published + QStringLiteral(" = ") + next),
                      published + QStringLiteral(" participates in change detection and publication"), err);
    }
    for (const char* property : {"themeModeToken", "lightThemeToken", "darkThemeToken", "activeThemeToken"}) {
        const QString name = QString::fromLatin1(property);
        ok &= require(settingsHeader.contains(name + QStringLiteral(" READ ") + name
                                                  + QStringLiteral(" NOTIFY themeChanged")),
                      name + QStringLiteral(" is bindable through the shared theme notification"), err);
    }
    const QString activeTheme =
        functionBody(settings, QStringLiteral("QString WorkbenchSettings::activeThemeToken() const"));
    ok &= require(activeTheme.contains(QStringLiteral("ThemeVariantResolver::resolve("))
                      && activeTheme.contains(QStringLiteral("darkAppearance ? darkThemeToken() : lightThemeToken()")),
                  QStringLiteral("resolved appearance selects the user's dark or light palette slot"), err);
    ok &= require(themeQml.contains(QStringLiteral("\"legacy\": { id: \"legacy\", dark: true, colors: legacyDarkColors }"))
                      && themeQml.contains(QStringLiteral("\"legacy_light\": { id: \"legacy_light\", dark: false, colors: legacyLightColors }"))
                      && themeQml.contains(QStringLiteral("preferences.activeThemeToken"))
                      && themeQml.contains(QStringLiteral("themeCatalog[activeThemeToken]"))
                      && themeQml.contains(QStringLiteral("colors: activeTheme.colors")),
                  QStringLiteral("QML binds the active palette through the catalog, with independent legacy palettes"), err);

    // Every user writer must finish at the same notification as the OS path.
    const struct { const char* setter; const char* store; } writers[] = {
        {"setThemeModeToken", "setPreferredTheme"},
        {"setLightThemeToken", "setPreferredLightTheme"},
        {"setDarkThemeToken", "setPreferredDarkTheme"},
    };
    for (const auto& writer : writers) {
        const QString setter = QString::fromLatin1(writer.setter);
        const QString body = functionBody(settings, QStringLiteral("void WorkbenchSettings::") + setter + QLatin1Char('('));
        ok &= require(body.contains(QStringLiteral("PreferenceDocument::")
                                        + QString::fromLatin1(writer.store) + QStringLiteral("(next)"))
                          && body.contains(QStringLiteral("reloadTheme()")),
                      setter + QStringLiteral(" stores its own preference and publishes the theme"), err);
    }
    if (ok) {
        QTextStream(stdout) << "qml_ui_theme_contract_spec: OK" << Qt::endl;
    }
    return ok ? 0 : 1;
}

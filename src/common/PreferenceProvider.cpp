#include "common/PreferenceProvider.h"

#include <atomic>

namespace miacode::preferences {
namespace {

std::atomic<PreferenceProvider*>& installedProvider()
{
    static std::atomic<PreferenceProvider*> provider{nullptr};
    return provider;
}

}  // namespace

void installPreferenceProvider(PreferenceProvider* provider)
{
    installedProvider().store(provider, std::memory_order_release);
}

PreferenceProvider* preferenceProvider()
{
    return installedProvider().load(std::memory_order_acquire);
}

QJsonObject appPreferenceSection(const QString& name)
{
    const PreferenceProvider* provider = preferenceProvider();
    return provider != nullptr ? provider->appSection(name) : QJsonObject();
}

bool setAppPreferenceSection(const QString& name, const QJsonObject& section)
{
    PreferenceProvider* provider = preferenceProvider();
    return provider != nullptr && provider->setAppSection(name, section);
}

QString preferencesDirectoryPath()
{
    const PreferenceProvider* provider = preferenceProvider();
    return provider != nullptr ? provider->preferencesDirectoryPath() : QString();
}

QString resolvedLanguageToken()
{
    const PreferenceProvider* provider = preferenceProvider();
    return provider != nullptr ? provider->resolvedLanguageToken() : QString();
}

}  // namespace miacode::preferences

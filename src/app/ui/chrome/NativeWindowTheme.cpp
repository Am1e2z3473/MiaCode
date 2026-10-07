#include "app/ui/chrome/NativeWindowTheme.h"

#include "app/ui/theme/UiTheme.h"

#ifdef Q_OS_MACOS
#include "app/ui/chrome/NativeWindowThemeMac.h"
#endif

#include <QWindow>

#ifdef Q_OS_WIN
#include <windows.h>
#endif

namespace NativeWindowTheme {
namespace {

#ifdef Q_OS_WIN
constexpr DWORD kDwmwaUseImmersiveDarkMode = 20;
constexpr DWORD kDwmwaSystemBackdropType = 38;
constexpr int kDwmsbtNone = 1;
constexpr int kDwmsbtMainWindow = 2;
constexpr int kDwmsbtTransientWindow = 3;

bool setDwmWindowAttribute(HWND hwnd, DWORD attribute, const void* value, DWORD size)
{
    if (hwnd == nullptr || value == nullptr || size == 0) {
        return false;
    }
    static HMODULE dwmapiModule = ::LoadLibraryW(L"dwmapi.dll");
    if (dwmapiModule == nullptr) {
        return false;
    }
    using DwmSetWindowAttributeFn = HRESULT(WINAPI*)(HWND, DWORD, LPCVOID, DWORD);
    static auto setWindowAttribute = reinterpret_cast<DwmSetWindowAttributeFn>(
        ::GetProcAddress(dwmapiModule, "DwmSetWindowAttribute")
    );
    if (setWindowAttribute == nullptr) {
        return false;
    }
    return SUCCEEDED(setWindowAttribute(hwnd, attribute, value, size));
}

void applyAppearanceToNativeHandle(HWND hwnd)
{
    if (hwnd == nullptr) {
        return;
    }

    const BOOL darkMode = UiTheme::isDarkTheme() ? TRUE : FALSE;
    setDwmWindowAttribute(hwnd, kDwmwaUseImmersiveDarkMode, &darkMode, sizeof(darkMode));

}

bool applyBackdropToNativeHandle(HWND hwnd, bool backdropEnabled, BackdropMaterial material)
{
    const int backdropType = backdropEnabled
        ? (material == BackdropMaterial::Acrylic ? kDwmsbtTransientWindow : kDwmsbtMainWindow)
        : kDwmsbtNone;
    const bool backdropApplied = setDwmWindowAttribute(hwnd, kDwmwaSystemBackdropType, &backdropType, sizeof(backdropType));

    return backdropEnabled && backdropApplied;
}

#endif  // Q_OS_WIN

}  // namespace

void applyAppearanceToWindow(QWindow* window)
{
    if (window == nullptr) {
        return;
    }
#ifdef Q_OS_WIN
    const HWND hwnd = reinterpret_cast<HWND>(window->winId());
    applyAppearanceToNativeHandle(hwnd);
#elif defined(Q_OS_MACOS)
    NativeWindowThemeMac::applyToNativeView(
        reinterpret_cast<void*>(window->winId()),
        UiTheme::isDarkTheme()
            ? NativeWindowThemePolicy::Appearance::Dark
            : NativeWindowThemePolicy::Appearance::Light);
#else
    Q_UNUSED(window);
#endif
}

bool applyToWindow(QWindow* window, bool backdropEnabled, BackdropMaterial material)
{
    if (window == nullptr) {
        return false;
    }
    applyAppearanceToWindow(window);
#ifdef Q_OS_WIN
    return applyBackdropToNativeHandle(reinterpret_cast<HWND>(window->winId()), backdropEnabled, material);
#else
    Q_UNUSED(backdropEnabled);
    Q_UNUSED(material);
#endif
    return false;
}

}  // namespace NativeWindowTheme

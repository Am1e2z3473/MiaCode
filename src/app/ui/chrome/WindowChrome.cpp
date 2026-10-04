#include "chrome/WindowChrome.h"
#include "chrome/NativeWindowTheme.h"
#include "preferences/PreferenceDocument.h"
#include "common/DebugLog.h"

#include <QtGlobal>
#include <QCoreApplication>
#include <QGuiApplication>
#include <QJsonObject>
#include <QPlatformSurfaceEvent>
#include <QPointer>
#include <QScreen>
#include <QWindow>

#ifdef Q_OS_WIN
#include <dwmapi.h>
#include <windows.h>
#include <windowsx.h>
#endif

namespace miacode::ui {

WindowChrome::WindowChrome(QObject* parent)
    : QObject(parent)
{
    // Capture after the geometry and window-state events of a transition settle.
    stateCaptureTimer_.setSingleShot(true);
    stateCaptureTimer_.setInterval(0);
    connect(&stateCaptureTimer_, &QTimer::timeout, this, &WindowChrome::captureWindowState);
    materialUpdateTimer_.setSingleShot(true);
    materialUpdateTimer_.setInterval(0);
    connect(&materialUpdateTimer_, &QTimer::timeout, this, &WindowChrome::refreshNativeMaterial);
}

WindowChrome::~WindowChrome()
{
    stopObservingMacOsFullScreen();
    releaseMacOsMaterial();
    if (QCoreApplication::instance() != nullptr) {
        QCoreApplication::instance()->removeNativeEventFilter(this);
    }
}

void WindowChrome::setTitleBarLeadingInset(qreal inset)
{
    if (qFuzzyCompare(titleBarLeadingInset_, inset)) {
        return;
    }
    titleBarLeadingInset_ = inset;
    emit titleBarLeadingInsetChanged();
}

void WindowChrome::setTitleBarHeight(qreal height)
{
    if (qFuzzyCompare(titleBarHeight_, height)) {
        return;
    }
    titleBarHeight_ = height;
    emit titleBarHeightChanged();
}

void WindowChrome::setNativeMaterialAvailable(bool available)
{
    if (nativeMaterialAvailable_ == available) {
        return;
    }
    nativeMaterialAvailable_ = available;
    emit nativeMaterialAvailableChanged();
}

void WindowChrome::attach(QWindow* window)
{
    if (window == nullptr) {
        return;
    }

    window_ = window;
    restoreWindowState();
    const auto scheduleCapture = [this]() { stateCaptureTimer_.start(); };
    connect(window, &QWindow::xChanged, this, scheduleCapture);
    connect(window, &QWindow::yChanged, this, scheduleCapture);
    connect(window, &QWindow::widthChanged, this, scheduleCapture);
    connect(window, &QWindow::heightChanged, this, scheduleCapture);
    connect(window, &QWindow::windowStateChanged, this, scheduleCapture);
    connect(window, &QWindow::visibilityChanged, this, scheduleCapture);
    connect(window, &QWindow::screenChanged, this, scheduleCapture);

#ifdef Q_OS_WIN
    nativeHandle_ = window->winId();
    const auto handle = reinterpret_cast<HWND>(nativeHandle_);
    window->installEventFilter(this);
    QCoreApplication::instance()->installNativeEventFilter(this);
    refreshNativeMaterial();

    SetWindowPos(
        handle,
        nullptr,
        0,
        0,
        0,
        0,
        SWP_FRAMECHANGED | SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
    setTitleBarLeadingInset(0);
#elif defined(Q_OS_MACOS)
    window->installEventFilter(this);
    applyMacOs(window);
    observeMacOsFullScreen(window);
    QObject::connect(
        window,
        &QWindow::windowStateChanged,
        this,
        [this](Qt::WindowState state) {
            if (state == Qt::WindowFullScreen) {
                setTitleBarLeadingInset(0);
                return;
            }
            setTitleBarLeadingInset(windowedTitleBarLeadingInset_);
            setTitleBarHeight(windowedTitleBarHeight_);
        },
        static_cast<Qt::ConnectionType>(Qt::UniqueConnection));
#else
    Q_UNUSED(window);
    setTitleBarLeadingInset(0);
#endif
}

void WindowChrome::minimize()
{
    if (window_.isNull()) {
        return;
    }
    captureWindowState();
    window_->showMinimized();
}

void WindowChrome::toggleMaximized()
{
    if (window_.isNull()) {
        return;
    }
    if (window_->windowStates().testFlag(Qt::WindowMaximized)) {
        window_->showNormal();
    } else {
        window_->showMaximized();
    }
}

void WindowChrome::setBlurMaterialsEnabled(bool enabled)
{
    if (blurMaterialsEnabled_ == enabled) {
        return;
    }
    blurMaterialsEnabled_ = enabled;
    if (window_.isNull()) {
        return;
    }
    refreshNativeMaterial();
}

void WindowChrome::refreshNativeTheme()
{
    NativeWindowTheme::applyAppearanceToWindow(window_.data());
}

void WindowChrome::refreshNativeMaterial()
{
    if (window_.isNull()) {
        return;
    }
#ifdef Q_OS_WIN
    nativeHandle_ = window_->winId();
    const bool frameApplied = extendDwmFrame();
    const bool backdropApplied = NativeWindowTheme::applyToWindow(
        window_.data(), blurMaterialsEnabled_, NativeWindowTheme::BackdropMaterial::Acrylic);
    setNativeMaterialAvailable(frameApplied && backdropApplied);
#elif defined(Q_OS_MACOS)
    applyMacOs(window_.data());
#endif
}

bool WindowChrome::eventFilter(QObject* watched, QEvent* event)
{
#if defined(Q_OS_WIN) || defined(Q_OS_MACOS)
    if (watched == window_.data()) {
        if (event->type() == QEvent::Show) {
            // Apply after Qt has completed the native show operation.
            materialUpdateTimer_.start();
        } else if (event->type() == QEvent::PlatformSurface) {
            const auto* surfaceEvent = static_cast<QPlatformSurfaceEvent*>(event);
            if (surfaceEvent->surfaceEventType() == QPlatformSurfaceEvent::SurfaceCreated) {
                materialUpdateTimer_.start();
            } else {
                materialUpdateTimer_.stop();
#ifdef Q_OS_MACOS
                releaseMacOsMaterial();
#endif
                nativeHandle_ = 0;
                setNativeMaterialAvailable(false);
            }
        }
    }
#endif
    return QObject::eventFilter(watched, event);
}

void WindowChrome::restoreWindowState()
{
    const QJsonObject saved = PreferenceDocument::loadPreferencesObject()
        .value(QStringLiteral("ui")).toObject()
        .value(QStringLiteral("main_window")).toObject();
    const QJsonObject geometry = saved.value(QStringLiteral("normal_geometry")).toObject();
    normalGeometry_ = QRect(geometry.value(QStringLiteral("x")).toInt(),
                            geometry.value(QStringLiteral("y")).toInt(),
                            geometry.value(QStringLiteral("width")).toInt(),
                            geometry.value(QStringLiteral("height")).toInt());
    if (!normalGeometry_.isValid()) {
        normalGeometry_ = window_->geometry();
        screenName_ = window_->screen() ? window_->screen()->name() : QString();
        return;
    }

    // Wayland's compositor owns top-level placement; restore only size and state.
    const bool canRestorePosition = !QGuiApplication::platformName().startsWith(QStringLiteral("wayland"));
    QScreen* screen = nullptr;
    if (canRestorePosition) {
        const QString savedScreenName = saved.value(QStringLiteral("screen_name")).toString();
        for (QScreen* candidate : QGuiApplication::screens()) {
            if (candidate->name() == savedScreenName) {
                screen = candidate;
                break;
            }
        }
        if (screen == nullptr) {
            screen = QGuiApplication::screenAt(normalGeometry_.center());
        }
    }
    if (screen == nullptr) {
        screen = window_->screen();
    }
    if (screen != nullptr) {
        const QRect available = screen->availableGeometry();
        normalGeometry_.setSize(QSize(
            qBound(qMin(window_->minimumWidth(), available.width()), normalGeometry_.width(), available.width()),
            qBound(qMin(window_->minimumHeight(), available.height()), normalGeometry_.height(), available.height())));
        if (canRestorePosition) {
            normalGeometry_.moveLeft(qBound(available.left(), normalGeometry_.left(),
                                           available.right() - normalGeometry_.width() + 1));
            normalGeometry_.moveTop(qBound(available.top(), normalGeometry_.top(),
                                          available.bottom() - normalGeometry_.height() + 1));
            window_->setScreen(screen);
        }
        screenName_ = screen->name();
    }
    if (canRestorePosition) {
        window_->setGeometry(normalGeometry_);
    } else {
        window_->resize(normalGeometry_.size());
    }
    maximized_ = saved.value(QStringLiteral("maximized")).toBool();
    if (maximized_) {
        window_->setWindowStates(Qt::WindowMaximized);
    }
}

void WindowChrome::captureWindowState()
{
    if (window_.isNull() || !window_->isVisible()) {
        return;
    }
    const Qt::WindowStates states = window_->windowStates();
    if (states.testFlag(Qt::WindowMinimized) || states.testFlag(Qt::WindowFullScreen)) {
        return;
    }
    maximized_ = states.testFlag(Qt::WindowMaximized);
    if (!maximized_) {
        normalGeometry_ = window_->geometry();
    }
    if (window_->screen() != nullptr) {
        screenName_ = window_->screen()->name();
    }
}

void WindowChrome::saveWindowState()
{
    if (window_.isNull()) {
        return;
    }
    captureWindowState();
    QJsonObject root = PreferenceDocument::loadPreferencesObject();
    QJsonObject ui = root.value(QStringLiteral("ui")).toObject();
    ui.insert(QStringLiteral("main_window"), QJsonObject{
        {QStringLiteral("normal_geometry"), QJsonObject{
             {QStringLiteral("x"), normalGeometry_.x()},
             {QStringLiteral("y"), normalGeometry_.y()},
             {QStringLiteral("width"), normalGeometry_.width()},
             {QStringLiteral("height"), normalGeometry_.height()}}},
        {QStringLiteral("screen_name"), screenName_},
        {QStringLiteral("maximized"), maximized_}});
    root.insert(QStringLiteral("ui"), ui);
    if (!PreferenceDocument::savePreferencesObject(root)) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
                                     QStringLiteral("window"), QStringLiteral("action=state_save_failed"));
    }
}

void WindowChrome::refreshTitleBarMetrics()
{
#ifdef Q_OS_MACOS
    if (window_.isNull()) {
        return;
    }
    applyMacOs(window_.data());
#else
    setTitleBarLeadingInset(0);
#endif
}

bool WindowChrome::nativeEventFilter(const QByteArray& eventType, void* message, qintptr* result)
{
#ifdef Q_OS_WIN
    if (eventType != "windows_generic_MSG" || nativeHandle_ == 0) {
        return false;
    }

    const auto* nativeMessage = static_cast<MSG*>(message);
    const auto handle = reinterpret_cast<HWND>(nativeHandle_);
    if (nativeMessage->hwnd != handle) {
        return false;
    }

    if (nativeMessage->message == WM_NCACTIVATE) {
        // Preserve native activation while the QML caption owns its pixels.
        *result = DefWindowProcW(handle, WM_NCACTIVATE, nativeMessage->wParam, -1);
        return true;
    }

    if (nativeMessage->message == WM_NCCALCSIZE && nativeMessage->wParam == TRUE) {
        if (IsZoomed(handle) && !window_->windowStates().testFlag(Qt::WindowFullScreen)) {
            MONITORINFO monitorInfo{};
            monitorInfo.cbSize = sizeof(monitorInfo);
            const HMONITOR monitor = MonitorFromWindow(handle, MONITOR_DEFAULTTONEAREST);
            if (GetMonitorInfoW(monitor, &monitorInfo)) {
                // QML owns the caption, so its client origin is the work-area
                // origin rather than the native caption's client origin.
                auto* parameters = reinterpret_cast<NCCALCSIZE_PARAMS*>(nativeMessage->lParam);
                parameters->rgrc[0] = monitorInfo.rcWork;
            }
        }

        *result = 0;
        return true;
    }

    if (nativeMessage->message == WM_NCHITTEST) {
        if (IsZoomed(handle)) {
            *result = HTCLIENT;
            return true;
        }

        RECT windowRect{};
        GetWindowRect(handle, &windowRect);

        const UINT dpi = GetDpiForWindow(handle);
        const int horizontalBorder = GetSystemMetricsForDpi(SM_CXFRAME, dpi)
            + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const int verticalBorder = GetSystemMetricsForDpi(SM_CYFRAME, dpi)
            + GetSystemMetricsForDpi(SM_CXPADDEDBORDER, dpi);
        const POINT cursorPosition{
            GET_X_LPARAM(nativeMessage->lParam),
            GET_Y_LPARAM(nativeMessage->lParam)};

        const bool left = cursorPosition.x < windowRect.left + horizontalBorder;
        const bool right = cursorPosition.x >= windowRect.right - horizontalBorder;
        const bool top = cursorPosition.y < windowRect.top + verticalBorder;
        const bool bottom = cursorPosition.y >= windowRect.bottom - verticalBorder;

        if (top && left)
            *result = HTTOPLEFT;
        else if (top && right)
            *result = HTTOPRIGHT;
        else if (bottom && left)
            *result = HTBOTTOMLEFT;
        else if (bottom && right)
            *result = HTBOTTOMRIGHT;
        else if (left)
            *result = HTLEFT;
        else if (right)
            *result = HTRIGHT;
        else if (top)
            *result = HTTOP;
        else if (bottom)
            *result = HTBOTTOM;
        else
            *result = HTCLIENT;

        return true;
    }

    if (nativeMessage->message == WM_DWMCOMPOSITIONCHANGED) {
        // Qt also updates composition settings while handling this message.
        materialUpdateTimer_.start();
    }
#else
    Q_UNUSED(eventType);
    Q_UNUSED(message);
    Q_UNUSED(result);
#endif

    return false;
}

bool WindowChrome::extendDwmFrame() const
{
#ifdef Q_OS_WIN
    if (nativeHandle_ == 0) {
        return false;
    }

    // The system backdrop covers the window independently of frame margins.
    // Keep the native caption style for animations, with its painting outside
    // the client area owned by QML. A full glass frame exposes DWM buttons.
    const MARGINS margins{1, 1, 0, 1};
    return SUCCEEDED(DwmExtendFrameIntoClientArea(reinterpret_cast<HWND>(nativeHandle_), &margins));
#else
    return false;
#endif
}

#ifndef Q_OS_MACOS
void WindowChrome::applyMacOs(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::observeMacOsFullScreen(QWindow* window)
{
    Q_UNUSED(window);
}

void WindowChrome::stopObservingMacOsFullScreen()
{
}

void WindowChrome::releaseMacOsMaterial()
{
}
#endif

} // namespace miacode::ui

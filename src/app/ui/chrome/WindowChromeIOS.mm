#include "app/ui/chrome/WindowChrome.h"
#include "app/ui/editor/PointerInputIOS.h"
#include "common/DebugLog.h"

#include <QQuickWindow>
#include <QWindow>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>

// Qt owns the scene and rendering controller. Extend their native responder
// integration without replacing the platform's scene or rendering lifecycle.
@interface QIOSWindowSceneDelegate : NSObject <UIWindowSceneDelegate>
@end
@interface QIOSWindowSceneDelegate (MiaCodeWindowing)
- (UISceneWindowingControlStyle*)preferredWindowingControlStyleForScene:(UIWindowScene*)scene API_AVAILABLE(ios(26.0));
@end
@implementation QIOSWindowSceneDelegate (MiaCodeWindowing)
- (UISceneWindowingControlStyle*)preferredWindowingControlStyleForScene:(UIWindowScene*)scene
{
    return UISceneWindowingControlStyle.unifiedStyle;
}
@end

// A toolbar-sized view lets UIKit resolve the actual corner exclusion area.
// It follows native layout notifications, including changes without a resize.
@interface MiaCodeTitleBarLayout : UIView {
    QPointer<miacode::ui::WindowChrome> _chrome;
    UIEdgeInsets _lastInsets;
}
- (instancetype)initWithFrame:(CGRect)frame chrome:(miacode::ui::WindowChrome*)chrome;
- (UIEdgeInsets)contentInsets;
@end

@implementation MiaCodeTitleBarLayout
- (instancetype)initWithFrame:(CGRect)frame chrome:(miacode::ui::WindowChrome*)chrome
{
    if ((self = [super initWithFrame:frame])) {
        _chrome = chrome;
        _lastInsets = UIEdgeInsetsMake(-1, -1, -1, -1);
        self.userInteractionEnabled = NO;
        self.autoresizingMask = UIViewAutoresizingFlexibleWidth;
        self.backgroundColor = UIColor.clearColor;
        self.accessibilityElementsHidden = YES;
    }
    return self;
}
- (UIEdgeInsets)contentInsets
{
    if (@available(iOS 26.0, *))
        return [self edgeInsetsForLayoutRegion:[UIViewLayoutRegion
            safeAreaLayoutRegionWithCornerAdaptation:UIViewLayoutRegionAdaptivityAxisHorizontal]];
    return self.safeAreaInsets;
}
- (void)layoutSubviews
{
    [super layoutSubviews];
    const UIEdgeInsets insets = self.contentInsets;
    if (!UIEdgeInsetsEqualToEdgeInsets(insets, _lastInsets)) {
        _lastInsets = insets;
        if (_chrome)
            QMetaObject::invokeMethod(_chrome.data(), &miacode::ui::WindowChrome::refreshTitleBarMetrics,
                Qt::QueuedConnection);
    }
}
- (void)safeAreaInsetsDidChange
{
    [super safeAreaInsetsDidChange];
    [self setNeedsLayout];
}
@end

namespace {
char titleBarLayoutKey;
}

namespace miacode::ui {
void WindowChrome::applyIos(QWindow* window)
{
    // Qt's fullscreen state fills the existing UIKit scene and hides the
    // in-scene status bar. The outer scene remains managed by iPadOS.
    if (window->windowState() != Qt::WindowFullScreen)
        window->setWindowState(Qt::WindowFullScreen);
    UIView* view = reinterpret_cast<UIView*>(window->winId());
    installPointerInput(view, window);
    MiaCodeTitleBarLayout* titleBar = objc_getAssociatedObject(view, &titleBarLayoutKey);
    if (!titleBar) {
        titleBar = [[MiaCodeTitleBarLayout alloc]
            initWithFrame:CGRectMake(0, 0, view.bounds.size.width, 44) chrome:this];
        [view addSubview:titleBar];
        objc_setAssociatedObject(view, &titleBarLayoutKey, titleBar, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
        [titleBar release];
    }
    setTitleBarLeadingInset(titleBar.contentInsets.left);
    setTitleBarHeight(44);
    if (view.window.windowScene) {
        view.window.windowScene.title = window->title().toNSString();
        const QRect bounds(0, 0, view.window.bounds.size.width, view.window.bounds.size.height);
        if (window->property("miacodeIosSceneBounds").toRect() != bounds) {
            window->setProperty("miacodeIosSceneBounds", bounds);
            miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
                QStringLiteral("window"), QStringLiteral("action=ios_scene_geometry width=%1 height=%2 client_width=%3 client_height=%4 titlebar_inset=%5 titlebar_height=%6")
                    .arg(bounds.width()).arg(bounds.height()).arg(window->width()).arg(window->height())
                    .arg(titleBarLeadingInset_).arg(titleBarHeight_));
        }
    }
    if (!window->property("miacodeIosChromeAttached").toBool()) {
        window->setProperty("miacodeIosChromeAttached", true);
        const auto refresh = [this, window] { applyIos(window); };
        connect(window, &QWindow::widthChanged, this, refresh);
        connect(window, &QWindow::heightChanged, this, refresh);
        connect(window, &QWindow::safeAreaMarginsChanged, this, refresh);
        connect(window, &QWindow::windowTitleChanged, this, refresh);
        connect(window, &QWindow::visibilityChanged, this, refresh);
        if (auto* quickWindow = qobject_cast<QQuickWindow*>(window))
            connect(quickWindow, &QQuickWindow::frameSwapped, this, refresh,
                Qt::ConnectionType(Qt::QueuedConnection | Qt::SingleShotConnection));
    }
}
}

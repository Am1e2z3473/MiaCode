#include "app/ui/editor/PointerInputIOS.h"

#include <QGuiApplication>
#include <QPointer>
#include <QWindow>
#include <qpa/qwindowsysteminterface.h>
#import <UIKit/UIKit.h>
#import <objc/runtime.h>
#import <objc/message.h>

// UIKit reports indirect-pointer presses as UITouch. Qt's iOS plugin treats
// those as touchscreen taps, losing mouse buttons and drag selection. Extend
// each rendering view with mouse dispatch, retaining Qt's direct-touch path.
@interface MiaCodePointerState : NSObject {
@public
    QPointer<QWindow> window;
    Qt::MouseButtons buttons;
}
@end
@implementation MiaCodePointerState
@end

namespace {
char pointerStateKey;

Qt::MouseButtons pointerButtons(UIEvent* event)
{
    Qt::MouseButtons buttons;
    if (event.buttonMask & UIEventButtonMaskPrimary) buttons |= Qt::LeftButton;
    if (event.buttonMask & UIEventButtonMaskSecondary) buttons |= Qt::RightButton;
    if (event.buttonMask & (1 << 2)) buttons |= Qt::MiddleButton;
    return buttons;
}

Qt::KeyboardModifiers pointerModifiers(UIEvent* event)
{
    Qt::KeyboardModifiers modifiers;
    if (event.modifierFlags & UIKeyModifierShift) modifiers |= Qt::ShiftModifier;
    if (event.modifierFlags & UIKeyModifierControl) modifiers |= Qt::ControlModifier;
    if (event.modifierFlags & UIKeyModifierAlternate) modifiers |= Qt::AltModifier;
    if (event.modifierFlags & UIKeyModifierCommand) modifiers |= Qt::MetaModifier;
    return modifiers;
}

void pointerTouches(UIView* view, SEL selector, NSSet<UITouch*>* touches, UIEvent* event)
{
    MiaCodePointerState* state = objc_getAssociatedObject(view, &pointerStateKey);
    NSMutableSet<UITouch*>* directTouches = [NSMutableSet set];
    for (UITouch* touch in touches) {
        if (touch.type != UITouchTypeIndirectPointer || !state->window) {
            [directTouches addObject:touch];
            continue;
        }
        QWindow* window = state->window.data();
        const QPointF local = QPointF::fromCGPoint([touch locationInView:view]);
        const QPointF global = window->mapToGlobal(local);
        // The pointer touch lifetime defines press/drag/release. A trackpad
        // may retain buttonMask on Ended or omit it on Moved, so those phases
        // must not derive the held-button state from the event mask.
        Qt::MouseButtons next = state->buttons;
        if (selector == @selector(touchesBegan:withEvent:)) {
            const Qt::MouseButtons pressed = pointerButtons(event);
            next |= pressed == Qt::NoButton ? Qt::LeftButton : pressed;
        } else if (selector == @selector(touchesEnded:withEvent:)
                   || selector == @selector(touchesCancelled:withEvent:)) {
            next = Qt::NoButton;
        }
        const auto changed = state->buttons ^ next;
        for (const auto button : {Qt::LeftButton, Qt::RightButton, Qt::MiddleButton}) {
            if (!changed.testFlag(button))
                continue;
            const bool pressed = next.testFlag(button);
            state->buttons.setFlag(button, pressed);
            QWindowSystemInterface::handleMouseEvent(window, ulong(event.timestamp * 1000),
                local, global, state->buttons, button,
                pressed ? QEvent::MouseButtonPress : QEvent::MouseButtonRelease,
                pointerModifiers(event));
        }
        if (selector == @selector(touchesMoved:withEvent:))
            QWindowSystemInterface::handleMouseEvent(window, ulong(event.timestamp * 1000),
                local, global, state->buttons, Qt::NoButton, QEvent::MouseMove,
                pointerModifiers(event));
    }
    if (directTouches.count) {
        struct objc_super parent = {view, class_getSuperclass(object_getClass(view))};
        reinterpret_cast<void (*)(struct objc_super*, SEL, NSSet*, UIEvent*)>(objc_msgSendSuper)
            (&parent, selector, directTouches, event);
    }
}

void pointerHover(UIView* view, SEL selector, UIHoverGestureRecognizer* recognizer)
{
    Q_UNUSED(selector);
    MiaCodePointerState* state = objc_getAssociatedObject(view, &pointerStateKey);
    if (!state->window)
        return;
    QWindow* window = state->window.data();
    const QPointF local = QPointF::fromCGPoint([recognizer locationInView:view]);
    const QPointF global = window->mapToGlobal(local);
    if (recognizer.state == UIGestureRecognizerStateEnded) {
        QWindowSystemInterface::handleLeaveEvent(window);
        return;
    }
    if (recognizer.state == UIGestureRecognizerStateBegan)
        QWindowSystemInterface::handleEnterEvent(window, local, global);
    if (recognizer.state == UIGestureRecognizerStateBegan || recognizer.state == UIGestureRecognizerStateChanged)
        QWindowSystemInterface::handleMouseEvent(window, local, global,
            state->buttons, Qt::NoButton, QEvent::MouseMove, QGuiApplication::keyboardModifiers());
}

}

namespace miacode::ui {
void installPointerInput(UIView* view, QWindow* window)
{
    if (objc_getAssociatedObject(view, &pointerStateKey))
        return;
    MiaCodePointerState* state = [[MiaCodePointerState alloc] init];
    state->window = window;
    state->buttons = Qt::NoButton;
    objc_setAssociatedObject(view, &pointerStateKey, state, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [state release];
    Class original = object_getClass(view);
    NSString* name = [@"MiaCodePointer_" stringByAppendingString:NSStringFromClass(original)];
    Class adapter = NSClassFromString(name);
    if (!adapter) {
        adapter = objc_allocateClassPair(original, name.UTF8String, 0);
        for (SEL selector : {@selector(touchesBegan:withEvent:), @selector(touchesMoved:withEvent:),
                             @selector(touchesEnded:withEvent:), @selector(touchesCancelled:withEvent:)}) {
            Method method = class_getInstanceMethod(original, selector);
            class_addMethod(adapter, selector, reinterpret_cast<IMP>(pointerTouches), method_getTypeEncoding(method));
        }
        Method hover = class_getInstanceMethod(original, @selector(handleMouseHover:));
        class_addMethod(adapter, @selector(handleMouseHover:), reinterpret_cast<IMP>(pointerHover),
            method_getTypeEncoding(hover));
        objc_registerClassPair(adapter);
    }
    object_setClass(view, adapter);
}
}

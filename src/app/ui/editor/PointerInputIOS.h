#pragma once

class QWindow;
@class UIView;

namespace miacode::ui {
void installPointerInput(UIView* view, QWindow* window);
}

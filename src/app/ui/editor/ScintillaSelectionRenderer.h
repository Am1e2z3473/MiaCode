#pragma once

#include <QColor>
class QQuickWindow;
class QSGNode;

namespace miacode::ui {
// Restyles Scintilla's captured selection geometry in its native under-text
// layer. Text layout, clipping, caret and selection state remain with the core.
void renderRoundedScintillaSelection(QSGNode* root, QQuickWindow* window, const QColor& color);
}

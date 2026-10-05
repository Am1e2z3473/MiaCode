#pragma once

#include <QColor>
#include <QVector>
class QQuickWindow;
class QSGNode;

namespace miacode::ui {
// Each captured range forms one filled contour beneath the text.
void renderRoundedScintillaHighlights(QSGNode* root, QQuickWindow* window, const QVector<QColor>& colors);
}

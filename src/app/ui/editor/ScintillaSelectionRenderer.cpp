#include "editor/ScintillaSelectionRenderer.h"

#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QQuickWindow>
#include <QSGImageNode>
#include <QSGRectangleNode>
#include <algorithm>
#include <cmath>

namespace miacode::ui {
namespace {
void appendPoint(QVector<QPointF>& edge, const QPointF& point)
{
    if (!edge.isEmpty() && edge.back() == point) return;
    if (edge.size() > 1) {
        const auto before = edge.at(edge.size() - 2);
        const auto last = edge.back();
        if ((before.x() == last.x() && last.x() == point.x())
            || (before.y() == last.y() && last.y() == point.y())) {
            edge.back() = point;
            return;
        }
    }
    edge.append(point);
}

QPainterPath selectionOutline(const QVector<QRectF>& rows)
{
    QPainterPath path;
    QVector<QPointF> left, right;
    auto closeContour = [&] {
        if (left.isEmpty()) return;
        QVector<QPointF> points = right;
        for (auto it = left.crbegin(); it != left.crend(); ++it) points.append(*it);
        for (int i = 0; i < points.size(); ++i) {
            const auto previous = points.at((i + points.size() - 1) % points.size());
            const auto current = points.at(i);
            const auto next = points.at((i + 1) % points.size());
            const qreal incoming = std::hypot(current.x() - previous.x(), current.y() - previous.y());
            const qreal outgoing = std::hypot(next.x() - current.x(), next.y() - current.y());
            const qreal radius = qMin(3.0, qMin(incoming / 2, outgoing / 2));
            const auto start = current - (current - previous) * (radius / incoming);
            const auto end = current + (next - current) * (radius / outgoing);
            if (i == 0) path.moveTo(start);
            else path.lineTo(start);
            // The same contour rounds convex outside and concave row joins.
            path.quadTo(current, end);
        }
        path.closeSubpath();
        left.clear();
        right.clear();
    };
    QRectF previous;
    for (const auto& row : rows) {
        if (!previous.isEmpty()) {
            const qreal overlapLeft = qMax(previous.left(), row.left());
            const qreal overlapRight = qMin(previous.right(), row.right());
            if (std::abs(row.top() - previous.bottom()) <= qMax(0.5, row.height() / 8)
                && overlapRight - overlapLeft > qMax(0.5, row.height() / 8)) {
                appendPoint(left, {overlapLeft, previous.bottom()});
                appendPoint(left, {overlapLeft, row.top()});
                appendPoint(right, {overlapRight, previous.bottom()});
                appendPoint(right, {overlapRight, row.top()});
            } else closeContour();
        }
        appendPoint(left, row.topLeft());
        appendPoint(left, row.bottomLeft());
        appendPoint(right, row.topRight());
        appendPoint(right, row.bottomRight());
        previous = row;
    }
    closeContour();
    return path;
}

class RoundedSelectionNode final : public QSGNode
{
public:
    void synchronize(QQuickWindow* window, const QVector<QRectF>& rows, const QColor& color)
    {
        const qreal dpr = window->effectiveDevicePixelRatio();
        if (rows == rows_ && color == color_ && dpr == dpr_) return;
        rows_ = rows;
        color_ = color;
        dpr_ = dpr;
        if (rows.isEmpty()) {
            if (image_) {
                removeChildNode(image_);
                delete image_;
                image_ = nullptr;
            }
            return;
        }
        const QPainterPath outline = selectionOutline(rows);
        const QRectF bounds = outline.boundingRect().adjusted(-1, -1, 1, 1);
        QImage image(QSize(qCeil(bounds.width() * dpr), qCeil(bounds.height() * dpr)), QImage::Format_ARGB32_Premultiplied);
        image.setDevicePixelRatio(dpr);
        image.fill(Qt::transparent);
        QPainter painter(&image);
        painter.setRenderHint(QPainter::Antialiasing);
        painter.translate(-bounds.topLeft());
        painter.fillPath(outline, color);
        painter.end();
        if (!image_) {
            image_ = window->createImageNode();
            image_->setOwnsTexture(true);
            image_->setFiltering(QSGTexture::Linear);
            image_->setTexture(window->createTextureFromImage(image));
            image_->setRect(bounds);
            appendChildNode(image_);
        } else image_->setTexture(window->createTextureFromImage(image));
        image_->setRect(bounds);
    }
private:
    QSGImageNode* image_ = nullptr;
    QVector<QRectF> rows_;
    QColor color_;
    qreal dpr_ = 0;
};

struct SelectionLayer {
    QSGNode* parent = nullptr;
    QVector<QRectF> rectangles;
    RoundedSelectionNode* contour = nullptr;
};

void collectSelectionLayers(QSGNode* parent, const QColor& color, QVector<SelectionLayer>& layers)
{
    SelectionLayer layer;
    layer.parent = parent;
    for (auto* child = parent->firstChild(); child; child = child->nextSibling()) {
        if (auto* contour = dynamic_cast<RoundedSelectionNode*>(child)) {
            layer.contour = contour;
        } else if (auto* rectangle = dynamic_cast<QSGRectangleNode*>(child); rectangle && rectangle->color().rgba() == color.rgba()) {
            if (!rectangle->rect().isEmpty()) layer.rectangles.append(rectangle->rect());
            rectangle->setColor(Qt::transparent);
        } else collectSelectionLayers(child, color, layers);
    }
    if (layer.contour || !layer.rectangles.isEmpty()) layers.append(layer);
}
}

void renderRoundedScintillaSelection(QSGNode* root, QQuickWindow* window, const QColor& color)
{
    if (!root || !window) return;
    // 使用指定选区颜色对应的公开 QSGRectangleNode 几何绘制圆角。
    QVector<SelectionLayer> layers;
    collectSelectionLayers(root, color, layers);
    for (auto& layer : layers) {
        std::sort(layer.rectangles.begin(), layer.rectangles.end(), [](const QRectF& a, const QRectF& b) {
            return a.top() == b.top() ? a.left() < b.left() : a.top() < b.top();
        });
        QVector<QRectF> rows;
        for (const auto& rectangle : std::as_const(layer.rectangles)) {
            if (!rows.isEmpty() && rows.back().top() == rectangle.top() && rows.back().height() == rectangle.height()
                && rectangle.left() <= rows.back().right()) rows.back() = rows.back().united(rectangle);
            else rows.append(rectangle);
        }
        if (!layer.contour) {
            layer.contour = new RoundedSelectionNode;
            layer.parent->appendChildNode(layer.contour);
        }
        layer.contour->synchronize(window, rows, color);
    }
}
}

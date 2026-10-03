#include "editor/EditorTextStyle.h"

#include <QTextBlockFormat>
#include <QTextCursor>
#include <QTextDocument>
#include <QtGlobal>
#include <QtQuick/private/qquicktextinterface_p.h>


namespace miacode::ui {
EditorTextStyle::EditorTextStyle(QObject* parent)
    : QObject(parent)
{
}

QQuickTextDocument* EditorTextStyle::textDocument() const { return textDocument_; }

QTextDocument* EditorTextStyle::document() const
{
    return textDocument_ != nullptr ? textDocument_->textDocument() : nullptr;
}

void EditorTextStyle::setTextDocument(QQuickTextDocument* value)
{
    if (textDocument_ == value) {
        return;
    }
    if (QTextDocument* previous = document(); previous != nullptr) {
        disconnect(previous, nullptr, this, nullptr);
    }
    textDocument_ = value;
    if (QTextDocument* current = document(); current != nullptr) {
        connect(current, &QTextDocument::contentsChange,
                this, &EditorTextStyle::onContentsChange);
    }
    applyToDocument();
    emit textDocumentChanged();
}

int EditorTextStyle::blockSpacing() const { return blockSpacing_; }

void EditorTextStyle::setBlockSpacing(int pixels)
{
    const int normalized = qMax(0, pixels);
    if (blockSpacing_ == normalized) {
        return;
    }
    blockSpacing_ = normalized;
    applyToDocument();
    emit blockSpacingChanged();
}

void EditorTextStyle::applyImmediately()
{
    // A queued range pass may have been scheduled by the replacement itself.
    // The full-document pass below supersedes it, so let the queued callback
    // become a no-op instead of applying a stale range later.
    rangePending_ = false;
    applyToDocument();
}

void EditorTextStyle::invalidateRendering()
{
    if (textDocument_ == nullptr) return;
    if (auto* item = qobject_cast<QQuickTextInterface*>(textDocument_->parent())) {
        // Qt queues a complete text-node rebuild. An ordinary item update or
        // rehighlight retains the old node offsets, which may exceed the end
        // of a shorter replacement document.
        item->invalidate();
    }
}

void EditorTextStyle::onContentsChange(int position, int charsRemoved, int charsAdded)
{
    Q_UNUSED(charsRemoved);
    if (applying_) {
        return;
    }
    const int start = qMax(0, position);
    const int end = start + qMax(0, charsAdded);
    if (rangePending_) {
        pendingStart_ = qMin(pendingStart_, start);
        pendingEnd_ = qMax(pendingEnd_, end);
        return;
    }
    pendingStart_ = start;
    pendingEnd_ = end;
    rangePending_ = true;
    QMetaObject::invokeMethod(this, [this] { flushPendingRange(); }, Qt::QueuedConnection);
}

void EditorTextStyle::flushPendingRange()
{
    if (!rangePending_) {
        return;
    }
    rangePending_ = false;
    applyToRange(pendingStart_, pendingEnd_);
}

void EditorTextStyle::applyToDocument()
{
    QTextDocument* target = document();
    if (target == nullptr) {
        return;
    }
    applyToRange(0, target->characterCount());
}

void EditorTextStyle::applyToRange(int start, int end)
{
    QTextDocument* target = document();
    if (target == nullptr) {
        return;
    }
    const int last = qMax(0, target->characterCount() - 1);
    const int from = qBound(0, start, last);
    const int to = qBound(from, end, last);
    applying_ = true;
    QTextCursor cursor(target);
    cursor.setPosition(from);
    cursor.setPosition(to, QTextCursor::KeepAnchor);
    QTextBlockFormat format;
    format.setLineHeight(static_cast<qreal>(blockSpacing_), QTextBlockFormat::LineDistanceHeight);
    format.setBottomMargin(0);
    cursor.mergeBlockFormat(format);
    applying_ = false;
}

} // namespace miacode::ui

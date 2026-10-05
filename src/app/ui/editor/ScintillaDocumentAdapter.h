#pragma once

#include <scintillaquick/scintillaquick_item.h>
#include <QHash>
#include <QVector>

namespace miacode::ui {

// Scintilla owns document storage and undo. This adapter retains one document
// reference and viewport per difficulty, and projects UTF-16 service offsets.
class ScintillaDocumentAdapter
{
public:
    explicit ScintillaDocumentAdapter(ScintillaQuick_item& editor);
    ~ScintillaDocumentAdapter();
    void activate(const QString& scope, const QString& text);
    void clear();
    void drop(const QString& scope);
    void refresh();
    void applyChange(int position, int deletedBytes, const QByteArray& inserted);
    int lineCount() const { return utf16Lines_.size(); }
    int lineStart(int line) const { return utf16Lines_[line]; }
    int lineAt(int position) const;
    void captureViewport();
    bool restoreViewport();
    int bytePosition(int utf16) const;
    int utf16Position(int byte) const;
    const QString& text() const { return text_; }
    const QString& scope() const { return scope_; }

private:
    struct Document {
        sptr_t pointer = 0;
        int anchor = 0;
        int caret = 0;
        int topPosition = 0;
        int xOffset = 0;
    };
    void saveViewport();
    ScintillaQuick_item& editor_;
    QHash<QString, Document> documents_;
    QString scope_;
    QString text_;
    QVector<int> utf16Lines_;
    QVector<int> byteLines_;
    bool viewportPending_ = false;
};
}

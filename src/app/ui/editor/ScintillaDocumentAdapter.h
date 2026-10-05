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
    QVector<int> bytes_;
    bool viewportPending_ = false;
};
}

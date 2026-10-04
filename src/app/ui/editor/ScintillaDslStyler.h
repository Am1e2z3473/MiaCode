#pragma once
#include <QVariantMap>
#include "editor/ScintillaDocumentAdapter.h"

namespace miacode::ui {
class ScintillaDslStyler
{
public:
    ScintillaDslStyler(ScintillaQuick_item& editor, ScintillaDocumentAdapter& document);
    void setPalette(const QVariantMap& palette);
    void style();
    void diagnostics(const QVariantList& validation, const QVariantList& muri);
    void bookmarks(const QVariantList& bookmarks);
    void follow(bool active, int start, int end, int caret);
private:
    void fill(int indicator, int start, int end);
    ScintillaQuick_item& editor_;
    ScintillaDocumentAdapter& document_;
};
}

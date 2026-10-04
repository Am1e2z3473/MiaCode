#include "editor/ScintillaDocumentAdapter.h"
#include <algorithm>

namespace miacode::ui {
ScintillaDocumentAdapter::ScintillaDocumentAdapter(ScintillaQuick_item& editor) : editor_(editor) { refresh(); }
ScintillaDocumentAdapter::~ScintillaDocumentAdapter() { clear(); }

void ScintillaDocumentAdapter::saveViewport()
{
    auto it = documents_.find(scope_);
    if (it == documents_.end()) return;
    it->anchor = editor_.send(SCI_GETANCHOR);
    it->caret = editor_.send(SCI_GETCURRENTPOS);
    it->firstLine = editor_.send(SCI_GETFIRSTVISIBLELINE);
    it->xOffset = editor_.send(SCI_GETXOFFSET);
}

void ScintillaDocumentAdapter::activate(const QString& scope, const QString& text)
{
    if (scope_ != scope || !documents_.contains(scope)) {
        saveViewport();
        if (!documents_.contains(scope)) {
            Document document;
            document.pointer = editor_.send(SCI_CREATEDOCUMENT, 0, SC_DOCUMENTOPTION_DEFAULT);
            documents_.insert(scope, document);
        }
        scope_ = scope;
        const auto& document = documents_[scope];
        editor_.send(SCI_SETDOCPOINTER, 0, document.pointer);
        editor_.send(SCI_SETCODEPAGE, SC_CP_UTF8);
        refresh();
        if (text_ != text) {
            editor_.sends(SCI_SETTEXT, 0, text.toUtf8().constData());
            editor_.send(SCI_EMPTYUNDOBUFFER);
            refresh();
        }
        editor_.send(SCI_SETSEL, document.anchor, document.caret);
        editor_.send(SCI_SETFIRSTVISIBLELINE, document.firstLine);
        editor_.send(SCI_SETXOFFSET, document.xOffset);
    } else if (text_ != text) {
        // External DSL transforms and source replacements join native history.
        editor_.send(SCI_BEGINUNDOACTION);
        editor_.send(SCI_SETTARGETSTART, 0);
        editor_.send(SCI_SETTARGETEND, editor_.send(SCI_GETLENGTH));
        const QByteArray replacement = text.toUtf8();
        editor_.sends(SCI_REPLACETARGET, replacement.size(), replacement.constData());
        editor_.send(SCI_ENDUNDOACTION);
        refresh();
    }
}

void ScintillaDocumentAdapter::drop(const QString& scope)
{
    auto it = documents_.find(scope);
    if (it == documents_.end()) return;
    editor_.send(SCI_RELEASEDOCUMENT, 0, it->pointer);
    documents_.erase(it);
    if (scope_ == scope) scope_.clear();
}

void ScintillaDocumentAdapter::clear()
{
    for (const auto& document : std::as_const(documents_))
        editor_.send(SCI_RELEASEDOCUMENT, 0, document.pointer);
    documents_.clear();
    scope_.clear();
}

void ScintillaDocumentAdapter::refresh()
{
    text_ = editor_.property("text").toString();
    bytes_.resize(text_.size() + 1);
    int byte = 0;
    for (int i = 0; i < text_.size(); ++i) {
        bytes_[i] = byte;
        const QChar ch = text_[i];
        if (ch.isHighSurrogate() && i + 1 < text_.size() && text_[i + 1].isLowSurrogate()) {
            bytes_[++i] = byte;
            byte += 4;
        } else {
            byte += ch.unicode() < 0x80 ? 1 : ch.unicode() < 0x800 ? 2 : 3;
        }
    }
    bytes_[text_.size()] = byte;
}
int ScintillaDocumentAdapter::bytePosition(int utf16) const
{
    return bytes_[qBound(0, utf16, int(text_.size()))];
}
int ScintillaDocumentAdapter::utf16Position(int byte) const
{
    const auto it = std::lower_bound(bytes_.cbegin(), bytes_.cend(), qBound(0, byte, bytes_.constLast()));
    return int(it - bytes_.cbegin());
}
}

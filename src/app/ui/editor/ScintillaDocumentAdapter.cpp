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
    it->xOffset = editor_.send(SCI_GETXOFFSET);
}

void ScintillaDocumentAdapter::captureViewport()
{
    auto it = documents_.find(scope_);
    if (it == documents_.end()) return;
    int x = editor_.send(SCI_GETMARGINLEFT);
    for (int margin = 0; margin < editor_.send(SCI_GETMARGINS); ++margin)
        x += editor_.send(SCI_GETMARGINWIDTHN, margin);
    it->topPosition = editor_.send(SCI_POSITIONFROMPOINT, x, 0);
    saveViewport();
}

bool ScintillaDocumentAdapter::restoreViewport()
{
    if (!viewportPending_) return false;
    viewportPending_ = false;
    const auto& document = documents_[scope_];
    const int position = qBound(0, document.topPosition, int(editor_.send(SCI_GETLENGTH)));
    editor_.send(SCI_ENSUREVISIBLE, editor_.send(SCI_LINEFROMPOSITION, position));
    const int row = editor_.send(SCI_GETFIRSTVISIBLELINE)
        + editor_.send(SCI_POINTYFROMPOSITION, 0, position) / editor_.send(SCI_TEXTHEIGHT, 0);
    editor_.scrollVertical(row);
    editor_.send(SCI_SETXOFFSET, document.xOffset);
    return true;
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
        }
        const int length = editor_.send(SCI_GETLENGTH);
        editor_.send(SCI_SETSELECTION, qBound(0, document.caret, length), qBound(0, document.anchor, length));
        viewportPending_ = true;
    } else if (text_ != text) {
        saveViewport();
        // External DSL transforms and source replacements join native history.
        editor_.send(SCI_BEGINUNDOACTION);
        editor_.send(SCI_SETTARGETSTART, 0);
        editor_.send(SCI_SETTARGETEND, editor_.send(SCI_GETLENGTH));
        const QByteArray replacement = text.toUtf8();
        editor_.sends(SCI_REPLACETARGET, replacement.size(), replacement.constData());
        editor_.send(SCI_ENDUNDOACTION);
        viewportPending_ = true;
    }
}

void ScintillaDocumentAdapter::drop(const QString& scope)
{
    auto it = documents_.find(scope);
    if (it == documents_.end()) return;
    editor_.send(SCI_RELEASEDOCUMENT, 0, it->pointer);
    documents_.erase(it);
    if (scope_ == scope) {
        scope_.clear();
        viewportPending_ = false;
    }
}

void ScintillaDocumentAdapter::clear()
{
    for (const auto& document : std::as_const(documents_))
        editor_.send(SCI_RELEASEDOCUMENT, 0, document.pointer);
    documents_.clear();
    scope_.clear();
    viewportPending_ = false;
}

void ScintillaDocumentAdapter::refresh()
{
    const int length = editor_.send(SCI_GETLENGTH);
    QByteArray buffer(length + 1, Qt::Uninitialized);
    editor_.send(SCI_GETTEXT, buffer.size(), reinterpret_cast<sptr_t>(buffer.data()));
    text_ = QString::fromUtf8(buffer.constData(), length);
    utf16Lines_ = {0};
    byteLines_ = {0};
    const QByteArray bytes = text_.toUtf8();
    for (int i = 0; i < text_.size(); ++i)
        if (text_[i] == QLatin1Char('\n')) utf16Lines_.append(i + 1);
    for (int i = 0; i < bytes.size(); ++i)
        if (bytes[i] == '\n') byteLines_.append(i + 1);
}
int ScintillaDocumentAdapter::lineAt(int position) const
{
    return int(std::upper_bound(utf16Lines_.cbegin(), utf16Lines_.cend(), position) - utf16Lines_.cbegin()) - 1;
}
void ScintillaDocumentAdapter::applyChange(int position, int deletedBytes, const QByteArray& inserted)
{
    const int begin = utf16Position(position);
    const int end = utf16Position(position + deletedBytes);
    const QString replacement = QString::fromUtf8(inserted);
    const int first = int(std::upper_bound(byteLines_.cbegin(), byteLines_.cend(), position) - byteLines_.cbegin());
    const int last = int(std::upper_bound(byteLines_.cbegin(), byteLines_.cend(), position + deletedBytes) - byteLines_.cbegin());
    utf16Lines_.remove(first, last - first);
    byteLines_.remove(first, last - first);
    for (int i = first; i < byteLines_.size(); ++i) {
        byteLines_[i] += inserted.size() - deletedBytes;
        utf16Lines_[i] += replacement.size() - (end - begin);
    }
    int line = first;
    for (int i = 0; i < replacement.size(); ++i)
        if (replacement[i] == QLatin1Char('\n')) utf16Lines_.insert(line++, begin + i + 1);
    line = first;
    for (int i = 0; i < inserted.size(); ++i)
        if (inserted[i] == '\n') byteLines_.insert(line++, position + i + 1);
    text_.replace(begin, end - begin, replacement);
}
int ScintillaDocumentAdapter::bytePosition(int utf16) const
{
    utf16 = qBound(0, utf16, int(text_.size()));
    const int line = lineAt(utf16);
    return byteLines_[line] + QStringView(text_).mid(utf16Lines_[line], utf16 - utf16Lines_[line]).toUtf8().size();
}
int ScintillaDocumentAdapter::utf16Position(int byte) const
{
    const int line = int(std::upper_bound(byteLines_.cbegin(), byteLines_.cend(), byte) - byteLines_.cbegin()) - 1;
    const int end = line + 1 < utf16Lines_.size() ? utf16Lines_[line + 1] : text_.size();
    const QByteArray bytes = QStringView(text_).mid(utf16Lines_[line], end - utf16Lines_[line]).toUtf8();
    return utf16Lines_[line] + QString::fromUtf8(bytes.constData(), qBound(0, byte - byteLines_[line], int(bytes.size()))).size();
}
}

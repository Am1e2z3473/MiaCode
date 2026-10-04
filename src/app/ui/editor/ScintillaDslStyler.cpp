#include "editor/ScintillaDslStyler.h"
#include <QColor>

namespace miacode::ui {
namespace {
constexpr int errorIndicator = 8;
constexpr int warningIndicator = 9;
constexpr int muriIndicator = 10;
constexpr int followIndicator = 11;
constexpr int followCaretIndicator = 12;
constexpr int bookmarkMarker = 1;
constexpr int followMarker = 2;
int color(const QVariantMap& palette, const char* key) { return scintillaquick::rgb_from_color(palette.value(QLatin1String(key)).value<QColor>()); }
}
ScintillaDslStyler::ScintillaDslStyler(ScintillaQuick_item& editor, ScintillaDocumentAdapter& document)
    : editor_(editor), document_(document)
{
    editor_.send(SCI_SETILEXER, 0, 0);
    editor_.set_auto_line_number_margin(0);
    editor_.send(SCI_SETMARGINTYPEN, 1, SC_MARGIN_SYMBOL);
    editor_.send(SCI_SETMARGINMASKN, 1, (1 << bookmarkMarker));
    editor_.send(SCI_SETMARGINWIDTHN, 1, 16);
    editor_.send(SCI_SETMARGINSENSITIVEN, 1, 1);
    editor_.send(SCI_MARKERDEFINE, bookmarkMarker, SC_MARK_BOOKMARK);
    editor_.send(SCI_MARKERDEFINE, followMarker, SC_MARK_BACKGROUND);
    for (int indicator : {errorIndicator, warningIndicator, muriIndicator})
        editor_.send(SCI_INDICSETSTYLE, indicator, INDIC_SQUIGGLE);
    editor_.send(SCI_INDICSETSTYLE, followIndicator, INDIC_ROUNDBOX);
    editor_.send(SCI_INDICSETUNDER, followIndicator, 1);
    editor_.send(SCI_INDICSETALPHA, followIndicator, 70);
    editor_.send(SCI_INDICSETSTYLE, followCaretIndicator, INDIC_POINT);
    editor_.send(SCI_SETCARETLINEVISIBLE, 1);
    editor_.send(SCI_SETCARETLINEBACKALPHA, 40);
    editor_.send(SCI_SETWRAPMODE, SC_WRAP_WORD);
}
void ScintillaDslStyler::setPalette(const QVariantMap& palette)
{
    editor_.send(SCI_STYLESETFORE, STYLE_DEFAULT, color(palette, "text"));
    editor_.send(SCI_STYLESETBACK, STYLE_DEFAULT, color(palette, "background"));
    editor_.send(SCI_STYLECLEARALL);
    editor_.send(SCI_STYLESETFORE, 1, color(palette, "keyword"));
    editor_.send(SCI_STYLESETFORE, 2, color(palette, "duration"));
    editor_.send(SCI_STYLESETFORE, 3, color(palette, "comment"));
    editor_.send(SCI_STYLESETFORE, STYLE_LINENUMBER, color(palette, "comment"));
    editor_.send(SCI_SETCARETFORE, color(palette, "text"));
    editor_.send(SCI_SETCARETLINEBACK, color(palette, "currentLine"));
    editor_.send(SCI_SETSELBACK, 1, color(palette, "selection"));
    editor_.send(SCI_SETSELALPHA, 100);
    editor_.send(SCI_MARKERSETBACK, bookmarkMarker, color(palette, "keyword"));
    editor_.send(SCI_MARKERSETBACK, followMarker, color(palette, "follow"));
    editor_.send(SCI_MARKERSETALPHA, followMarker, 70);
    editor_.send(SCI_INDICSETFORE, errorIndicator, color(palette, "error"));
    editor_.send(SCI_INDICSETFORE, warningIndicator, color(palette, "warning"));
    editor_.send(SCI_INDICSETFORE, muriIndicator, color(palette, "error"));
    editor_.send(SCI_INDICSETFORE, followIndicator, color(palette, "follow"));
    editor_.send(SCI_INDICSETFORE, followCaretIndicator, color(palette, "keyword"));
    style();
}
void ScintillaDslStyler::style()
{
    const QByteArray text = document_.text().toUtf8();
    QByteArray styles(text.size(), 0);
    QByteArray stack;
    bool comment = false;
    for (int i = 0; i < text.size(); ++i) {
        const char ch = text[i];
        if (ch == '\n' || ch == '\r') comment = false;
        if (ch == '|' && i + 1 < text.size() && text[i + 1] == '|') comment = true;
        if (comment) { styles[i] = 3; continue; }
        styles[i] = stack.isEmpty() ? 0 : stack.back() == ']' ? 2 : 1;
        if (ch == '<' && text.mid(i + 1, 3) == "HS*") {
            const int close = text.indexOf('>', i + 1);
            const int newline = text.indexOf('\n', i + 1);
            if (close >= 0 && (newline < 0 || close < newline)) {
                for (; i <= close; ++i) styles[i] = 1;
                --i;
                continue;
            }
        }
        if (ch == '(' || ch == '{' || ch == '[') {
            stack.append(ch == '(' ? ')' : ch == '{' ? '}' : ']');
            styles[i] = ch == '[' ? 2 : 1;
        } else if (ch == ')' || ch == '}' || ch == ']') {
            styles[i] = ch == ']' ? 2 : 1;
            const int matching = stack.lastIndexOf(ch);
            if (matching >= 0) stack.truncate(matching);
        }
    }
    editor_.send(SCI_STARTSTYLING, 0);
    editor_.sends(SCI_SETSTYLINGEX, styles.size(), styles.constData());
}
void ScintillaDslStyler::fill(int indicator, int start, int end)
{
    const int first = document_.bytePosition(start);
    const int last = document_.bytePosition(end);
    editor_.send(SCI_SETINDICATORCURRENT, indicator);
    editor_.send(SCI_INDICATORFILLRANGE, first, last - first);
}
void ScintillaDslStyler::diagnostics(const QVariantList& validation, const QVariantList& muri)
{
    for (int indicator : {errorIndicator, warningIndicator, muriIndicator}) {
        editor_.send(SCI_SETINDICATORCURRENT, indicator);
        editor_.send(SCI_INDICATORCLEARRANGE, 0, editor_.send(SCI_GETLENGTH));
    }
    auto decorate = [this](const QVariantList& rows, bool isMuri) {
        for (const auto& value : rows) {
            const auto row = value.toMap();
            const int line = row.value(QStringLiteral("line")).toInt() - 1;
            if (line < 0 || line >= editor_.send(SCI_GETLINECOUNT)) continue;
            const int lineStart = document_.utf16Position(editor_.send(SCI_POSITIONFROMLINE, line));
            const int lineEnd = document_.utf16Position(editor_.send(SCI_GETLINEENDPOSITION, line));
            const int start = qBound(lineStart, lineStart + qMax(0, row.value(QStringLiteral("column")).toInt() - 1), lineEnd);
            const int end = qBound(start, lineStart + qMax(row.value(QStringLiteral("endColumn")).toInt(), row.value(QStringLiteral("column")).toInt()), lineEnd);
            const int indicator = isMuri ? muriIndicator : row.value(QStringLiteral("severity")) == QStringLiteral("warning") ? warningIndicator : errorIndicator;
            fill(indicator, start, end);
        }
    };
    decorate(validation, false);
    decorate(muri, true);
}
void ScintillaDslStyler::bookmarks(const QVariantList& bookmarks)
{
    editor_.send(SCI_MARKERDELETEALL, bookmarkMarker);
    for (const auto& value : bookmarks)
        editor_.send(SCI_MARKERADD, value.toMap().value(QStringLiteral("line")).toInt() - 1, bookmarkMarker);
}
void ScintillaDslStyler::follow(bool active, int start, int end, int caret)
{
    for (int indicator : {followIndicator, followCaretIndicator}) {
        editor_.send(SCI_SETINDICATORCURRENT, indicator);
        editor_.send(SCI_INDICATORCLEARRANGE, 0, editor_.send(SCI_GETLENGTH));
    }
    editor_.send(SCI_MARKERDELETEALL, followMarker);
    if (!active) return;
    fill(followIndicator, start, end);
    fill(followCaretIndicator, caret, caret + 1);
    editor_.send(SCI_MARKERADD, editor_.send(SCI_LINEFROMPOSITION, document_.bytePosition(caret)), followMarker);
}
}

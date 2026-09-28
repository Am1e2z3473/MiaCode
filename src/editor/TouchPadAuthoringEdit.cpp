#include "editor/TouchPadAuthoringEdit.h"

#include "core/chart/parser/SimaiCommentScan.h"

#include <QTextCursor>
#include <QTextDocument>

namespace miacode::editor {

namespace {

int skipSpaces(const QString& text, int position, int end)
{
    while (position < end && text.at(position).isSpace()) {
        ++position;
    }
    return position;
}

int firstTouchCandidateStart(const QString& text, int start, int end)
{
    int position = skipSpaces(text, start, end);
    while (position < end) {
        const QChar opening = text.at(position);
        const QChar closing = opening == QLatin1Char('(')
            ? QLatin1Char(')')
            : opening == QLatin1Char('{') ? QLatin1Char('}') : QChar();
        const bool isHsDirective = text.mid(position, 4) == QStringLiteral("<HS*");
        if (closing.isNull() && !isHsDirective) {
            break;
        }
        const int closePosition = isHsDirective
            ? text.indexOf(QLatin1Char('>'), position + 4)
            : text.indexOf(closing, position + 1);
        if (closePosition < 0 || closePosition >= end) {
            break;
        }
        position = skipSpaces(text, closePosition + 1, end);
    }
    return position;
}

bool isTouchItemSeparator(QChar ch)
{
    return ch == QLatin1Char('/') || ch == QLatin1Char('`');
}

bool isSelectedOrdinaryTouch(const QString& item, const QString& normalizedPad)
{
    if (item.compare(normalizedPad, Qt::CaseInsensitive) == 0) {
        return true;
    }
    return item.size() == normalizedPad.size() + 1
        && item.endsWith(QLatin1Char('f'), Qt::CaseInsensitive)
        && item.left(normalizedPad.size()).compare(normalizedPad, Qt::CaseInsensitive) == 0;
}

int trimmedEnd(const QString& text, int start, int end)
{
    while (end > start && text.at(end - 1).isSpace()) {
        --end;
    }
    return end;
}

// Where a pad lands in `[start, end)` when that range holds no note: right
// after its leading controls. When the range covers a line break the beat
// belongs to the LAST line it reaches: appending at the trimmed content end
// would strand the pad on the previous line, behind that line's trailing
// comment.
int emptyTokenPadPosition(const QString& text, int start, int end)
{
    const QVector<miacode::simai::ChartContentSpan> spans =
        miacode::simai::chartContentSpans(text, start, end);
    const int lastNewline = end > start
        ? text.lastIndexOf(QLatin1Char('\n'), end - 1)
        : -1;
    int contentStart = start;
    int contentEnd = start;
    if (!spans.isEmpty()) {
        if (lastNewline >= start) {
            // Only that last line's own text counts; whatever preceded the line
            // break belongs to the bar above.
            const miacode::simai::ChartContentSpan& lastSpan = spans.constLast();
            contentStart = qMax(lastSpan.start, lastNewline + 1);
            contentEnd = trimmedEnd(text, contentStart, qMax(contentStart, lastSpan.end));
        } else {
            contentStart = spans.first().start;
            contentEnd = trimmedEnd(text, contentStart, spans.first().end);
        }
    }
    // Controls are part of the token prefix, not an each-note separator — a
    // `{16}` opening the line still has to precede the authored pad.
    return firstTouchCandidateStart(text, contentStart, contentEnd);
}

} // namespace

TouchPadAuthoringEditPlan planTouchPadAuthoringEdit(
    const QString& text,
    int cursorPosition,
    const QString& pad,
    QChar separator)
{
    TouchPadAuthoringEditPlan plan;
    const QString normalizedPad = pad.trimmed().toUpper();
    if (normalizedPad.isEmpty()) {
        return plan;
    }
    const int position = qBound(0, cursorPosition, text.size());
    // Commas inside a `||` comment are prose, not beat separators — both
    // parsers stop at the marker and resume on the next line.
    const int leftComma = miacode::simai::previousChartComma(text, position);
    if (separator == QLatin1Char(',')) {
        plan.tokenStart = leftComma + 1;
        plan.insertionPosition = position;
        plan.insertionText = normalizedPad + separator;
        plan.valid = true;
        return plan;
    }
    const int rightComma = miacode::simai::nextChartComma(text, position);
    plan.tokenStart = leftComma + 1;
    const int tokenEnd = rightComma >= 0 ? rightComma : text.size();
    // A comment ends at ITS newline, not at the token end, so one comma token
    // can hold chart content on both sides of one (or several) comments.
    const QVector<miacode::simai::ChartContentSpan> spans =
        miacode::simai::chartContentSpans(text, plan.tokenStart, tokenEnd);

    // Reuse the ordinary-touch removal path's leading-control scan: BPM and
    // subdivision declarations do not make a comma token non-empty.
    bool empty = true;
    int lastContentEnd = plan.tokenStart;
    for (const miacode::simai::ChartContentSpan& span : spans) {
        const int spanEnd = trimmedEnd(text, span.start, span.end);
        if (spanEnd > span.start) {
            lastContentEnd = spanEnd;
        }
        if (firstTouchCandidateStart(text, span.start, spanEnd) < spanEnd) {
            empty = false;
        }
    }

    // Left and Ctrl+Shift clicks toggle the first matching pad in the beat.
    // Items split on `/` and `` ` `` only; whitespace is not a separator.
    for (const miacode::simai::ChartContentSpan& span : spans) {
        if (empty) {
            break;
        }
        const int spanEnd = trimmedEnd(text, span.start, span.end);
        int itemStart = span.start;
        int itemIndex = 0;
        while (itemStart <= spanEnd) {
            int itemEnd = itemStart;
            while (itemEnd < spanEnd && !isTouchItemSeparator(text.at(itemEnd))) {
                ++itemEnd;
            }
            const int padStart = itemIndex == 0
                ? firstTouchCandidateStart(text, itemStart, itemEnd)
                : skipSpaces(text, itemStart, itemEnd);
            const int padEnd = trimmedEnd(text, padStart, itemEnd);
            if (isSelectedOrdinaryTouch(text.mid(padStart, padEnd - padStart), normalizedPad)) {
                plan.insertionText.clear();
                if (itemIndex > 0) {
                    plan.insertionPosition = itemStart - 1;
                    plan.removalLength = itemEnd - plan.insertionPosition;
                } else if (itemEnd < spanEnd) {
                    plan.insertionPosition = padStart;
                    plan.removalLength = itemEnd + 1 - padStart;
                } else {
                    plan.insertionPosition = padStart;
                    plan.removalLength = itemEnd - padStart;
                }
                plan.valid = true;
                return plan;
            }
            if (itemEnd >= spanEnd) {
                break;
            }
            itemStart = itemEnd + 1;
            ++itemIndex;
        }
    }

    if (!empty) {
        plan.insertionPosition = lastContentEnd;
        const QChar validatedSeparator = separator == QLatin1Char('`')
            ? separator
            : QLatin1Char('/');
        plan.insertionText = QString(validatedSeparator) + normalizedPad;
        plan.valid = true;
        return plan;
    }

    // The pad is written flush against the controls: authoring never inserts
    // whitespace of its own.
    plan.insertionPosition = emptyTokenPadPosition(text, plan.tokenStart, tokenEnd);
    plan.insertionText = normalizedPad;
    plan.valid = true;
    return plan;
}

bool applyTouchPadAuthoringEdit(
    QTextDocument* document,
    QTextCursor* cursor,
    const TouchPadAuthoringEditPlan& plan)
{
    if (document == nullptr || cursor == nullptr || !plan.valid) {
        return false;
    }
    QTextCursor editCursor(document);
    editCursor.beginEditBlock();
    const int documentLength = document->characterCount() - 1;
    const int editStart = qBound(0, plan.insertionPosition, documentLength);
    editCursor.setPosition(editStart);
    if (plan.removalLength > 0) {
        editCursor.setPosition(
            qBound(editStart, editStart + plan.removalLength, documentLength),
            QTextCursor::KeepAnchor);
    }
    editCursor.insertText(plan.insertionText);
    editCursor.endEditBlock();
    *cursor = editCursor;
    return true;
}

} // namespace miacode::editor

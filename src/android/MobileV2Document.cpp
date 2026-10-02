#include "AndroidDocumentSession.h"
#include "app/ui/document/ChartTransformCommands.h"
#include "core/chart/selection/ChartSelectionBeatSummary.h"
#include "core/chart/transform/ChartNormalization.h"
#include "app/ui/editor/EditorController.h"
#include <algorithm>
namespace miacode::android {
namespace {
miacode::chart_transform::ChartNormalizationOptions normalizeOptionsFromVariant(const QVariantMap& value) {
    miacode::chart_transform::ChartNormalizationOptions result;
    result.reduceTo384Grid = value.value("reduceTo384Grid", true).toBool();
    result.sectionMeasureCount = value.value("sectionMeasureCount", 4).toInt();
    result.splitEveryFourMeasures = result.sectionMeasureCount > 0;
    result.syntax = value.value("syntax").toString() == "compact_single_line"
        ? miacode::chart_transform::ChartNormalizationSyntax::CompactSingleLine
        : miacode::chart_transform::ChartNormalizationSyntax::SegmentPreserving;
    return result;
}
}
QVariantList AndroidDocumentSession::syntaxIssues() const {
    QVariantList rows;
    const auto result = SimaiNativeParser::buildValidationReport(chartText(), SimaiNativeValidationLocale::Chinese);
    for (const auto& issue : result.issues) rows.append(QVariantMap{
        {"line", issue.line}, {"column", issue.col}, {"endColumn", issue.endCol},
        {"severity", issue.severity == SimaiNativeValidationSeverity::Error ? "error" : "warning"},
        {"message", issue.displayMessage}});
    return rows;
}
int AndroidDocumentSession::syntaxErrorCount() const {
    const auto result = SimaiNativeParser::buildValidationReport(chartText(), SimaiNativeValidationLocale::Chinese);
    return std::count_if(result.issues.cbegin(), result.issues.cend(), [](const auto& issue) {
        return issue.severity == SimaiNativeValidationSeverity::Error;
    });
}
int AndroidDocumentSession::syntaxWarningCount() const {
    const auto result = SimaiNativeParser::buildValidationReport(chartText(), SimaiNativeValidationLocale::Chinese);
    return result.issues.size() - syntaxErrorCount();
}
QVariantList AndroidDocumentSession::normalizeGridOptions() const
{
    const auto row = [](bool on, const char* key) {
        QVariantMap option;
        option.insert(QStringLiteral("value"), on);
        option.insert(QStringLiteral("label"), qtTrId(key));
        return QVariant(option);
    };
    return QVariantList{
        row(true, "preferences.on"),
        row(false, "preferences.off"),
    };
}

QVariantList AndroidDocumentSession::normalizeSectionOptions() const
{
    const auto row = [](int measures, const char* key) {
        QVariantMap option;
        option.insert(QStringLiteral("value"), measures);
        option.insert(QStringLiteral("label"), qtTrId(key));
        return QVariant(option);
    };
    return QVariantList{
        row(4, "document.chart_section_every_4_measures"),
        row(2, "document.chart_section_every_2_measures"),
        row(0, "document.chart_section_none"),
    };
}

QVariantList AndroidDocumentSession::normalizeSyntaxOptions() const
{
    const auto row = [](const QString& token, const QString& label) {
        QVariantMap option;
        option.insert(QStringLiteral("value"), token);
        option.insert(QStringLiteral("label"), label);
        return QVariant(option);
    };
    return QVariantList{
        row(QStringLiteral("segment_preserving"),
            qtTrId("dialog.normalize.segment_preserving")),
        row(QStringLiteral("compact_single_line"),
            qtTrId("dialog.normalize.compact_single_line")),
    };
}

QVariantList AndroidDocumentSession::availableDifficulties() const {
    QVariantList rows;
    for (int id = 1; id <= 7; ++id) if (!workspace_.document().difficulty(id))
        rows.append(QVariantMap{{"id", id}, {"label", SimaiDocument::difficultyName(id)}});
    return rows;
}
QStringList AndroidDocumentSession::dirtyEditorKeys() const {
    QStringList keys;
    for (int id : workspace_.snapshot().dirtyDifficultyIds) keys.append("difficulty:" + QString::number(id));
    return keys;
}
QString AndroidDocumentSession::currentDifficultyLevel() const {
    const auto* difficulty = workspace_.document().difficulty(activeDifficulty());
    return difficulty ? difficulty->level : QString();
}
QString AndroidDocumentSession::currentDifficultyDesigner() const {
    const auto* difficulty = workspace_.document().difficulty(activeDifficulty());
    return difficulty ? difficulty->designer : QString();
}
void AndroidDocumentSession::setCurrentDifficultyLevel(const QString& value) {
    workspace_.updateDifficultyField(activeDifficulty(), ChartWorkspaceDifficultyField::Level, value);
}
void AndroidDocumentSession::setCurrentDifficultyDesigner(const QString& value) {
    workspace_.updateDifficultyField(activeDifficulty(), ChartWorkspaceDifficultyField::Designer, value);
}
QVariantList AndroidDocumentSession::bookmarksForDifficulty(int id) const {
    const auto* difficulty = workspace_.document().difficulty(id);
    if (!difficulty) return {};
    miacode::ui::EditorController parser;
    return parser.bookmarksForQml(difficulty->chart);
}
int AndroidDocumentSession::chartPosition(int line, int column) const {
    const auto text = chartText();
    int start = 0;
    for (int n = 1; n < line; ++n) {
        const int next = text.indexOf('\n', start);
        if (next < 0) return text.size();
        start = next + 1;
    }
    const int next = text.indexOf('\n', start);
    return qBound(start, start + qMax(0, column - 1), next < 0 ? int(text.size()) : next);
}
QVariantList AndroidDocumentSession::chartTransformMenu() const
{
    QVariantList rows;
    for (const miacode::ui::ChartTransformSpec& spec : miacode::ui::chartTransformSpecs()) {
        rows.append(QVariantMap{
            {QStringLiteral("id"), spec.id},
            {QStringLiteral("label"), qtTrId(spec.labelKey.toUtf8().constData())},
            {QStringLiteral("section"), spec.section},
        });
    }
    return rows;
}

QString AndroidDocumentSession::chartTransformMoreLabel() const
{
    return qtTrId("action.transform.more");
}

QVariantMap AndroidDocumentSession::transformChartSelection(
    const QString& text, int anchor, int position, const QString& opId) const
{
    QVariantMap transaction;
    transaction.insert(QStringLiteral("consumed"), false);
    transaction.insert(QStringLiteral("hasEdit"), false);
    transaction.insert(QStringLiteral("undoGroup"), true);
    transaction.insert(QStringLiteral("changed"), 0);

    const int begin = qBound(0, qMin(anchor, position), text.size());
    const int end = qBound(begin, qMax(anchor, position), text.size());
    if (begin >= end) {
        // Every one of these edits a range, so an empty selection is a
        // no-target, not a whole-chart shortcut.
        transaction.insert(QStringLiteral("error"), QStringLiteral("no_selection"));
        return transaction;
    }

    const auto specs = miacode::ui::chartTransformSpecs();
    const auto spec = std::find_if(specs.cbegin(), specs.cend(),
                                   [&opId](const miacode::ui::ChartTransformSpec& candidate) {
                                       return candidate.id == opId;
                                   });
    if (spec == specs.cend()) {
        transaction.insert(QStringLiteral("error"), QStringLiteral("unknown_transform"));
        return transaction;
    }

    const QString selected = text.mid(begin, end - begin);
    int changed = 0;
    QString replacement;
    if (spec->apply) {
        replacement = spec->apply(selected, {text.left(begin), text.mid(end)}, &changed);
    } else {
        const QString transformedFull =
            opId == QStringLiteral("transform.reset_tap_notes")
                ? miacode::chart_transform::resetTapNotesInSelection(text, begin, end, &changed)
                : miacode::chart_transform::clearCompleteElementsInSelection(text, begin, end, &changed);
        // The transform rewrites the whole text; the selection's new extent is
        // whatever is left once the untouched tail is accounted for.
        const int untouchedSuffix = text.size() - end;
        replacement = transformedFull.mid(begin, transformedFull.size() - untouchedSuffix - begin);
    }

    transaction.insert(QStringLiteral("consumed"), true);
    transaction.insert(QStringLiteral("changed"), changed);
    if (replacement == selected) {
        return transaction;
    }

    const int transformedEnd = begin + replacement.size();
    const bool forward = position >= anchor;
    transaction.insert(QStringLiteral("hasEdit"), true);
    transaction.insert(QStringLiteral("replacementStart"), begin);
    transaction.insert(QStringLiteral("replacementEnd"), end);
    transaction.insert(QStringLiteral("replacementText"), replacement);
    transaction.insert(QStringLiteral("anchor"), forward ? begin : transformedEnd);
    transaction.insert(QStringLiteral("position"), forward ? transformedEnd : begin);
    return transaction;
}

QVariantMap AndroidDocumentSession::selectionBeatSummary(
    const QString& text, int anchor, int position) const
{
    const miacode::chart_selection::ChartSelectionBeatSummary summary =
        miacode::chart_selection::summarizeChartSelectionBeats(text, anchor, position);
    QVariantList parts;
    for (const miacode::chart_selection::ChartSelectionBeatPart& part : summary.parts) {
        parts.append(QVariantMap{
            {QStringLiteral("count"), part.count},
            {QStringLiteral("denominator"), part.denominator},
        });
    }
    return QVariantMap{
        {QStringLiteral("totalCommaCount"), summary.totalCommaCount},
        {QStringLiteral("parts"), parts},
        {QStringLiteral("exact"), summary.exact},
    };
}

QVariantMap AndroidDocumentSession::normalizeChartSelection(
    const QString& text, int anchor, int position, const QVariantMap& options) const
{
    // Shaped as one of SourceEditor's editor transactions so the existing apply
    // path records it on the undo stack like any other edit.
    QVariantMap transaction;
    transaction.insert(QStringLiteral("consumed"), false);
    transaction.insert(QStringLiteral("hasEdit"), false);
    transaction.insert(QStringLiteral("undoGroup"), true);

    const int begin = qBound(0, qMin(anchor, position), text.size());
    const int end = qBound(begin, qMax(anchor, position), text.size());
    // No selection means the whole chart, matching the Widgets entry.
    const int selectionStart = begin == end ? 0 : begin;
    const int selectionEnd = begin == end ? text.size() : end;

    const auto normalized = miacode::chart_transform::normalizeChartSelectionText(
        text,
        selectionStart,
        selectionEnd,
        miacode::simai::buildTimingMetadata(workspace_.document()),
        normalizeOptionsFromVariant(options));
    if (!normalized.ok) {
        transaction.insert(QStringLiteral("error"), normalized.errorMessage);
        return transaction;
    }

    const QString replacement = miacode::chart_transform::composeNormalizedSelectionReplacement(
        text, selectionStart, selectionEnd, normalized.text);
    transaction.insert(QStringLiteral("consumed"), true);
    if (replacement == text.mid(selectionStart, selectionEnd - selectionStart)) {
        // Already normalized: consumed but with no edit, so the caller can say
        // so instead of recording an undo step that changes nothing.
        return transaction;
    }

    const int transformedEnd = selectionStart + replacement.size();
    const bool forward = position >= anchor;
    transaction.insert(QStringLiteral("hasEdit"), true);
    transaction.insert(QStringLiteral("replacementStart"), selectionStart);
    transaction.insert(QStringLiteral("replacementEnd"), selectionEnd);
    transaction.insert(QStringLiteral("replacementText"), replacement);
    transaction.insert(QStringLiteral("anchor"), forward ? selectionStart : transformedEnd);
    transaction.insert(QStringLiteral("position"), forward ? transformedEnd : selectionStart);
    return transaction;
}


} // namespace miacode::android

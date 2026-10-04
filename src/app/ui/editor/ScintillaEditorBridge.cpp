#include "editor/ScintillaEditorBridge.h"
#include "editor/SimaiCompletionCatalog.h"
#include <QClipboard>
#include <QGuiApplication>
#include <QInputMethod>
#include <QInputMethodEvent>
#include <QKeyEvent>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QScopedValueRollback>

namespace miacode::ui {
using miacode::editor::normalizeSimaiInput;
ScintillaEditorBridge::ScintillaEditorBridge(QQuickItem* parent)
    : ScintillaQuick_item(parent), document_(*this), styler_(*this, document_)
{
    send(SCI_SETCODEPAGE, SC_CP_UTF8);
    send(SCI_USEPOPUP, SC_POPUP_NEVER);
    connect(this, &ScintillaQuick_item::textChanged, this, &ScintillaEditorBridge::textMutated);
    connect(this, &ScintillaQuick_item::cursorPositionChanged, this, [this] {
        emit selectionChanged();
        emit cursorRectangleChanged();
        if (!synchronizing_ && !handlingIme_) {
            publishContext(!programmatic_);
            if (controller_ && !programmatic_)
                controller_->updateCompletionForQml(document_.text(), cursorPosition());
        }
    });
    connect(this, &ScintillaQuick_item::updateUi, this, [this](Scintilla::Update) {
        emit selectionChanged();
        emit cursorRectangleChanged();
        emit availabilityChanged();
    });
    connect(this, &QQuickItem::activeFocusChanged, this, [this] { publishContext(false); });
    connect(this, &ScintillaQuick_item::vertical_scroll_value_changed, this, &ScintillaEditorBridge::cursorRectangleChanged);
    connect(this, &ScintillaQuick_item::horizontal_scroll_value_changed, this, &ScintillaEditorBridge::cursorRectangleChanged);
    connect(this, &ScintillaQuick_item::notificationReceived, this, [this](const ScintillaQuick_notification& notification) {
        if ((int(notification.modificationType) & SC_MOD_CONTAINER) && touchUndoAnchors_.contains(notification.token))
            pendingTouchAnchor_ = touchUndoAnchors_.value(notification.token);
    });
    connect(this, &ScintillaQuick_item::styleNeeded, this, [this](Scintilla::Position) { if (ready_) styler_.style(); });
    connect(QGuiApplication::clipboard(), &QClipboard::dataChanged, this, &ScintillaEditorBridge::availabilityChanged);
    connect(this, &ScintillaQuick_item::marginClicked, this, [this](Scintilla::Position position, Scintilla::KeyMod, int margin) {
        if (margin != 1) return;
        const int line = send(SCI_LINEFROMPOSITION, position) + 1;
        const QRectF rect = positionToRectangle(document_.utf16Position(position));
        emit bookmarkMenuRequested(line, 16, rect.y());
    });
}
ScintillaEditorBridge::~ScintillaEditorBridge()
{
    if (syncController_) syncController_->setEditorReadiness(-1, 0, false);
}
void ScintillaEditorBridge::componentComplete()
{
    ScintillaQuick_item::componentComplete();
    ready_ = true;
    refreshSettings();
    synchronizeDocument();
}
void ScintillaEditorBridge::setDocumentSession(DocumentModel* value)
{
    if (documentSession_ == value) return;
    if (documentSession_) disconnect(documentSession_, nullptr, this, nullptr);
    documentSession_ = value;
    if (value) {
        connect(value, &DocumentModel::chartTextChanged, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::documentStateChanged, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::documentReplaced, this, &ScintillaEditorBridge::synchronizeDocument);
        connect(value, &DocumentModel::syntaxIssuesChanged, this, &ScintillaEditorBridge::refreshDiagnostics);
    }
    synchronizeDocument();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setController(EditorController* value)
{
    if (controller_ == value) return;
    if (controller_) disconnect(controller_, nullptr, this, nullptr);
    controller_ = value;
    if (value) connect(value, &EditorController::settingsChanged, this, &ScintillaEditorBridge::refreshSettings);
    refreshSettings();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setSyncController(miacode::EditorSyncController* value)
{
    if (syncController_ == value) return;
    if (syncController_) {
        syncController_->setEditorReadiness(-1, 0, false);
        disconnect(syncController_, nullptr, this, nullptr);
    }
    syncController_ = value;
    if (value) {
        connect(value, &miacode::EditorSyncController::navigationRequested, this, &ScintillaEditorBridge::navigate);
        connect(value, &miacode::EditorSyncController::followChanged, this, &ScintillaEditorBridge::applyFollow);
        connect(value, &miacode::EditorSyncController::touchPadAuthoringRequested, this, &ScintillaEditorBridge::touchAuthoring);
    }
    publishContext(false);
    emit bindingsChanged();
}
void ScintillaEditorBridge::setAnalysisSession(AnalysisModel* value)
{
    if (analysisSession_ == value) return;
    if (analysisSession_) disconnect(analysisSession_, nullptr, this, nullptr);
    analysisSession_ = value;
    if (value) connect(value, &AnalysisModel::changed, this, &ScintillaEditorBridge::refreshDiagnostics);
    refreshDiagnostics();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setNavigationVisible(bool value)
{
    if (navigationVisible_ == value) return;
    navigationVisible_ = value;
    publishContext(false);
    applyFollow();
    emit bindingsChanged();
}
void ScintillaEditorBridge::setPalette(const QVariantMap& value)
{
    palette_ = value;
    styler_.setPalette(value);
    emit paletteChanged();
}
void ScintillaEditorBridge::setBlockSpacing(int value)
{
    blockSpacing_ = qMax(0, value);
    send(SCI_SETEXTRAASCENT, blockSpacing_ / 2);
    send(SCI_SETEXTRADESCENT, blockSpacing_ - blockSpacing_ / 2);
    emit appearanceChanged();
}
void ScintillaEditorBridge::setScrollPastEnd(bool value)
{
    scrollPastEnd_ = value;
    send(SCI_SETENDATLASTLINE, !value);
    emit appearanceChanged();
}
int ScintillaEditorBridge::cursorPosition() const { return document_.utf16Position(send(SCI_GETCURRENTPOS)); }
void ScintillaEditorBridge::setCursorPosition(int value) { select(value, value); }
int ScintillaEditorBridge::selectionStart() const { return document_.utf16Position(send(SCI_GETSELECTIONSTART)); }
int ScintillaEditorBridge::selectionEnd() const { return document_.utf16Position(send(SCI_GETSELECTIONEND)); }
QString ScintillaEditorBridge::selectedText() const { return document_.text().mid(selectionStart(), selectionEnd() - selectionStart()); }
QRectF ScintillaEditorBridge::positionToRectangle(int position) const
{
    const int byte = document_.bytePosition(position);
    return QRectF(send(SCI_POINTXFROMPOSITION, 0, byte), send(SCI_POINTYFROMPOSITION, 0, byte), 2,
                  send(SCI_TEXTHEIGHT, send(SCI_LINEFROMPOSITION, byte)));
}
QRectF ScintillaEditorBridge::cursorRectangle() const { return positionToRectangle(cursorPosition()); }
int ScintillaEditorBridge::cursorLine() const { return send(SCI_LINEFROMPOSITION, send(SCI_GETCURRENTPOS)) + 1; }
int ScintillaEditorBridge::cursorColumn() const
{
    return cursorPosition() - document_.utf16Position(send(SCI_POSITIONFROMLINE, cursorLine() - 1)) + 1;
}
void ScintillaEditorBridge::synchronizeDocument()
{
    if (!ready_ || !documentSession_ || synchronizing_ || handlingIme_) return;
    QScopedValueRollback guard(synchronizing_, true);
    const auto generation = documentSession_->documentOpenGeneration();
    const QString scope = QStringLiteral("difficulty:%1").arg(documentSession_->currentDifficultyId());
    const bool identityChanged = generation_ != generation || scope != document_.scope();
    if (imeComposing_ && !identityChanged) { refreshDiagnostics(); return; }
    if (identityChanged && imeComposing_) {
        QScopedValueRollback imeGuard(handlingIme_, true);
        QGuiApplication::inputMethod()->reset();
        imeComposing_ = false;
        emit imeComposingChanged();
    }
    if (generation_ != generation) {
        QGuiApplication::inputMethod()->reset();
        imeComposing_ = false;
        emit imeComposingChanged();
        document_.clear();
        touchUndoAnchors_.clear();
        generation_ = generation;
    }
    document_.activate(scope, documentSession_->chartText());
    if (controller_) controller_->setDocumentContext(documentSession_->currentDifficultyId(), documentSession_->documentRevision());
    refreshDecorations();
    publishContext(false);
    emit selectionChanged();
    emit cursorRectangleChanged();
    emit availabilityChanged();
}
void ScintillaEditorBridge::textMutated()
{
    document_.refresh();
    if (!ready_ || synchronizing_ || handlingIme_) return;
    if (documentSession_) {
        QScopedValueRollback guard(synchronizing_, true);
        documentSession_->setChartText(document_.text());
        if (controller_) controller_->setDocumentContext(documentSession_->currentDifficultyId(), documentSession_->documentRevision());
    }
    refreshDecorations();
    publishContext(!programmatic_);
    if (controller_) {
        controller_->setUndoAvailability(canUndo(), canRedo());
        controller_->updateCompletionForQml(document_.text(), cursorPosition());
    }
    emit selectionChanged();
    emit availabilityChanged();
}
void ScintillaEditorBridge::refreshDecorations()
{
    styler_.style();
    bookmarks_ = controller_ ? controller_->bookmarksForQml(document_.text()) : QVariantList{};
    styler_.bookmarks(bookmarks_);
    refreshDiagnostics();
    applyFollow();
    emit bookmarksChanged();
}
void ScintillaEditorBridge::refreshDiagnostics()
{
    QVariantList validation, muri;
    if (documentSession_ && !documentSession_->validationPending()
        && documentSession_->validationRevision() == documentSession_->documentRevision())
        validation = documentSession_->syntaxIssues();
    if (documentSession_ && analysisSession_ && !analysisSession_->pending() && analysisSession_->available()
        && analysisSession_->difficultyId() == documentSession_->currentDifficultyId()
        && analysisSession_->revision() == documentSession_->documentRevision())
        muri = analysisSession_->muriRows();
    styler_.diagnostics(validation, muri);
}
void ScintillaEditorBridge::refreshSettings()
{
    if (!controller_) return;
    send(SCI_SETOVERTYPE, controller_->overwriteMode());
    setFlag(ItemAcceptsInputMethod, !controller_->imeInputDisabled());
}
void ScintillaEditorBridge::publishContext(bool userCaret)
{
    if (!ready_ || !documentSession_ || !syncController_) return;
    const int difficulty = documentSession_->currentDifficultyId();
    const auto revision = documentSession_->documentRevision();
    syncController_->setEditorReadiness(difficulty, revision, navigationVisible_);
    syncController_->setEditorContext(difficulty, revision, document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition(),
                                      hasActiveFocus(), imeComposing_, cursorLine(), cursorColumn(),
                                      userCaret && !imeComposing_ && !programmatic_ && !synchronizing_);
}
void ScintillaEditorBridge::revealPosition(int utf16, bool center)
{
    const int line = send(SCI_LINEFROMPOSITION, document_.bytePosition(utf16));
    const int visible = send(SCI_VISIBLEFROMDOCLINE, line);
    const int first = send(SCI_GETFIRSTVISIBLELINE);
    const int page = qMax(1, int(send(SCI_LINESONSCREEN)));
    if (center || visible < first || visible >= first + page)
        send(SCI_SETFIRSTVISIBLELINE, qMax(0, visible - page / 2));
}
void ScintillaEditorBridge::applyFollow()
{
    const bool active = documentSession_ && syncController_ && syncController_->followActive()
        && syncController_->followDifficultyId() == documentSession_->currentDifficultyId()
        && syncController_->followRevision() == documentSession_->documentRevision();
    styler_.follow(active, active ? syncController_->followStart() : 0,
                  active ? syncController_->followEnd() : 0, active ? syncController_->followCaret() : 0);
    if (active && navigationVisible_ && syncController_->followReveal())
        revealPosition(syncController_->followCaret(), syncController_->followPlaybackActive());
}
void ScintillaEditorBridge::navigate(qulonglong sequence, int difficulty, qulonglong revision, int start, int end, bool focus, bool reveal)
{
    const bool accepted = navigationVisible_ && documentSession_ && !imeComposing_
        && difficulty == documentSession_->currentDifficultyId() && revision == documentSession_->documentRevision()
        && start >= 0 && end >= start && end <= document_.text().size();
    if (accepted) {
        QScopedValueRollback guard(programmatic_, true);
        select(start, end);
        if (focus) forceActiveFocus();
        if (reveal) revealPosition(end, true);
        publishContext(false);
    }
    syncController_->acknowledgeNavigation(sequence, accepted);
}
void ScintillaEditorBridge::touchAuthoring(const QString& pad, QChar separator, int difficulty, qulonglong revision, int anchor, int position)
{
    if (!documentSession_ || !controller_ || imeComposing_ || difficulty != documentSession_->currentDifficultyId()
        || revision != documentSession_->documentRevision()) return;
    QScopedValueRollback guard(programmatic_, true);
    const auto tx = controller_->touchPadAuthoringForQml(document_.text(), anchor, position, pad, separator);
    if (applyEditorTransaction(tx)) {
        forceActiveFocus();
        syncController_->setTouchPadPreviewAnchor(difficulty, documentSession_->documentRevision(), document_.text(), tx.value(QStringLiteral("touchTokenStart")).toInt());
    }
}
void ScintillaEditorBridge::undo()
{
    QGuiApplication::inputMethod()->commit();
    if (controller_) controller_->closeCompletion();
    send(SCI_UNDO);
    publishTouchUndoAnchor();
}
void ScintillaEditorBridge::redo()
{
    QGuiApplication::inputMethod()->commit();
    if (controller_) controller_->closeCompletion();
    send(SCI_REDO);
    publishTouchUndoAnchor();
}
void ScintillaEditorBridge::publishTouchUndoAnchor()
{
    if (pendingTouchAnchor_ >= 0 && documentSession_ && syncController_)
        syncController_->setTouchPadPreviewAnchor(documentSession_->currentDifficultyId(), documentSession_->documentRevision(), document_.text(), pendingTouchAnchor_);
    pendingTouchAnchor_ = -1;
}
void ScintillaEditorBridge::cut() { send(SCI_CUT); }
void ScintillaEditorBridge::copy() { send(SCI_COPY); }
void ScintillaEditorBridge::paste() { send(SCI_PASTE); }
void ScintillaEditorBridge::selectAll() { send(SCI_SELECTALL); publishContext(true); }
void ScintillaEditorBridge::select(int anchor, int position)
{
    send(SCI_SETSEL, document_.bytePosition(anchor), document_.bytePosition(position));
    emit selectionChanged();
    emit cursorRectangleChanged();
    publishContext(!programmatic_);
}
void ScintillaEditorBridge::selectCurrentLine()
{
    const int line = cursorLine() - 1;
    select(document_.utf16Position(send(SCI_POSITIONFROMLINE, line)), document_.utf16Position(send(SCI_GETLINEENDPOSITION, line)));
}
void ScintillaEditorBridge::jumpToLine(int line)
{
    const int position = document_.utf16Position(send(SCI_POSITIONFROMLINE, qBound(0, line - 1, int(send(SCI_GETLINECOUNT)) - 1)));
    setCursorPosition(position);
    centerCursorInView();
    forceActiveFocus();
}
void ScintillaEditorBridge::centerCursorInView() { revealPosition(cursorPosition(), true); }
bool ScintillaEditorBridge::applyEditorTransaction(const QVariantMap& tx)
{
    if (!tx.value(QStringLiteral("consumed")).toBool()) return false;
    {
        QScopedValueRollback guard(synchronizing_, true);
        if (tx.value(QStringLiteral("hasEdit")).toBool()) {
            send(SCI_BEGINUNDOACTION);
            send(SCI_SETTARGETSTART, document_.bytePosition(tx.value(QStringLiteral("replacementStart")).toInt()));
            send(SCI_SETTARGETEND, document_.bytePosition(tx.value(QStringLiteral("replacementEnd")).toInt()));
            const QByteArray replacement = tx.value(QStringLiteral("replacementText")).toString().toUtf8();
            sends(SCI_REPLACETARGET, replacement.size(), replacement.constData());
            if (tx.contains(QStringLiteral("touchTokenStart"))) {
                const int token = nextTouchToken_++;
                touchUndoAnchors_.insert(token, tx.value(QStringLiteral("touchTokenStart")).toInt());
                send(SCI_ADDUNDOACTION, token, 0);
            }
            send(SCI_ENDUNDOACTION);
            document_.refresh();
        }
        select(tx.value(QStringLiteral("anchor")).toInt(), tx.value(QStringLiteral("position")).toInt());
    }
    textMutated();
    return true;
}
void ScintillaEditorBridge::acceptCompletionFromPopup()
{
    if (!controller_) return;
    applyEditorTransaction(controller_->acceptCompletionForQml(document_.text(), document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition()));
    forceActiveFocus();
}
void ScintillaEditorBridge::dropDocument(const QString& key) { document_.drop(key); }
void ScintillaEditorBridge::configureSearch(const QString& query, const QString& replacement, bool matchCase, bool wholeWord)
{
    setFindText(query); setReplacementText(replacement);
    setFindOptions((matchCase ? SCFIND_MATCHCASE : 0) | (wholeWord ? SCFIND_WHOLEWORD : 0));
}
void ScintillaEditorBridge::seekPreviewToCaret()
{
    if (syncController_ && documentSession_ && selectionStart() == selectionEnd())
        syncController_->seekPreviewToEditorLocation(documentSession_->currentDifficultyId(), documentSession_->documentRevision(), cursorLine(), cursorColumn());
}
void ScintillaEditorBridge::exportSelectionRange()
{
    if (syncController_ && documentSession_ && selectionStart() != selectionEnd())
        syncController_->requestSelectionRangeExport(documentSession_->currentDifficultyId(), documentSession_->documentRevision(), selectionStart(), selectionEnd());
}
bool ScintillaEditorBridge::applyNormalization(const QVariantMap& options)
{
    return documentSession_ && applyEditorTransaction(documentSession_->normalizeChartSelection(document_.text(), selectionStart(), selectionEnd(), options));
}
bool ScintillaEditorBridge::applyChartTransform(const QString& operation)
{
    return documentSession_ && applyEditorTransaction(documentSession_->transformChartSelection(document_.text(), selectionStart(), selectionEnd(), operation));
}
bool ScintillaEditorBridge::createBookmarkAtLine(int line, const QString& title) { return controller_ && applyEditorTransaction(controller_->createBookmarkForQml(document_.text(), line, title)); }
bool ScintillaEditorBridge::renameBookmarkAtLine(int line, const QString& title) { return controller_ && applyEditorTransaction(controller_->renameBookmarkForQml(document_.text(), line, title)); }
bool ScintillaEditorBridge::deleteBookmarkAtLine(int line) { return controller_ && applyEditorTransaction(controller_->deleteBookmarkForQml(document_.text(), line)); }
void ScintillaEditorBridge::beginUserInteraction()
{
    if (syncController_ && documentSession_)
        syncController_->beginPointerInteraction(documentSession_->currentDifficultyId(), documentSession_->documentRevision());
}
void ScintillaEditorBridge::keyPressEvent(QKeyEvent* event)
{
    if (event->matches(QKeySequence::Find)) { emit findRequested(); event->accept(); return; }
    if (event->key() == Qt::Key_Menu || (event->key() == Qt::Key_F10 && event->modifiers().testFlag(Qt::ShiftModifier))) {
        const auto rect = cursorRectangle(); emit contextMenuRequested(rect.x(), rect.bottom()); event->accept(); return;
    }
    if (event->matches(QKeySequence::Undo)) { undo(); event->accept(); return; }
    if (event->matches(QKeySequence::Redo)) { redo(); event->accept(); return; }
    if (event->matches(QKeySequence::Cut)) { cut(); event->accept(); return; }
    if (event->matches(QKeySequence::Copy)) { copy(); event->accept(); return; }
    if (event->matches(QKeySequence::Paste)) { paste(); event->accept(); return; }
    if (event->matches(QKeySequence::SelectAll)) { selectAll(); event->accept(); return; }
    if (event->key() == Qt::Key_PageUp || event->key() == Qt::Key_PageDown) beginUserInteraction();
    if (controller_) {
        const QString input = event->text();
        // Only DSL pairing, completion and half-width conversion intercept
        // typing. Scintilla executes ordinary editing and clipboard commands.
        const bool smartGlyph = controller_->autoCompletionEnabled() &&
            (QStringLiteral("(){}[]h").contains(input) && input.size() == 1);
        const bool convert = controller_->halfWidthInputEnabled() && normalizeSimaiInput(input) != input;
        const bool completionKey = controller_->completionActive() &&
            (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down || event->key() == Qt::Key_Escape
             || event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter || event->key() == Qt::Key_Tab);
        if (completionKey || smartGlyph || convert || event->key() == Qt::Key_Backspace) {
            const auto tx = controller_->processKeyForQml(document_.text(), document_.utf16Position(send(SCI_GETANCHOR)), cursorPosition(), input, event->key(), int(event->modifiers()));
            if (applyEditorTransaction(tx) || tx.value(QStringLiteral("suppressFallbackInsert")).toBool()) { event->accept(); return; }
        }
    }
    ScintillaQuick_item::keyPressEvent(event);
    publishContext(true);
}
void ScintillaEditorBridge::mousePressEvent(QMouseEvent* event)
{
    beginUserInteraction();
    ScintillaQuick_item::mousePressEvent(event);
    if (event->button() == Qt::RightButton)
        emit contextMenuRequested(event->position().x(), event->position().y());
    publishContext(true);
}
void ScintillaEditorBridge::mouseReleaseEvent(QMouseEvent* event)
{
    ScintillaQuick_item::mouseReleaseEvent(event);
    publishContext(true);
    if (event->button() == Qt::LeftButton && (event->modifiers() & (Qt::ControlModifier | Qt::MetaModifier))) seekPreviewToCaret();
}
void ScintillaEditorBridge::wheelEvent(QWheelEvent* event)
{
    beginUserInteraction();
    ScintillaQuick_item::wheelEvent(event);
}
void ScintillaEditorBridge::inputMethodEvent(QInputMethodEvent* event)
{
    if (controller_ && controller_->imeInputDisabled()) { event->accept(); return; }
    {
        QScopedValueRollback guard(handlingIme_, true);
        imeComposing_ = !event->preeditString().isEmpty() && event->commitString().isEmpty();
        QInputMethodEvent normalized(event->preeditString(), event->attributes());
        const QString commit = controller_ && controller_->halfWidthInputEnabled() ? normalizeSimaiInput(event->commitString()) : event->commitString();
        normalized.setCommitString(commit, event->replacementStart(), event->replacementLength());
        const bool committed = !commit.isEmpty();
        if (committed) send(SCI_BEGINUNDOACTION);
        ScintillaQuick_item::inputMethodEvent(&normalized);
        document_.refresh();
        if (controller_ && committed && commit.size() == 1 && controller_->autoCompletionEnabled()
            && !controller_->overwriteMode() && !controller_->completionActive()) {
            const QChar glyph = commit.front();
            const QChar closing = miacode::editor::closingBracketFor(glyph);
            bool closingPresent = false;
            if (!closing.isNull()) {
                const int caret = cursorPosition();
                if (caret >= document_.text().size() || document_.text().at(caret) != closing) {
                    const QByteArray closingText = QString(closing).toUtf8();
                    sends(SCI_INSERTTEXT, send(SCI_GETCURRENTPOS), closingText.constData());
                    document_.refresh();
                }
                closingPresent = true;
            }
            controller_->triggerCompletion(glyph, document_.text(), cursorPosition(), closingPresent);
        }
        if (committed) send(SCI_ENDUNDOACTION);
        event->setAccepted(normalized.isAccepted());
    }
    emit imeComposingChanged();
    if (imeComposing_) {
        if (syncController_) syncController_->setTouchPadControlHold(false);
        publishContext(false);
    } else textMutated();
}
}

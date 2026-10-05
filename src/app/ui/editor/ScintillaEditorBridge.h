#pragma once

#include "editor/ScintillaDslStyler.h"
#include "editor/EditorController.h"
#include "document/DocumentModel.h"
#include "document/AnalysisModel.h"
#include "app/services/EditorSyncController.h"
#include <QtQmlIntegration/qqmlintegration.h>
#include <optional>

namespace miacode::ui {
struct ScintillaQuickForeign
{
    Q_GADGET
    QML_FOREIGN(ScintillaQuick_item)
    QML_ANONYMOUS
};

class ScintillaEditorBridge : public ScintillaQuick_item
{
    Q_OBJECT
    QML_NAMED_ELEMENT(ScintillaEditor)
    Q_PROPERTY(DocumentModel* documentSession READ documentSession WRITE setDocumentSession NOTIFY bindingsChanged)
    Q_PROPERTY(EditorController* controller READ controller WRITE setController NOTIFY bindingsChanged)
    Q_PROPERTY(miacode::EditorSyncController* syncController READ syncController WRITE setSyncController NOTIFY bindingsChanged)
    Q_PROPERTY(AnalysisModel* analysisSession READ analysisSession WRITE setAnalysisSession NOTIFY bindingsChanged)
    Q_PROPERTY(bool navigationVisible READ navigationVisible WRITE setNavigationVisible NOTIFY bindingsChanged)
    // The hosting QML surface owns the theme fill, including wallpaper opacity.
    Q_PROPERTY(QColor sceneBackgroundColor READ sceneBackgroundColor CONSTANT)
    Q_PROPERTY(QVariantMap palette READ palette WRITE setPalette NOTIFY paletteChanged)
    Q_PROPERTY(int blockSpacing READ blockSpacing WRITE setBlockSpacing NOTIFY appearanceChanged)
    Q_PROPERTY(bool scrollPastEnd READ scrollPastEnd WRITE setScrollPastEnd NOTIFY appearanceChanged)
    Q_PROPERTY(QFont effectiveFont READ effectiveFont NOTIFY layoutChanged)
    Q_PROPERTY(int lineHeight READ lineHeight NOTIFY layoutChanged)
    Q_PROPERTY(int cursorPosition READ cursorPosition WRITE setCursorPosition NOTIFY selectionChanged)
    Q_PROPERTY(int selectionStart READ selectionStart NOTIFY selectionChanged)
    Q_PROPERTY(int selectionEnd READ selectionEnd NOTIFY selectionChanged)
    Q_PROPERTY(QString selectedText READ selectedText NOTIFY selectionChanged)
    Q_PROPERTY(QRectF cursorRectangle READ cursorRectangle NOTIFY cursorRectangleChanged)
    Q_PROPERTY(QRectF followCursorRectangle READ followCursorRectangle NOTIFY followVisualChanged)
    Q_PROPERTY(bool followCaretVisible READ followCaretVisible NOTIFY followVisualChanged)
    Q_PROPERTY(int cursorLine READ cursorLine NOTIFY selectionChanged)
    Q_PROPERTY(int cursorColumn READ cursorColumn NOTIFY selectionChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY availabilityChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY availabilityChanged)
    Q_PROPERTY(bool canPaste READ canPaste NOTIFY availabilityChanged)
    Q_PROPERTY(QVariantList bookmarks READ bookmarks NOTIFY bookmarksChanged)
    Q_PROPERTY(bool imeComposing READ imeComposing NOTIFY imeComposingChanged)
public:
    QColor sceneBackgroundColor() const { return Qt::transparent; }
    explicit ScintillaEditorBridge(QQuickItem* parent = nullptr);
    ~ScintillaEditorBridge() override;
    DocumentModel* documentSession() const { return documentSession_; }
    EditorController* controller() const { return controller_; }
    miacode::EditorSyncController* syncController() const { return syncController_; }
    AnalysisModel* analysisSession() const { return analysisSession_; }
    void setDocumentSession(DocumentModel* value);
    void setController(EditorController* value);
    void setSyncController(miacode::EditorSyncController* value);
    void setAnalysisSession(AnalysisModel* value);
    bool navigationVisible() const { return navigationVisible_; }
    void setNavigationVisible(bool value);
    QVariantMap palette() const { return palette_; }
    void setPalette(const QVariantMap& value);
    int blockSpacing() const { return blockSpacing_; }
    void setBlockSpacing(int value);
    bool scrollPastEnd() const { return scrollPastEnd_; }
    void setScrollPastEnd(bool value);
    QFont effectiveFont() const { return effectiveFont_; }
    int lineHeight() const { return lineHeight_; }
    int cursorPosition() const;
    void setCursorPosition(int value);
    int selectionStart() const;
    int selectionEnd() const;
    QString selectedText() const;
    QRectF cursorRectangle() const;
    QRectF followCursorRectangle() const;
    bool followCaretVisible() const;
    int cursorLine() const;
    int cursorColumn() const;
    bool canUndo() const { return send(SCI_CANUNDO); }
    bool canRedo() const { return send(SCI_CANREDO); }
    bool canPaste() const { return send(SCI_CANPASTE); }
    QVariantList bookmarks() const { return bookmarks_; }
    bool imeComposing() const { return imeComposing_; }
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void cut();
    Q_INVOKABLE void copy();
    Q_INVOKABLE void paste();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void beginViewportInteraction() { beginUserInteraction(); }
    Q_INVOKABLE void select(int anchor, int position);
    Q_INVOKABLE void selectCurrentLine();
    Q_INVOKABLE void jumpToLine(int line);
    Q_INVOKABLE int positionAt(qreal x, qreal y) const;
    Q_INVOKABLE QRectF textPositionRectangle(int position) const;
    Q_INVOKABLE bool applyEditorTransaction(const QVariantMap& transaction);
    Q_INVOKABLE void acceptCompletionFromPopup();
    Q_INVOKABLE void dropDocument(const QString& key);
    Q_INVOKABLE void configureSearch(const QString& query, const QString& replacement, bool matchCase, bool wholeWord);
    Q_INVOKABLE void exportSelectionRange();
    Q_INVOKABLE bool applyNormalization(const QVariantMap& options);
    Q_INVOKABLE bool applyChartTransform(const QString& operation);
    Q_INVOKABLE bool createBookmarkAtLine(int line, const QString& title);
    Q_INVOKABLE bool renameBookmarkAtLine(int line, const QString& title);
    Q_INVOKABLE bool deleteBookmarkAtLine(int line);
signals:
    void bindingsChanged();
    void paletteChanged();
    void appearanceChanged();
    void layoutChanged();
    void scenePositionChanged();
    void selectionChanged();
    void cursorRectangleChanged();
    void followVisualChanged();
    void availabilityChanged();
    void bookmarksChanged();
    void imeComposingChanged();
    void findRequested();
    void contextMenuRequested(qreal x, qreal y);
    void bookmarkMenuRequested(int line, qreal x, qreal y);
protected:
    void componentComplete() override;
    void updatePolish() override;
    void geometryChange(const QRectF& newGeometry, const QRectF& oldGeometry) override;
    QSGNode* updatePaintNode(QSGNode* oldNode, UpdatePaintNodeData* data) override;
    void keyPressEvent(QKeyEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void inputMethodEvent(QInputMethodEvent* event) override;
private:
    void centerCursorInView();
    void publishLayout();
    void preserveViewport();
    void trackScenePosition();
    void seekPreviewToCaret();
    QRectF positionToRectangle(int position) const;
    void synchronizeDocument();
    void textMutated();
    void refreshDecorations();
    void refreshDiagnostics();
    void refreshSettings();
    void publishContext(bool userCaret);
    void applyFollow(bool reveal = false);
    void refreshSelection(bool userCaret);
    void revealPosition(int utf16, bool center);
    void navigate(qulonglong sequence, int difficulty, qulonglong revision, int start, int end, bool focus, bool reveal);
    void touchAuthoring(const QString& pad, QChar separator, int difficulty, qulonglong revision, int anchor, int position);
    void beginUserInteraction();
    void publishTouchUndoAnchor();
    ScintillaDocumentAdapter document_;
    ScintillaDslStyler styler_;
    QPointer<DocumentModel> documentSession_;
    QPointer<EditorController> controller_;
    QPointer<miacode::EditorSyncController> syncController_;
    QPointer<AnalysisModel> analysisSession_;
    QVariantMap palette_;
    QVariantList bookmarks_;
    QFont effectiveFont_;
    int lineHeight_ = 0;
    QSizeF layoutSize_;
    QRectF cursorRectangle_;
    QRectF anchorRectangle_;
    QRectF followCursorRectangle_;
    qreal viewportY_ = 0;
    std::optional<qreal> viewportToRestore_;
    QVector<QMetaObject::Connection> sceneConnections_;
    struct NavigationRequest {
        qulonglong sequence;
        int difficulty;
        qulonglong revision;
        qulonglong generation;
        int start;
        int end;
        bool focus;
        bool reveal;
    };
    std::optional<NavigationRequest> pendingNavigation_;
    qulonglong generation_ = 0;
    bool ready_ = false;
    bool synchronizing_ = false;
    bool programmatic_ = false;
    bool imeComposing_ = false;
    bool handlingIme_ = false;
    bool navigationVisible_ = false;
    int reportedAnchor_ = -1;
    int reportedCaret_ = -1;
    qreal wheelRemainder_ = 0;
    int blockSpacing_ = 0;
    bool scrollPastEnd_ = true;
    struct TouchUndoAnchor {
        QString scope;
        int position;
    };
    QHash<int, TouchUndoAnchor> touchUndoAnchors_;
    int nextTouchToken_ = 1;
    int pendingTouchAnchor_ = -1;
};
}

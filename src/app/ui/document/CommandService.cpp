#include "app/ui/document/CommandService.h"
#include "app/ui/chrome/ShortcutCommands.h"

#include "app/ui/document/DocumentModel.h"
#include "app/platform/DocumentFileAccess.h"


namespace miacode::ui {
CommandService::CommandService(
    DocumentModel& document,
    miacode::DocumentBridge*& bridgeSlot,
    QObject* parent)
    : QObject(parent)
    , bridgeSlot_(&bridgeSlot)
    , document_(&document)
{
}

void CommandService::whenDocumentMayBeLeft(std::function<void()> proceed)
{
    if (bridge() == nullptr) {
        return;
    }
    bridge()->requestLeaveDocument([proceed = std::move(proceed)](bool mayLeave) {
        if (mayLeave && proceed) {
            proceed();
        }
    });
}

void CommandService::openDocument(const QUrl& fileUrl)
{
    whenDocumentMayBeLeft([this, fileUrl]() {
#ifdef Q_OS_IOS
        const QString path = miacode::document_access::resolvePath(
            fileUrl.isLocalFile() ? fileUrl.toLocalFile() : fileUrl.toString());
        if (!miacode::document_access::hasFolderAccess(path)) {
            miacode::document_access::pickChartFolder(this, path, [this](const QString& chart) {
                document_->openFile(QUrl::fromLocalFile(chart));
            });
            return;
        }
        document_->openFile(QUrl::fromLocalFile(path));
#else
        document_->openFile(fileUrl);
#endif
    });
}

#ifdef Q_OS_IOS
void CommandService::openChartFolder()
{
    whenDocumentMayBeLeft([this] {
        miacode::document_access::pickChartFolder(this, {}, [this](const QString& chart) {
            document_->openFile(QUrl::fromLocalFile(chart));
        });
    });
}
#endif

void CommandService::openRecentDocument(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    openDocument(QUrl::fromLocalFile(miacode::document_access::resolvePath(path)));
}

void CommandService::newDocument()
{
    whenDocumentMayBeLeft([this]() { document_->createDocumentFromPickedAudio(); });
}

void CommandService::restoreBackupDocument(const QString& path)
{
    if (path.trimmed().isEmpty()) {
        return;
    }
    whenDocumentMayBeLeft([this, path]() { document_->restoreBackup(path); });
}

void CommandService::closeDocument()
{
    if (document_ == nullptr || !document_->hasDocument()) {
        return;
    }
    whenDocumentMayBeLeft([this]() { document_->closeDocument(); });
}

bool CommandService::saveDocument() { return document_->save(); }
bool CommandService::saveWholeDocument() { return document_->saveWholeDocument(); }
bool CommandService::saveDocumentAs(const QUrl& fileUrl) { return document_->saveAs(fileUrl); }
void CommandService::discardDocumentChanges() { document_->discardChanges(); }
void CommandService::selectDifficulty(int id) { document_->selectDifficulty(id); }
bool CommandService::addDifficulty(int id) { return document_->addDifficulty(id); }
bool CommandService::removeDifficulty(int id) { return document_->removeDifficulty(id); }
bool CommandService::applyDesignerSlots(
    const QVariantList& slotValues, bool unified, const QString& canonicalName)
{
    return document_->applyDesignerSlots(slotValues, unified, canonicalName);
}

QStringList CommandService::shortcutCommandIds() const
{
    // Chart transforms only, and the shell binds them to the editor rather than
    // back to MainWindow: a transform acts on the editor's selection, which is
    // the one thing this side does not have. Preview commands bind straight to
    // the preview session instead of coming through here.
    return miacode::ui::shortcutCommandIds();
}

} // namespace miacode::ui

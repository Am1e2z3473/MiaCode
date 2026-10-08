#include "app/platform/DocumentFileAccess.h"
#include "common/DebugLog.h"

#include <QDir>
#include <QFileInfo>
#include <QGuiApplication>
#include <QPointer>
#include <QRegularExpression>
#include <QSettings>
#include <QStandardPaths>
#include <QWindow>
#import <UIKit/UIKit.h>
#import <UniformTypeIdentifiers/UniformTypeIdentifiers.h>
#import <objc/runtime.h>

namespace {
QString normalizedPath(const QString& path)
{
    const QString clean = QDir::cleanPath(path);
    return clean.startsWith(QStringLiteral("/private/var/")) ? clean.mid(8) : clean;
}

QString bookmarkSettingsPath()
{
    return QDir(QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation))
        .filePath(QStringLiteral("document-access.ini"));
}

struct FolderGrant {
    QString originalPath;
    QString currentPath;
    NSURL* url;
};
struct FolderAccess {
    QList<FolderGrant> grants;
    bool loaded = false;
    ~FolderAccess() {
        for (const auto& grant : grants) {
            [grant.url stopAccessingSecurityScopedResource];
            [grant.url release];
        }
    }
};
FolderAccess& access() { static FolderAccess value; return value; }

void loadGrants()
{
    auto& state = access();
    if (state.loaded) return;
    state.loaded = true;
    QSettings settings(bookmarkSettingsPath(), QSettings::IniFormat);
    const QVariantMap bookmarks = settings.value(QStringLiteral("document_access/folder_bookmarks")).toMap();
    for (auto it = bookmarks.cbegin(); it != bookmarks.cend(); ++it) {
        const QByteArray bytes = it.value().toByteArray();
        NSData* data = [NSData dataWithBytes:bytes.constData() length:bytes.size()];
        BOOL stale = NO;
        NSError* error = nil;
        NSURL* url = [NSURL URLByResolvingBookmarkData:data options:NSURLBookmarkResolutionWithoutUI
            relativeToURL:nil bookmarkDataIsStale:&stale error:&error];
        if (url && [url startAccessingSecurityScopedResource]) {
            state.grants.append({it.key(), normalizedPath(QString::fromNSString(url.path)), [url retain]});
            if (stale) {
                NSData* refreshed = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark
                    includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
                if (refreshed) {
                    QVariantMap updated = settings.value(QStringLiteral("document_access/folder_bookmarks")).toMap();
                    updated.insert(it.key(), QByteArray(static_cast<const char*>(refreshed.bytes), refreshed.length));
                    settings.setValue(QStringLiteral("document_access/folder_bookmarks"), updated);
                }
            }
        }
    }
}

bool inside(const QString& path, const QString& directory)
{
    const QString file = normalizedPath(path);
    const QString root = normalizedPath(directory);
    return file == root || file.startsWith(root + QLatin1Char('/'));
}

bool retainFolder(NSURL* url)
{
    loadGrants();
    const QString path = normalizedPath(QString::fromNSString(url.path));
    if (inside(path, QDir::homePath())) return true;
    for (const auto& grant : access().grants)
        if (path == grant.currentPath) return true;
    if (![url startAccessingSecurityScopedResource]) return false;
    NSError* error = nil;
    NSData* bookmark = [url bookmarkDataWithOptions:NSURLBookmarkCreationMinimalBookmark
        includingResourceValuesForKeys:nil relativeToURL:nil error:&error];
    if (!bookmark) {
        [url stopAccessingSecurityScopedResource];
        return false;
    }
    QSettings settings(bookmarkSettingsPath(), QSettings::IniFormat);
    QVariantMap bookmarks = settings.value(QStringLiteral("document_access/folder_bookmarks")).toMap();
    bookmarks.insert(path, QByteArray(static_cast<const char*>(bookmark.bytes), bookmark.length));
    settings.setValue(QStringLiteral("document_access/folder_bookmarks"), bookmarks);
    access().grants.append({path, path, [url retain]});
    return true;
}
char pickerDelegateKey;
}

@interface MiaCodeChartFolderPicker : NSObject <UIDocumentPickerDelegate> {
@public
    QPointer<QObject> owner;
    QString initialPath;
    std::function<void(const QString&)> accepted;
}
@end
@implementation MiaCodeChartFolderPicker
- (void)documentPicker:(UIDocumentPickerViewController*)controller didPickDocumentsAtURLs:(NSArray<NSURL*>*)urls
{
    if (!owner || urls.count == 0) return;
    NSURL* folder = urls.firstObject;
    if (!retainFolder(folder)) {
        miacode::debug_log::appendLine(miacode::debug_log::Channel::Runtime,
            QStringLiteral("document/access"), QStringLiteral("action=folder_authorization_failed"));
        return;
    }
    const QString directory = normalizedPath(QString::fromNSString(folder.path));
    const QString previous = miacode::document_access::resolvePath(initialPath);
    const QString chart = inside(previous, directory) && QFileInfo(previous).isFile()
        ? previous : QDir(directory).filePath(QStringLiteral("maidata.txt"));
    accepted(chart);
}
@end

namespace miacode::document_access {
QString persistentPath(const QString& path)
{
    const QString normalized = normalizedPath(path);
    const QString home = normalizedPath(QDir::homePath());
    if (path.isEmpty()) return {};
    if (inside(normalized, home))
        return QStringLiteral("~/") + QDir(home).relativeFilePath(normalized);
    loadGrants();
    for (const auto& grant : access().grants)
        if (inside(normalized, grant.currentPath))
            return grant.originalPath + normalized.mid(grant.currentPath.size());
    return normalized;
}

QString resolvePath(const QString& stored)
{
    const QString reference = stored.isEmpty() ? QString() : normalizedPath(stored);
    if (reference.isEmpty()) return {};
    loadGrants();
    if (reference.startsWith(QStringLiteral("~/")))
        return QDir::home().filePath(reference.mid(2));
    for (const auto& grant : access().grants)
        if (inside(reference, grant.originalPath))
            return grant.currentPath + reference.mid(grant.originalPath.size());
    // Migrate existing absolute paths from an earlier installation when the
    // same document is present in this application's Documents directory.
    static const QRegularExpression legacy(QStringLiteral(
        "/Containers/Data/Application/[0-9A-Fa-f-]{36}/(Documents/.*)$"));
    const auto match = legacy.match(reference);
    if (match.hasMatch() && !QFileInfo::exists(reference)) {
        const QString local = QDir::home().filePath(match.captured(1));
        if (QFileInfo::exists(local)) return local;
    }
    return reference;
}

bool hasFolderAccess(const QString& path)
{
    const QString resolved = resolvePath(path);
    if (inside(resolved, QDir::homePath())) return true;
    for (const auto& grant : access().grants)
        if (inside(resolved, grant.currentPath)) return true;
    return false;
}

void pickChartFolder(QObject* owner, const QString& initialPath,
                     std::function<void(const QString&)> accepted)
{
    QWindow* focused = QGuiApplication::focusWindow();
    UIView* view = focused ? reinterpret_cast<UIView*>(focused->winId()) : nil;
    UIViewController* presenter = view.window.rootViewController;
    while (presenter.presentedViewController)
        presenter = presenter.presentedViewController;
    if (!presenter) return;
    UIDocumentPickerViewController* picker = [[UIDocumentPickerViewController alloc]
        initForOpeningContentTypes:@[UTTypeFolder] asCopy:NO];
    MiaCodeChartFolderPicker* delegate = [[MiaCodeChartFolderPicker alloc] init];
    delegate->owner = owner;
    delegate->initialPath = initialPath;
    delegate->accepted = std::move(accepted);
    picker.delegate = delegate;
    objc_setAssociatedObject(picker, &pickerDelegateKey, delegate, OBJC_ASSOCIATION_RETAIN_NONATOMIC);
    [delegate release];
    if (!initialPath.isEmpty())
        picker.directoryURL = [NSURL fileURLWithPath:QFileInfo(resolvePath(initialPath)).absolutePath().toNSString()];
    [presenter presentViewController:picker animated:YES completion:nil];
    [picker release];
}
}

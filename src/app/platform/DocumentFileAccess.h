#pragma once

#include <QString>
#ifdef Q_OS_IOS
#include <functional>
class QObject;
#endif

namespace miacode::document_access {
#ifdef Q_OS_IOS
QString persistentPath(const QString& path);
QString resolvePath(const QString& reference);
bool hasFolderAccess(const QString& path);
void pickChartFolder(QObject* owner, const QString& initialPath,
                     std::function<void(const QString&)> accepted);
#else
inline QString persistentPath(const QString& path) { return path; }
inline QString resolvePath(const QString& path) { return path; }
#endif
}

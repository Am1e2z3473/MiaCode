#include "AndroidFileRequests.h"
#include "ExportDestination.h"
#include <QJsonDocument>
#include <QCoreApplication>
#ifdef Q_OS_ANDROID
#include <QJniObject>
#include <QJniEnvironment>
#endif

namespace miacode::android {
AndroidFileRequests::AndroidFileRequests(UiRequestService& requests, QObject* parent)
    : QObject(parent), requests_(requests) {
#ifdef Q_OS_ANDROID
    connect(&requests_, &UiRequestService::fileRequested, this, [this](const QString& id, const QVariantMap& request) {
        pending_.insert(id);
        if (!QNativeInterface::QAndroidApplication::isActivityContext()) {
            deliver({{"requestId", id}, {"ok", false}, {"error", tr("安卓文件选择器当前不可用")}}); return;
        }
        auto payload = QJsonObject::fromVariantMap(request); payload.insert("requestId", id);
        QNativeInterface::QAndroidApplication::context().callMethod<void>("requestUiFile",
            QJniObject::fromString(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact))).object<jstring>());
        QJniEnvironment env;
        if (env.checkAndClearExceptions()) deliver({{"requestId", id}, {"ok", false}, {"error", tr("无法启动安卓文件选择器")}});
    });
#endif
}
void AndroidFileRequests::deliver(const QJsonObject& result) {
    const auto id = result.value("requestId").toString();
    if (!pending_.remove(id)) return;
    if (!result.value("ok").toBool()) {
        requests_.cancelFileRequest(id);
        if (!result.value("cancelled").toBool()) requests_.postNotice(NoticeSeverity::Error, tr("文件操作失败"), result.value("error").toString());
        return;
    }
    const auto path = result.value("localPath").toString();
    if (path.isEmpty()) { requests_.cancelFileRequest(id); return; }
    if (result.value("saveMode").toBool())
        rememberExportDestination(path, result.value("uri").toString(), result.value("selectFolder").toBool(), result.value("displayPath").toString());
    requests_.submitFileResult(id, QUrl::fromLocalFile(path));
}
}

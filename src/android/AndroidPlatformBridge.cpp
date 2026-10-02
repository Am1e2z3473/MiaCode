#include "AndroidPlatformBridge.h"
#include "AndroidDocumentSession.h"
#include <QCoreApplication>
#include <QJniObject>
#include <QJsonDocument>
#include <QPointer>
#include <jni.h>

namespace {
QPointer<miacode::android::AndroidPlatformBridge> bridge;
}

extern "C" JNIEXPORT void JNICALL
Java_org_miacode_android_MiaCodeActivity_deliverResult(JNIEnv* env, jclass, jstring json)
{
    const jsize length = env->GetStringLength(json);
    const jchar* chars = env->GetStringChars(json, nullptr);
    const QString result = QString::fromUtf16(reinterpret_cast<const char16_t*>(chars), length);
    env->ReleaseStringChars(json, chars);
    // Java I/O runs off the UI thread; all workspace transactions run in Qt's thread.
    QMetaObject::invokeMethod(QCoreApplication::instance(), [result] {
        if (bridge) bridge->deliver(result);
    }, Qt::QueuedConnection);
}

namespace miacode::android {
AndroidPlatformBridge::AndroidPlatformBridge(AndroidDocumentSession* session)
    : QObject(session), session_(session)
{
    bridge = this;
    connect(session, &AndroidDocumentSession::ioRequested, this,
        [this](const QString& kind, const QString& uri, const QString& payload) {
        const auto activity = QNativeInterface::QAndroidApplication::context();
        if (!QNativeInterface::QAndroidApplication::isActivityContext()) {
            session_->completeIo(QJsonObject{{"kind", kind}, {"ok", false},
                {"error", tr("安卓 Activity 当前不可用")}});
            return;
        }
        activity.callMethod<void>("dispatch",
            QJniObject::fromString(kind).object<jstring>(),
            QJniObject::fromString(uri).object<jstring>(),
            QJniObject::fromString(payload).object<jstring>());
    });
}
AndroidPlatformBridge::~AndroidPlatformBridge() { bridge.clear(); }
void AndroidPlatformBridge::deliver(const QString& json)
{
    const auto document = QJsonDocument::fromJson(json.toUtf8());
    if (!document.isObject()) return;
    if (document.object().value("kind").toString() == "chartExportCancel") emit chartExportCancelled();
    else if (document.object().value("kind").toString() == "uiFile") emit uiFileResult(document.object());
    else if (document.object().value("kind").toString() == "exportPublish") emit exportPublicationUpdate(document.object());
    else session_->completeIo(document.object());
}
}

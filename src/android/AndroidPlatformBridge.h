#pragma once
#include <QObject>
#include <QJsonObject>

namespace miacode::android {
class AndroidDocumentSession;
class AndroidPlatformBridge final : public QObject
{
    Q_OBJECT
public:
    explicit AndroidPlatformBridge(AndroidDocumentSession* session);
    ~AndroidPlatformBridge() override;
    void deliver(const QString& json);
signals:
    void chartExportCancelled();
    void uiFileResult(const QJsonObject& result);
    void exportPublicationUpdate(const QJsonObject& result);
private:
    AndroidDocumentSession* session_;
};
}

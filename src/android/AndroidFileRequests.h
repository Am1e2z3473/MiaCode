#pragma once
#include "app/services/UiRequestService.h"
#include <QJsonObject>
#include <QSet>

namespace miacode::android {
// Converts the existing v2 file request contract into private working files
// backed by user-selected SAF documents, without exposing URIs as local paths.
class AndroidFileRequests final : public QObject {
    Q_OBJECT
public:
    explicit AndroidFileRequests(UiRequestService& requests, QObject* parent = nullptr);
    void deliver(const QJsonObject& result);
private:
    UiRequestService& requests_;
    QSet<QString> pending_;
};
}

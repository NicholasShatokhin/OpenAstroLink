#pragma once
#include <QByteArray>
#include <QJsonObject>
#include <QNetworkReply>
#include <QString>
#include <QUrl>
#include <QUrlQuery>
#include <optional>

namespace oas {
class HttpJsonClient {
public:
    struct Reply { int httpStatus{0}; QJsonObject json; QByteArray bytes; QString error; bool ok() const { return error.isEmpty() && httpStatus >= 200 && httpStatus < 300; } };
    Reply get(const QUrl &url, int timeoutMs = 5000);
    Reply postJson(const QUrl &url, const QJsonObject &body, int timeoutMs = 10000);
    Reply putForm(const QUrl &url, const QUrlQuery &form, int timeoutMs = 10000);
private:
    Reply wait(::QNetworkReply *reply, int timeoutMs);
};
QUrl appendPath(const QUrl &base, const QString &part);
}

#pragma once

#include "backends/http_json_client.h"
#include "core/astro_types.h"
#include <QHash>
#include <QUrl>

namespace oas {

// Client-side integration with Stellarium's optional Remote Control HTTP API.
// This is intentionally separate from the standard Telescope Control TCP
// bridge: the standard protocol only transports telescope position/GOTO.
class StellariumRemoteControlClient {
public:
    bool sendFrame(const QUrl &baseUrl, const SkyFrame &frame, QString *error = nullptr);
    bool hideFrame(const QUrl &baseUrl, QString *error = nullptr);
private:
    bool propertyMap(const QUrl &baseUrl,QHash<QString,QString> &ids,QString *error);
    bool setProperty(const QUrl &baseUrl,const QString &id,const QString &value,QString *error);
    bool setView(const QUrl &baseUrl,const EquatorialCoord &center,double fovDeg,QString *error);
    HttpJsonClient http_;
};

} // namespace oas

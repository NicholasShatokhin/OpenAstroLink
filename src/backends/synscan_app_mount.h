#pragma once
#include "core/interfaces.h"
#include <QHostAddress>
#include <QUdpSocket>

namespace oas {
// Network client for Sky-Watcher's SynScan App Protocol (default UDP 11881).
// The endpoint is the device running SynScan/SynScan Pro, NOT the Wi-Fi
// adapter/mount IP. This is the richer ITelescopeV3-shaped compatibility path.
class SynScanAppMount final : public IMount {
public:
    explicit SynScanAppMount(QString endpoint);
    QString id() const override{return "synscan-app:"+endpoint_;}
    QString displayName() const override{return "SynScan App network telescope";}
    QString backendName() const override{return "synscan-app";}
    ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &status,QString *error=nullptr) override;
    bool slewTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool abortMotion(QString *error=nullptr) override;
    bool syncTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool setTracking(bool enabled,TrackingRate rate=TrackingRate::Sidereal,QString *error=nullptr) override;
    bool park(bool enabled,QString *error=nullptr) override;
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr) override;
private:
    bool parseEndpoint(QHostAddress &host,quint16 &port,QString *error) const;
    bool command(const QString &command,QStringList *fields,QString *error,int timeoutMs=3000);
    bool boolGet(const QString &property,bool &value,QString *error=nullptr);
    QString endpoint_;
    QHostAddress host_;
    quint16 port_{11881};
    ConnectionState state_{ConnectionState::Disconnected};
    QUdpSocket socket_;
};
}

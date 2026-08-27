#pragma once
#include "core/interfaces.h"
#include <QTcpSocket>

namespace oas {
class SynScanNetworkMount final : public IMount {
public:
    explicit SynScanNetworkMount(QString endpoint);
    QString id() const override{return "synscan-wifi:"+endpoint_;}
    QString displayName() const override{return "SynScan network telescope";}
    QString backendName() const override{return "synscan-wifi";}
    ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &status,QString *error=nullptr) override;
    bool slewTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool abortMotion(QString *error=nullptr) override;
    bool syncTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool setTracking(bool enabled,QString *error=nullptr) override;
    bool park(bool enabled,QString *error=nullptr) override;
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr) override;
private:
    bool parseEndpoint(QString &host,quint16 &port,QString *error) const;
    bool exchange(const QByteArray &command,QByteArray &reply,int timeoutMs=3000,QString *error=nullptr);
    bool fixedRate(bool raAxis,bool positive,int rate,QString *error);
    QString endpoint_;
    ConnectionState state_{ConnectionState::Disconnected};
    QTcpSocket socket_;
    int model_{-1};
};
}

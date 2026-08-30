#pragma once
#include "core/interfaces.h"
#include "core/mount_geometry.h"
#include <QHostAddress>
#include <QUdpSocket>
#include <utility>

namespace oas {

// Direct SynScan Wi-Fi / EQDrive Wi-Fi transport.
//
// Despite the historic class name, this backend does NOT connect to SynScan
// Pro.  It talks directly to the mount/Wi-Fi adapter using the Sky-Watcher
// Motor Controller Command Set over UDP/11880.  SynScan Pro itself is exposed
// separately by the synscan-app backend on UDP/11881.
class SynScanNetworkMount final : public IMount {
public:
    explicit SynScanNetworkMount(QString endpoint, MountGeometryConfig geometry = {}, ObserverLocation observer = {});
    QString id() const override{return "synscan-wifi:"+endpoint_;}
    QString displayName() const override{return "Direct SynScan/EQDrive Wi-Fi mount";}
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
    bool manualSlew(int axis1Direction,int axis2Direction,int rateLevel,QString *error=nullptr) override;
    void configureGeometry(const MountGeometryConfig &c,const ObserverLocation &o) override { geometry_.configure(c,o); parked_=false; }
private:
    bool resolveEndpoint(QHostAddress &host,quint16 &port,QString *error);
    bool exchange(const QByteArray &command,QByteArray &reply,int timeoutMs=1800,QString *error=nullptr);
    bool axisQuery(char opcode,int axis,QByteArray &payload,QString *error=nullptr);
    bool axisCommand(char opcode,int axis,const QByteArray &payload={},QString *error=nullptr);
    bool readAxis(int axis,qint32 &position,bool &running,bool &gotoMode,bool &initialized,QString *error=nullptr);
    bool stopAxis(int axis,QString *error=nullptr);
    bool waitStopped(int axis,int timeoutMs,QString *error=nullptr);
    bool gotoAxisDelta(int axis,double deltaDeg,QString *error=nullptr);
    bool setManualRate(int axis,int direction,int rateLevel,QString *error=nullptr);
    double axisDeltaDeg(int axis,qint32 from,qint32 to) const;
    MechanicalAxes axesFromEncoder(qint32 p1,qint32 p2) const;

    QString endpoint_;
    ConnectionState state_{ConnectionState::Disconnected};
    QUdpSocket socket_;
    QHostAddress host_;
    quint16 port_{11880};
    quint32 countsPerRev1_{0},countsPerRev2_{0};
    quint32 timerFreq_{0};
    QString firmware1_,firmware2_;
    MountGeometryModel geometry_{};
    bool trackingRequested_{false};
    bool parked_{false};
};
}

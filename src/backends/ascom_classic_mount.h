#pragma once
#include "core/interfaces.h"
#include "core/equatorial_frames.h"
#include <QMutex>
#include <QProcess>

namespace oas {
class AscomClassicMount final : public IMount {
public:
    explicit AscomClassicMount(QString progId);
    ~AscomClassicMount() override;
    QString id() const override{return "ascom-classic:"+progId_;}
    QString displayName() const override{return name_.isEmpty()?progId_:name_;}
    QString backendName() const override{return "ascom-classic";}
    ConnectionState connectionState() const override{return state_;}
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &status,QString *error=nullptr) override;
    bool slewTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool abortMotion(QString *error=nullptr) override;
    bool syncTo(const EquatorialCoord &target,QString *error=nullptr) override;
    bool setTracking(bool enabled,TrackingRate rate=TrackingRate::Sidereal,QString *error=nullptr) override;
    bool setSiteTime(const ObserverLocation &site,const QDateTime &utc,QString *error=nullptr) override;
    bool park(bool enabled,QString *error=nullptr) override;
    bool setCurrentParkPosition(QString *error=nullptr) override;
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr) override;
    bool manualSlew(int axis1Direction,int axis2Direction,int rateLevel,QString *error=nullptr) override;
    QString destinationPierSide(const EquatorialCoord &target,QString *error=nullptr) override;
    static bool chooseTelescope(const QString &current,QString &selected,QString *error=nullptr);
    static bool setupTelescope(const QString &progId,QString *error=nullptr);
    static QString hostExecutable();
private:
    bool ensureHost(QString *error);
    bool request(const QJsonObject &request,QJsonObject *data,QString *error,int timeoutMs=10000);
    EquatorialCoord toAscomFrame(const EquatorialCoord &coord) const;
    EquatorialCoord fromAscomFrame(const EquatorialCoord &coord) const;
    void updateEquatorialSystem(int value);
    QString progId_,name_;
    int equatorialSystem_{-1};
    EquatorialFrame nativeFrame_{EquatorialFrame::J2000};
    bool equatorialFrameAssumed_{false};
    ConnectionState state_{ConnectionState::Disconnected};
    QProcess host_;
    mutable QMutex ioMutex_;
};
}

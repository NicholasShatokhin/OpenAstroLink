#pragma once
#include "core/interfaces.h"
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
    bool setTracking(bool enabled,QString *error=nullptr) override;
    bool park(bool enabled,QString *error=nullptr) override;
    bool pulseGuide(GuideDirection direction,int durationMs,QString *error=nullptr) override;
    static bool chooseTelescope(const QString &current,QString &selected,QString *error=nullptr);
    static bool setupTelescope(const QString &progId,QString *error=nullptr);
    static QString hostExecutable();
private:
    bool ensureHost(QString *error);
    bool request(const QJsonObject &request,QJsonObject *data,QString *error,int timeoutMs=10000);
    QString progId_,name_;
    ConnectionState state_{ConnectionState::Disconnected};
    QProcess host_;
    mutable QMutex ioMutex_;
};
}

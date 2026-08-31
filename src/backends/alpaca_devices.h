#pragma once
#include "backends/http_json_client.h"
#include "core/interfaces.h"

namespace oas {
class AlpacaDeviceBase {
public:
    AlpacaDeviceBase(QUrl base, int clientId = 12001) : base_(std::move(base)), clientId_(clientId) {}
protected:
    QUrl endpoint(const QString &name) const;
    QUrlQuery transactionForm() const;
    bool check(const HttpJsonClient::Reply &reply, QString *error) const;
    QUrl base_; mutable int transactionId_{1}; int clientId_{12001}; mutable HttpJsonClient http_;
};

class AlpacaMount final : public IMount, private AlpacaDeviceBase {
public:
    explicit AlpacaMount(QUrl deviceBase) : AlpacaDeviceBase(std::move(deviceBase)) {}
    QString id() const override { return "alpaca-mount"; }
    QString displayName() const override { return "ASCOM Alpaca telescope"; }
    QString backendName() const override { return "ascom-alpaca"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error=nullptr) override; void disconnectDevice() override;
    bool status(MountStatus&,QString *error=nullptr) override;
    bool slewTo(const EquatorialCoord&,QString *error=nullptr) override;
    bool abortMotion(QString *error=nullptr) override;
    bool syncTo(const EquatorialCoord&,QString *error=nullptr) override;
    bool setTracking(bool,TrackingRate rate=TrackingRate::Sidereal,QString *error=nullptr) override;
    bool park(bool,QString *error=nullptr) override;
    bool pulseGuide(GuideDirection,int,QString *error=nullptr) override;
private: ConnectionState state_{ConnectionState::Disconnected};
};

class AlpacaFocuser final : public IFocuser, private AlpacaDeviceBase {
public:
    explicit AlpacaFocuser(QUrl deviceBase) : AlpacaDeviceBase(std::move(deviceBase)) {}
    QString id() const override { return "alpaca-focuser"; }
    QString displayName() const override { return "ASCOM Alpaca focuser"; }
    QString backendName() const override { return "ascom-alpaca"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error=nullptr) override; void disconnectDevice() override;
    bool status(FocuserStatus&,QString *error=nullptr) override;
    bool moveAbsolute(int,QString *error=nullptr) override;
    bool moveRelative(int,QString *error=nullptr) override;
    bool halt(QString *error=nullptr) override;
private: ConnectionState state_{ConnectionState::Disconnected};
};
}

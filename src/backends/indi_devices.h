#pragma once
#ifdef OAS_HAVE_INDI

#include "core/interfaces.h"
#include <utility>

namespace oas {

struct IndiEndpoint {
    QString host{"127.0.0.1"};
    quint16 port{7624};
    QString device;
    static IndiEndpoint parse(const QString &text);
    QString normalized() const;
};

struct IndiDeviceInfo {
    QString name;
    QStringList properties;
};

QList<IndiDeviceInfo> discoverIndiDevices(const QString &host = "127.0.0.1",
                                          quint16 port = 7624,
                                          int timeoutMs = 2500,
                                          QString *error = nullptr);

class IndiXmlClient {
public:
    explicit IndiXmlClient(IndiEndpoint endpoint) : ep_(std::move(endpoint)) {}

    bool connectServer(QString *error);
    void close();
    bool setConnection(bool connected, QString *error);
    QByteArray queryProperty(const QString &name, int timeoutMs, QString *error);
    bool propertyExists(const QString &name, int timeoutMs = 1000);
    bool sendNumber(const QString &property,
                    const QList<QPair<QString, double>> &values,
                    QString *error);
    bool sendSwitch(const QString &property,
                    const QList<QPair<QString, bool>> &values,
                    QString *error);

    const IndiEndpoint &endpoint() const { return ep_; }

    static bool number(const QByteArray &xml, const QString &name, double &value);
    static bool switchValue(const QByteArray &xml, const QString &name, bool &value);
    static QString vectorState(const QByteArray &xml, const QString &property);

private:
    bool waitConnectionState(bool wantConnected, int timeoutMs, QString *error);

    IndiEndpoint ep_;
};

class IndiMount final : public IMount {
public:
    explicit IndiMount(QString endpoint);
    QString id() const override { return "indi-mount"; }
    QString displayName() const override { return deviceName_; }
    QString backendName() const override { return "indi"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &, QString *error = nullptr) override;
    bool slewTo(const EquatorialCoord &, QString *error = nullptr) override;
    bool abortMotion(QString *error = nullptr) override;
    bool syncTo(const EquatorialCoord &, QString *error = nullptr) override;
    bool setTracking(bool, TrackingRate rate = TrackingRate::Sidereal, QString *error = nullptr) override;
    bool park(bool, QString *error = nullptr) override;
    bool pulseGuide(GuideDirection, int, QString *error = nullptr) override;

private:
    IndiXmlClient client_;
    QString deviceName_;
    ConnectionState state_{ConnectionState::Disconnected};
};

class IndiFocuser final : public IFocuser {
public:
    explicit IndiFocuser(QString endpoint);
    QString id() const override { return "indi-focuser"; }
    QString displayName() const override { return deviceName_; }
    QString backendName() const override { return "indi"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(FocuserStatus &, QString *error = nullptr) override;
    bool moveAbsolute(int, QString *error = nullptr) override;
    bool moveRelative(int, QString *error = nullptr) override;
    bool halt(QString *error = nullptr) override;

private:
    IndiXmlClient client_;
    QString deviceName_;
    ConnectionState state_{ConnectionState::Disconnected};
};

} // namespace oas
#endif

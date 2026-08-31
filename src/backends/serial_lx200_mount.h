#pragma once
#include "core/interfaces.h"
#include <QSerialPort>

namespace oas {
class SerialLx200Mount final : public IMount {
public:
    SerialLx200Mount(QString portName, int baud = 9600) : portName_(std::move(portName)), baud_(baud) {}
    QString id() const override { return "lx200-mount"; }
    QString displayName() const override { return QString("LX200/SynScan on %1").arg(portName_); }
    QString backendName() const override { return "serial-lx200"; }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error = nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &status, QString *error = nullptr) override;
    bool slewTo(const EquatorialCoord &target, QString *error = nullptr) override;
    bool abortMotion(QString *error = nullptr) override;
    bool syncTo(const EquatorialCoord &target, QString *error = nullptr) override;
    bool setTracking(bool enabled, TrackingRate rate = TrackingRate::Sidereal, QString *error = nullptr) override;
    bool park(bool enabled, QString *error = nullptr) override;
    bool pulseGuide(GuideDirection direction, int durationMs, QString *error = nullptr) override;
private:
    QByteArray command(const QByteArray &cmd, int timeoutMs, QString *error);
    static bool parseRa(const QByteArray &s, double &deg);
    static bool parseDec(const QByteArray &s, double &deg);
    static QByteArray raString(double deg);
    static QByteArray decString(double deg);
    QString portName_; int baud_{9600}; ConnectionState state_{ConnectionState::Disconnected}; QSerialPort serial_;
    bool slewActive_{false};
    EquatorialCoord slewTarget_{};
};
}

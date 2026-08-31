#pragma once

#include "core/interfaces.h"
#include "core/mount_geometry.h"
#include <QJsonObject>
#include <memory>

namespace oas {
class OalDriverPluginLoader;

QString nativeBackendKey(const QString &driverId, const QString &deviceId);
bool parseNativeBackendKey(const QString &key, QString &driverId, QString &deviceId);

class NativeOalDeviceBase {
public:
    NativeOalDeviceBase(std::shared_ptr<OalDriverPluginLoader> loader, QJsonObject descriptor);
    virtual ~NativeOalDeviceBase() = default;

protected:
    bool invokeOk(const QString &method, const QJsonObject &request,
                  QJsonObject *data, QString *error, const QString &operationId={}) const;
    QJsonObject capabilities(QString *error=nullptr) const;
    QString driverId() const { return driverId_; }
    QString nativeDeviceId() const { return deviceId_; }
    QString deviceName() const { return name_; }
    QString backendKey() const { return nativeBackendKey(driverId_, deviceId_); }

    std::shared_ptr<OalDriverPluginLoader> loader_;
    QString driverId_;
    QString deviceId_;
    QString name_;
    QJsonObject descriptor_;
    ConnectionState state_{ConnectionState::Disconnected};
};

class NativeOalCamera final : public ICamera, private NativeOalDeviceBase {
public:
    NativeOalCamera(std::shared_ptr<OalDriverPluginLoader> loader, const QJsonObject &descriptor);
    ~NativeOalCamera() override { disconnectDevice(); }

    QString id() const override { return deviceId_; }
    QString displayName() const override { return name_; }
    QString backendName() const override { return backendKey(); }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool capture(const ExposureRequest &, CameraFrame &, QString *error=nullptr) override;
    bool canAbortExposure() const override;
    bool abortExposure(QString *error=nullptr) override;
    QSize sensorSize() const override;
    // Native vendor streaming transport used by Live/Finder when the driver
    // exposes camera.streaming.supported. This avoids emulating video with
    // repeated still exposures on cameras such as QHY.
    bool nativeLiveSupported() const;
    bool startNativeLive(const LiveViewRequest &request, QString *error=nullptr);
    bool nextNativeLiveFrame(CameraFrame &frame, int timeoutMs, QString *error=nullptr);
    bool stopNativeLive(QString *error=nullptr);
private:
    LiveViewRequest liveRequest_{};
    QSize sensorSizeCache_{};
};

class NativeOalMount final : public IMount, private NativeOalDeviceBase {
public:
    NativeOalMount(std::shared_ptr<OalDriverPluginLoader> loader, const QJsonObject &descriptor,
                   MountGeometryConfig geometry = {}, ObserverLocation observer = {});
    ~NativeOalMount() override { disconnectDevice(); }

    QString id() const override { return deviceId_; }
    QString displayName() const override { return name_; }
    QString backendName() const override { return backendKey(); }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool status(MountStatus &, QString *error=nullptr) override;
    bool slewTo(const EquatorialCoord &, QString *error=nullptr) override;
    bool abortMotion(QString *error=nullptr) override;
    bool syncTo(const EquatorialCoord &, QString *error=nullptr) override;
    bool setTracking(bool, TrackingRate rate=TrackingRate::Sidereal, QString *error=nullptr) override;
    bool setSiteTime(const ObserverLocation &, const QDateTime &, QString *error=nullptr) override;
    bool park(bool, QString *error=nullptr) override;
    bool pulseGuide(GuideDirection, int durationMs, QString *error=nullptr) override;
    bool manualSlew(int axis1Direction,int axis2Direction,int rateLevel,QString *error=nullptr) override;
    void configureGeometry(const MountGeometryConfig &c,const ObserverLocation &o) override;
private:
    bool rawAxisStatus(MechanicalAxes &axes, bool *slewing=nullptr, QString *error=nullptr, QJsonObject *diagnostics=nullptr) const;
    bool rawAxisGoto(const MechanicalAxes &axes, double maxAxisDeltaDeg, QString *error=nullptr);
    bool tryAutoHomeSync(QString *error=nullptr);
    bool geometryAware_{false};
    MountGeometryModel geometry_;
    bool parked_{false};
    bool parking_{false};
    MechanicalAxes parkTarget_{};
    QString alignmentSource_;
    QString homeAlignmentNote_;
    MechanicalAxes lastCommandedTarget_{};
};

class NativeOalFocuser final : public IFocuser, private NativeOalDeviceBase {
public:
    NativeOalFocuser(std::shared_ptr<OalDriverPluginLoader> loader, const QJsonObject &descriptor);
    ~NativeOalFocuser() override { disconnectDevice(); }

    QString id() const override { return deviceId_; }
    QString displayName() const override { return name_; }
    QString backendName() const override { return backendKey(); }
    ConnectionState connectionState() const override { return state_; }
    bool connectDevice(QString *error=nullptr) override;
    void disconnectDevice() override;
    bool status(FocuserStatus &, QString *error=nullptr) override;
    bool moveAbsolute(int, QString *error=nullptr) override;
    bool moveRelative(int, QString *error=nullptr) override;
    bool halt(QString *error=nullptr) override;
};

} // namespace oas

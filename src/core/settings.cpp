#include "core/settings.h"

namespace oas {
TelescopeProfile AppSettings::loadProfile() const {
    TelescopeProfile p;
    p.name = settings_.value("profile/name", p.name).toString();
    p.opticalDesign = settings_.value("profile/opticalDesign", p.opticalDesign).toString();
    p.apertureMm = settings_.value("profile/apertureMm", p.apertureMm).toDouble();
    p.centralObstructionMm = settings_.value("profile/centralObstructionMm", p.centralObstructionMm).toDouble();
    p.focalLengthMm = settings_.value("profile/focalMm", p.focalLengthMm).toDouble();
    p.pixelSizeUm = settings_.value("profile/pixelUm", p.pixelSizeUm).toDouble();
    p.sensorWidthPx = settings_.value("profile/sensorWidth", p.sensorWidthPx).toInt();
    p.sensorHeightPx = settings_.value("profile/sensorHeight", p.sensorHeightPx).toInt();
    p.guideScopeName = settings_.value("guide/name", p.guideScopeName).toString();
    p.guideApertureMm = settings_.value("guide/apertureMm", p.guideApertureMm).toDouble();
    p.guideFocalLengthMm = settings_.value("guide/focalMm", p.guideFocalLengthMm).toDouble();
    p.guidePixelSizeUm = settings_.value("guide/pixelUm", p.guidePixelSizeUm).toDouble();
    p.guideSensorWidthPx = settings_.value("guide/sensorWidth", p.guideSensorWidthPx).toInt();
    p.guideSensorHeightPx = settings_.value("guide/sensorHeight", p.guideSensorHeightPx).toInt();
    p.observer.latitudeDeg = settings_.value("observer/lat", 0.0).toDouble();
    p.observer.longitudeDeg = settings_.value("observer/lon", 0.0).toDouble();
    p.observer.elevationM = settings_.value("observer/elevation", 0.0).toDouble();
    return p;
}
void AppSettings::saveProfile(const TelescopeProfile &p) const {
    settings_.setValue("profile/name", p.name);
    settings_.setValue("profile/opticalDesign", p.opticalDesign);
    settings_.setValue("profile/apertureMm", p.apertureMm);
    settings_.setValue("profile/centralObstructionMm", p.centralObstructionMm);
    settings_.setValue("profile/focalMm", p.focalLengthMm);
    settings_.setValue("profile/pixelUm", p.pixelSizeUm);
    settings_.setValue("profile/sensorWidth", p.sensorWidthPx);
    settings_.setValue("profile/sensorHeight", p.sensorHeightPx);
    settings_.setValue("guide/name", p.guideScopeName);
    settings_.setValue("guide/apertureMm", p.guideApertureMm);
    settings_.setValue("guide/focalMm", p.guideFocalLengthMm);
    settings_.setValue("guide/pixelUm", p.guidePixelSizeUm);
    settings_.setValue("guide/sensorWidth", p.guideSensorWidthPx);
    settings_.setValue("guide/sensorHeight", p.guideSensorHeightPx);
    settings_.setValue("observer/lat", p.observer.latitudeDeg);
    settings_.setValue("observer/lon", p.observer.longitudeDeg);
    settings_.setValue("observer/elevation", p.observer.elevationM);
}
bool AppSettings::oalEnabled() const { return settings_.value("server/enabled", false).toBool(); }
quint16 AppSettings::oalPort() const { return settings_.value("server/port", 8080).value<quint16>(); }
bool AppSettings::wsEnabled() const { return settings_.value("server/wsEnabled", true).toBool(); }
quint16 AppSettings::wsPort() const { return settings_.value("server/wsPort", 8090).value<quint16>(); }
void AppSettings::saveServer(bool e, quint16 p, bool we, quint16 wp) const {
    settings_.setValue("server/enabled", e);
    settings_.setValue("server/port", p);
    settings_.setValue("server/wsEnabled", we);
    settings_.setValue("server/wsPort", wp);
}
bool AppSettings::stellariumEnabled() const { return settings_.value("stellarium/enabled", false).toBool(); }
quint16 AppSettings::stellariumPort() const { return settings_.value("stellarium/port", 10000).value<quint16>(); }
void AppSettings::saveStellarium(bool e, quint16 p) const {
    settings_.setValue("stellarium/enabled", e);
    settings_.setValue("stellarium/port", p);
}
DeviceBinding AppSettings::loadBinding(const QString &prefix) const {
    DeviceBinding b;
    b.backend = settings_.value(prefix + "/backend").toString();
    b.endpoint = settings_.value(prefix + "/endpoint").toString();
    b.autoConnect = settings_.value(prefix + "/autoConnect", false).toBool();
    return b;
}
void AppSettings::saveBinding(const QString &prefix, const DeviceBinding &b) const {
    settings_.setValue(prefix + "/backend", b.backend);
    settings_.setValue(prefix + "/endpoint", b.endpoint);
    settings_.setValue(prefix + "/autoConnect", b.autoConnect);
}
DeviceBinding AppSettings::cameraBinding() const { return loadBinding("devices/camera"); }
DeviceBinding AppSettings::guideCameraBinding() const { return loadBinding("devices/guideCamera"); }
DeviceBinding AppSettings::mountBinding() const { return loadBinding("devices/mount"); }
DeviceBinding AppSettings::focuserBinding() const { return loadBinding("devices/focuser"); }
void AppSettings::saveCameraBinding(const DeviceBinding &b) const { saveBinding("devices/camera", b); }
void AppSettings::saveGuideCameraBinding(const DeviceBinding &b) const { saveBinding("devices/guideCamera", b); }
void AppSettings::saveMountBinding(const DeviceBinding &b) const { saveBinding("devices/mount", b); }
void AppSettings::saveFocuserBinding(const DeviceBinding &b) const { saveBinding("devices/focuser", b); }
QString AppSettings::nativeSerialPort(const QString &driverId) const {
    QString key = driverId; key.replace('/', '_').replace('\\', '_').replace(':', '_');
    return settings_.value("nativeSerial/" + key + "/port").toString().trimmed();
}
void AppSettings::saveNativeSerialPort(const QString &driverId, const QString &port) const {
    QString key = driverId; key.replace('/', '_').replace('\\', '_').replace(':', '_');
    const QString value = port.trimmed();
    if (value.isEmpty()) settings_.remove("nativeSerial/" + key + "/port");
    else settings_.setValue("nativeSerial/" + key + "/port", value);
}
} // namespace oas

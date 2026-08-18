#include "core/settings.h"

namespace oas {
TelescopeProfile AppSettings::loadProfile() const {
    TelescopeProfile p;
    p.name = settings_.value("profile/name", p.name).toString();
    p.focalLengthMm = settings_.value("profile/focalMm", p.focalLengthMm).toDouble();
    p.pixelSizeUm = settings_.value("profile/pixelUm", p.pixelSizeUm).toDouble();
    p.sensorWidthPx = settings_.value("profile/sensorWidth", p.sensorWidthPx).toInt();
    p.sensorHeightPx = settings_.value("profile/sensorHeight", p.sensorHeightPx).toInt();
    p.observer.latitudeDeg = settings_.value("observer/lat", 0.0).toDouble();
    p.observer.longitudeDeg = settings_.value("observer/lon", 0.0).toDouble();
    p.observer.elevationM = settings_.value("observer/elevation", 0.0).toDouble();
    return p;
}
void AppSettings::saveProfile(const TelescopeProfile &p) const {
    settings_.setValue("profile/name", p.name);
    settings_.setValue("profile/focalMm", p.focalLengthMm);
    settings_.setValue("profile/pixelUm", p.pixelSizeUm);
    settings_.setValue("profile/sensorWidth", p.sensorWidthPx);
    settings_.setValue("profile/sensorHeight", p.sensorHeightPx);
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
DeviceBinding AppSettings::mountBinding() const { return loadBinding("devices/mount"); }
DeviceBinding AppSettings::focuserBinding() const { return loadBinding("devices/focuser"); }
void AppSettings::saveCameraBinding(const DeviceBinding &b) const { saveBinding("devices/camera", b); }
void AppSettings::saveMountBinding(const DeviceBinding &b) const { saveBinding("devices/mount", b); }
void AppSettings::saveFocuserBinding(const DeviceBinding &b) const { saveBinding("devices/focuser", b); }
} // namespace oas

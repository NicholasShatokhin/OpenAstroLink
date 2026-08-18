#pragma once
#include "core/astro_types.h"
#include <QSettings>

namespace oas {
struct DeviceBinding {
    QString backend;
    QString endpoint;
    bool autoConnect{false};
};

class AppSettings {
public:
    TelescopeProfile loadProfile() const;
    void saveProfile(const TelescopeProfile &profile) const;

    bool oalEnabled() const;
    quint16 oalPort() const;
    bool wsEnabled() const;
    quint16 wsPort() const;
    void saveServer(bool enabled, quint16 port, bool wsEnabled, quint16 wsPort) const;

    DeviceBinding cameraBinding() const;
    DeviceBinding mountBinding() const;
    DeviceBinding focuserBinding() const;
    void saveCameraBinding(const DeviceBinding &binding) const;
    void saveMountBinding(const DeviceBinding &binding) const;
    void saveFocuserBinding(const DeviceBinding &binding) const;
private:
    DeviceBinding loadBinding(const QString &prefix) const;
    void saveBinding(const QString &prefix, const DeviceBinding &binding) const;
    mutable QSettings settings_{"OpenAstroLink", "OpenAstroSuite"};
};
}

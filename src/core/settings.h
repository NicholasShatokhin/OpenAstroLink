#pragma once
#include "core/astro_types.h"
#include <QSettings>

namespace oas {
class AppSettings {
public:
    TelescopeProfile loadProfile() const;
    void saveProfile(const TelescopeProfile &profile) const;
    bool oalEnabled() const;
    quint16 oalPort() const;
    bool wsEnabled() const;
    quint16 wsPort() const;
    void saveServer(bool enabled, quint16 port, bool wsEnabled, quint16 wsPort) const;
private:
    mutable QSettings settings_{"OpenAstroLink", "OpenAstroSuite"};
};
}

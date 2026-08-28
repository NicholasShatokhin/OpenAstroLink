#pragma once

#include "core/astro_types.h"
#include <QDateTime>

namespace oas {

QString mountGeometryTypeName(MountGeometryType type);
MountGeometryType mountGeometryTypeFromString(const QString &text,
                                               MountGeometryType fallback = MountGeometryType::GermanEquatorial);

// Local apparent sidereal angle for mount geometry. Longitude is positive east.
double localSiderealTimeDeg(const QDateTime &utc, double longitudeDeg);

// Coordinate/axis model kept in OAL Core rather than in hardware drivers.
// A Sync establishes only the installation-specific encoder zero/sign offset;
// all time-dependent sky geometry is then recomputed from UTC and site longitude.
class MountGeometryModel {
public:
    MountGeometryModel() = default;
    MountGeometryModel(MountGeometryConfig config, ObserverLocation observer)
        : config_(std::move(config)), observer_(observer) {}

    void configure(MountGeometryConfig config, ObserverLocation observer);
    const MountGeometryConfig &config() const { return config_; }
    bool synced() const { return synced_; }
    QString pierSide() const { return pierSide_; }

    bool sync(const EquatorialCoord &sky, const MechanicalAxes &actualAxes,
              const QDateTime &utc = QDateTime::currentDateTimeUtc(), QString *error = nullptr);
    bool skyFromAxes(const MechanicalAxes &actualAxes, EquatorialCoord &skyJ2000,
                     const QDateTime &utc = QDateTime::currentDateTimeUtc(), QString *error = nullptr) const;
    bool axesForSky(const EquatorialCoord &sky, const MechanicalAxes &currentAxes,
                    MechanicalAxes &targetAxes,
                    const QDateTime &utc = QDateTime::currentDateTimeUtc(), QString *error = nullptr) const;

    MechanicalAxes parkAxes() const { return {config_.parkAxis1Deg, config_.parkAxis2Deg, true}; }
    int trackingAxis1Direction() const;

private:
    MechanicalAxes canonicalAxesForSky(const EquatorialCoord &skyJNow, const QDateTime &utc,
                                        QString *pierSide, QString *error) const;
    EquatorialCoord skyJNowFromCanonicalAxes(const MechanicalAxes &canonical,
                                             const QDateTime &utc, QString *error) const;

    MountGeometryConfig config_{};
    ObserverLocation observer_{};
    bool synced_{false};
    MechanicalAxes syncActual_{};
    MechanicalAxes syncCanonical_{};
    QString pierSide_{"unknown"};
};

} // namespace oas

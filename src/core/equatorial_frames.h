#pragma once

#include "core/astro_types.h"
#include <QDateTime>
#include <QString>

namespace oas {

QString equatorialFrameName(EquatorialFrame frame);
EquatorialFrame equatorialFrameFromString(const QString &text, EquatorialFrame fallback = EquatorialFrame::J2000);

// Convert between the J2000 mean equator/equinox and the mean equator/equinox
// of the supplied UTC date ("JNow" / of-date). The implementation uses the
// standard IAU-1976/Meeus precession rotation, which is appropriate for mount
// pointing and catalogue interchange at the precision required by the current
// OAL closed-loop plate-solving workflow. Nutation, annual aberration and
// atmospheric refraction are deliberately not folded into this coordinate
// frame conversion.
EquatorialCoord convertEquatorialFrame(const EquatorialCoord &coord,
                                       EquatorialFrame targetFrame,
                                       const QDateTime &utc = QDateTime::currentDateTimeUtc());

struct HorizontalCoord { double azDeg{0.0}; double altDeg{0.0}; };
struct GalacticCoord { double lDeg{0.0}; double bDeg{0.0}; };

// Horizontal azimuth is measured from true north through east (0..360 deg).
HorizontalCoord equatorialToHorizontal(const EquatorialCoord &coord,const ObserverLocation &observer,const QDateTime &utc=QDateTime::currentDateTimeUtc());
EquatorialCoord horizontalToEquatorial(const HorizontalCoord &coord,const ObserverLocation &observer,EquatorialFrame targetFrame=EquatorialFrame::J2000,const QDateTime &utc=QDateTime::currentDateTimeUtc());
GalacticCoord equatorialToGalactic(const EquatorialCoord &coord,const QDateTime &utc=QDateTime::currentDateTimeUtc());
EquatorialCoord galacticToEquatorial(const GalacticCoord &coord,EquatorialFrame targetFrame=EquatorialFrame::J2000,const QDateTime &utc=QDateTime::currentDateTimeUtc());
double localSiderealTimeDeg(const ObserverLocation &observer,const QDateTime &utc=QDateTime::currentDateTimeUtc());

} // namespace oas

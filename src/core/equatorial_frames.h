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

} // namespace oas

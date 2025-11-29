#include "starcatalog.h"
#include <QFile>
#include <QTextStream>
#include <cmath>

bool StarCatalog::loadCsv(const QString &path)
{
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text))
        return false;
    QTextStream ts(&f);
    stars_.clear();
    while (!ts.atEnd()) {
        auto line = ts.readLine();
        if (line.trimmed().isEmpty()) continue;
        // припустимо: name,ra_deg,dec_deg,mag
        auto parts = line.split(',');
        if (parts.size() < 4) continue;
        CatalogStar s;
        s.name  = parts[0].trimmed();
        s.raDeg = parts[1].toDouble();
        s.decDeg= parts[2].toDouble();
        s.mag   = parts[3].toDouble();
        stars_.push_back(s);
    }
    return true;
}

std::optional<CatalogStar> StarCatalog::findNearest(double raDeg, double decDeg, double maxDistDeg) const
{
    std::optional<CatalogStar> best;
    double bestDist = maxDistDeg;
    for (const auto &s : stars_) {
        double dra  = s.raDeg  - raDeg;
        double ddec = s.decDeg - decDeg;
        double d = std::sqrt(dra*dra + ddec*ddec);
        if (d < bestDist) {
            bestDist = d;
            best = s;
        }
    }
    return best;
}

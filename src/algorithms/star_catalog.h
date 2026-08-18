#pragma once
#include <QString>
#include <vector>

namespace oas {
struct CatalogStar { QString id; QString name; double raDeg{0}; double decDeg{0}; double magnitude{99}; };
class StarCatalog {
public:
    bool loadCsv(const QString &path, QString *error = nullptr);
    const std::vector<CatalogStar>& stars() const { return stars_; }
    std::vector<CatalogStar> around(double raDeg,double decDeg,double radiusDeg,double maxMagnitude=12.0,int limit=500) const;
private: std::vector<CatalogStar> stars_;
};
double angularDistanceDeg(double ra1,double dec1,double ra2,double dec2);
}

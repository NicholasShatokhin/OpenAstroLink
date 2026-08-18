#include "algorithms/star_catalog.h"
#include <QFile>
#include <QTextStream>
#include <algorithm>
#include <cmath>

namespace oas {
static constexpr double D2R=3.14159265358979323846/180.0;
double angularDistanceDeg(double a,double d,double b,double e){a*=D2R;d*=D2R;b*=D2R;e*=D2R;double c=std::sin(d)*std::sin(e)+std::cos(d)*std::cos(e)*std::cos(a-b);return std::acos(std::clamp(c,-1.0,1.0))/D2R;}
bool StarCatalog::loadCsv(const QString&p,QString*error){QFile f(p);if(!f.open(QIODevice::ReadOnly|QIODevice::Text)){if(error)*error=f.errorString();return false;}stars_.clear();QTextStream s(&f);bool first=true;while(!s.atEnd()){QString line=s.readLine().trimmed();if(line.isEmpty()||line.startsWith('#'))continue;auto v=line.split(',');if(first&&v.value(1).contains("ra",Qt::CaseInsensitive)){first=false;continue;}first=false;if(v.size()<4)continue;bool ok1=false,ok2=false,ok3=false;double ra=v[1].toDouble(&ok1),dec=v[2].toDouble(&ok2),mag=v[3].toDouble(&ok3);if(!ok1||!ok2||!ok3)continue;stars_.push_back({v.value(0).trimmed(),v.value(4).trimmed(),ra,dec,mag});}std::sort(stars_.begin(),stars_.end(),[](auto&a,auto&b){return a.magnitude<b.magnitude;});return !stars_.empty();}
std::vector<CatalogStar> StarCatalog::around(double ra,double dec,double radius,double maxMag,int limit)const{std::vector<CatalogStar> o;for(const auto&s:stars_)if(s.magnitude<=maxMag&&angularDistanceDeg(ra,dec,s.raDeg,s.decDeg)<=radius){o.push_back(s);if(int(o.size())>=limit)break;}return o;}
}

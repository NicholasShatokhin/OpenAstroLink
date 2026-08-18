#include "algorithms/guiding_engine.h"
#include <algorithm>
#include <cmath>

namespace oas {
GuidingStatus GuidingEngine::update(const EquatorialCoord&m,IMount*mount){if(!status_.active)return status_;double dra=status_.target.raDeg-m.raDeg;while(dra>180)dra-=360;while(dra<-180)dra+=360;status_.raErrorArcsec=dra*3600.0*std::cos(status_.target.decDeg*3.141592653589793/180.0);status_.decErrorArcsec=(status_.target.decDeg-m.decDeg)*3600.0;status_.rmsArcsec=std::hypot(status_.raErrorArcsec,status_.decErrorArcsec)/std::sqrt(2.0);if(mount){QString e;auto pulse=[&](double err,double agg,GuideDirection pos,GuideDirection neg){if(std::abs(err)<=deadbandArcsec_)return;int ms=int(std::clamp(std::abs(err)*agg*msPerArcsec_,50.0,2000.0));mount->pulseGuide(err>0?pos:neg,ms,&e);};pulse(status_.raErrorArcsec,raAgg_,GuideDirection::East,GuideDirection::West);pulse(status_.decErrorArcsec,decAgg_,GuideDirection::North,GuideDirection::South);}return status_;}
}

#pragma once
#include "core/interfaces.h"

namespace oas {
class GuidingEngine {
public:
    void setTarget(const EquatorialCoord &target){status_.target=target;status_.active=true;}
    void stop(){status_.active=false;}
    GuidingStatus status() const{return status_;}
    GuidingStatus update(const EquatorialCoord &measured,IMount *mount=nullptr);
    void setAggressiveness(double ra,double dec){raAgg_=ra;decAgg_=dec;}
private: GuidingStatus status_{};double raAgg_{0.6},decAgg_{0.6};double deadbandArcsec_{0.5};double msPerArcsec_{800.0};
};
}

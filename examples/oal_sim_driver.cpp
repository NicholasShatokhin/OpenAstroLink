#include "oal/driver_api.h"
#include <cstdlib>
#include <cstring>
#include <string>

namespace {OalDriverHostV1 host{};int focusPos=20000;double ra=83.8221,dec=-5.3911;char *copy(const std::string&s){auto*p=static_cast<char*>(host.allocate(s.size()+1));std::memcpy(p,s.c_str(),s.size()+1);return p;}bool start(void*,const char*){return true;}void stop(void*){}const char*devices(void*){return copy(R"([{"id":"mount","type":"mount","name":"Example mount"},{"id":"focuser","type":"focuser","name":"Example focuser"}])");}const char*invoke(void*,const char*device,const char*method,const char*){std::string d=device?device:"",m=method?method:"";if(d=="mount"&&m=="status")return copy("{\"ok\":true,\"data\":{\"raDeg\":"+std::to_string(ra)+",\"decDeg\":"+std::to_string(dec)+"}}");if(d=="focuser"&&m=="status")return copy("{\"ok\":true,\"data\":{\"position\":"+std::to_string(focusPos)+"}}");return copy(R"({"ok":false,"error":{"code":"NOT_IMPLEMENTED","message":"Example only"}})");}void release(void*,const char*p){host.deallocate(const_cast<char*>(p));}OalDriverV1 api{1,"example.sim","OAL example simulation driver","0.1",nullptr,&start,&stop,&devices,&invoke,&release};}
extern "C" OAL_DRIVER_EXPORT const OalDriverV1 *oalCreateDriverV1(const OalDriverHostV1 *h){host=*h;return &api;}

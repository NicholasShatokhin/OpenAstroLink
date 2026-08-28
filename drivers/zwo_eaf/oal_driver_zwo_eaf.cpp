#include "oal/driver_api.h"
#include <EAF_focuser.h>

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iomanip>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_map>

namespace {
OalDriverHostV2 host{};
struct FocuserState { int id{-1}; EAF_INFO info{}; std::atomic_bool connected{false}; std::mutex mutex; };
struct DriverState { std::mutex mutex; std::unordered_map<int,std::unique_ptr<FocuserState>> focusers; } state;
char *copyString(const std::string&s){auto*p=static_cast<char*>(host.allocate(host.hostContext,s.size()+1));std::memcpy(p,s.c_str(),s.size()+1);return p;}
std::string quote(const std::string&s){std::string o="\"";for(char c:s){if(c=='\\'||c=='\"')o+='\\';o+=c;}return o+'"';}
double number(const std::string &json,const std::string &key,double fallback){const auto p=json.find("\""+key+"\"");if(p==std::string::npos)return fallback;const auto c=json.find(':',p);if(c==std::string::npos)return fallback;char*e=nullptr;const double v=std::strtod(json.c_str()+c+1,&e);return e==json.c_str()+c+1?fallback:v;}
const char*ok(const std::string&d="{}"){return copyString("{\"ok\":true,\"data\":"+d+"}");}
const char*fail(const std::string&c,const std::string&m){return copyString("{\"ok\":false,\"error\":{\"code\":"+quote(c)+",\"message\":"+quote(m)+"}}");}
void event(const std::string&dev,const std::string&type,const std::string&payload="{}"){if(!host.emitEvent)return;const std::string e="{\"type\":"+quote(type)+",\"payload\":"+payload+"}";host.emitEvent(host.hostContext,"oal.zwo.eaf",dev.c_str(),e.c_str());}
std::string err(EAF_ERROR_CODE rc){return "EAF SDK error "+std::to_string(int(rc));}
int idFromDevice(const std::string&d){const std::string p="zwo-eaf:";if(d.rfind(p,0)!=0)return-1;char*e=nullptr;long v=std::strtol(d.c_str()+p.size(),&e,10);return e&&*e=='\0'?int(v):-1;}
FocuserState*focuserForId(int id){std::lock_guard<std::mutex>l(state.mutex);auto it=state.focusers.find(id);if(it!=state.focusers.end())return it->second.get();auto p=std::make_unique<FocuserState>();p->id=id;EAFGetProperty(id,&p->info);auto*r=p.get();state.focusers.emplace(id,std::move(p));return r;}
FocuserState*focuser(const std::string&d){int id=idFromDevice(d);return id<0?nullptr:focuserForId(id);}
std::string serialHex(int id){EAF_SN sn{};if(EAFGetSerialNumber(id,&sn)!=EAF_SUCCESS)return{};std::ostringstream o;o<<std::hex<<std::setfill('0');for(unsigned char b:sn.id)o<<std::setw(2)<<int(b);return o.str();}
bool start(void*,const char*){return true;} void stop(void*){std::lock_guard<std::mutex>l(state.mutex);for(auto&kv:state.focusers)if(kv.second->connected){EAFStop(kv.first);EAFClose(kv.first);kv.second->connected=false;}}
const char*manifest(void*){return copyString(R"({"driverId":"oal.zwo.eaf","name":"OpenAstroLink native ZWO EAF focuser driver","version":"0.2.10.25","abiVersion":2,"threadModel":"per-device-serial","transport":"ZWO EAF SDK"})");}
const char*devices(void*){const int n=EAFGetNum();std::ostringstream o;o<<'[';bool first=true;for(int i=0;i<n;++i){int id=-1;if(EAFGetID(i,&id)!=EAF_SUCCESS)continue;auto*f=focuserForId(id);EAFGetProperty(id,&f->info);if(!first)o<<',';first=false;o<<"{\"id\":"<<quote("zwo-eaf:"+std::to_string(id))<<",\"type\":\"focuser\",\"name\":"<<quote(f->info.Name)<<",\"vendor\":\"ZWO\",\"transport\":{\"kind\":\"vendor-sdk\",\"sdk\":\"EAF\",\"id\":"<<id<<"}}";}o<<']';return copyString(o.str());}
const char*caps(void*,const char*device){auto*f=focuser(device?device:"");if(!f)return copyString("{}");int maxStep=f->info.MaxStep,backlash=0;bool reverse=false;float temp=0;const bool opened=f->connected;if(opened){EAFGetMaxStep(f->id,&maxStep);EAFGetBacklash(f->id,&backlash);EAFGetReverse(f->id,&reverse);}std::ostringstream o;o<<"{\"schemaVersion\":\"1.0\",\"identity\":{\"vendor\":\"ZWO\",\"model\":"<<quote(f->info.Name)<<",\"id\":"<<f->id;if(opened){auto sn=serialHex(f->id);if(!sn.empty())o<<",\"serial\":"<<quote(sn);}o<<"},\"focuser\":{\"absoluteMove\":true,\"relativeMove\":true,\"halt\":true,\"position\":{\"min\":0,\"max\":"<<maxStep<<",\"unit\":\"step\"},\"movingState\":true,\"temperature\":{\"supported\":"<<(opened&&EAFGetTemp(f->id,&temp)==EAF_SUCCESS?"true":"false")<<"},\"backlash\":{\"supported\":true,\"current\":"<<backlash<<"},\"reverse\":{\"supported\":true,\"current\":"<<(reverse?"true":"false")<<"}}}";return copyString(o.str());}
const char*health(void*,const char*device){auto*f=focuser(device?device:"");return copyString(std::string("{\"state\":\"")+(f&&f->connected?"ok":"disconnected")+"\",\"connected\":"+(f&&f->connected?"true":"false")+"}");}
const char*invoke(void*,const char*device,const char*method,const char*request,const OalDriverCallV2*){const std::string dev=device?device:"",m=method?method:"",r=request?request:"{}";auto*f=focuser(dev);if(!f)return fail("INVALID_DEVICE","Unknown ZWO EAF");if(m=="device.connect"){std::lock_guard<std::mutex>g(f->mutex);if(f->connected)return ok();auto rc=EAFOpen(f->id);if(rc!=EAF_SUCCESS)return fail("EAF_OPEN_FAILED",err(rc));EAFGetProperty(f->id,&f->info);f->connected=true;event(dev,"device.connected");return ok();}if(m=="device.disconnect"){EAFStop(f->id);std::lock_guard<std::mutex>g(f->mutex);if(f->connected)EAFClose(f->id);f->connected=false;event(dev,"device.disconnected");return ok();}if(!f->connected)return fail("DEVICE_DISCONNECTED","ZWO EAF is not connected");if(m=="focuser.halt"){auto rc=EAFStop(f->id);return rc==EAF_SUCCESS?ok():fail("EAF_ERROR",err(rc));}std::lock_guard<std::mutex>g(f->mutex);if(m=="focuser.status"){int pos=0;bool moving=false,hand=false;float temp=0;auto rc=EAFGetPosition(f->id,&pos);if(rc!=EAF_SUCCESS)return fail("EAF_ERROR",err(rc));rc=EAFIsMoving(f->id,&moving,&hand);if(rc!=EAF_SUCCESS)return fail("EAF_ERROR",err(rc));std::ostringstream o;o<<"{\"position\":"<<pos<<",\"moving\":"<<(moving?"true":"false")<<",\"handControl\":"<<(hand?"true":"false");if(EAFGetTemp(f->id,&temp)==EAF_SUCCESS)o<<",\"temperatureC\":"<<temp;o<<'}';return ok(o.str());}if(m=="focuser.moveAbsolute"){int maxStep=f->info.MaxStep;EAFGetMaxStep(f->id,&maxStep);const int pos=std::clamp(int(number(r,"position",0)),0,maxStep);auto rc=EAFMove(f->id,pos);return rc==EAF_SUCCESS?ok("{\"targetPosition\":"+std::to_string(pos)+"}"):fail("EAF_ERROR",err(rc));}if(m=="focuser.moveRelative"){int cur=0,maxStep=f->info.MaxStep;if(EAFGetPosition(f->id,&cur)!=EAF_SUCCESS)return fail("EAF_ERROR","Could not read current EAF position");EAFGetMaxStep(f->id,&maxStep);const int pos=std::clamp(cur+int(number(r,"delta",0)),0,maxStep);auto rc=EAFMove(f->id,pos);return rc==EAF_SUCCESS?ok("{\"targetPosition\":"+std::to_string(pos)+"}"):fail("EAF_ERROR",err(rc));}return fail("NOT_IMPLEMENTED","Method is not implemented by native ZWO EAF driver");}
bool cancel(void*,const char*device,const char*){auto*f=focuser(device?device:"");return f&&f->connected&&EAFStop(f->id)==EAF_SUCCESS;}
void releaseString(void*,const char*p){if(p)host.deallocate(host.hostContext,const_cast<char*>(p));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,"oal.zwo.eaf","OpenAstroLink native ZWO EAF focuser driver","0.2.10.25",nullptr,&manifest,&start,&stop,&devices,&caps,&health,&invoke,&cancel,&releaseString};
}
extern "C" OAL_DRIVER_EXPORT const OalDriverV2*oalCreateDriverV2(const OalDriverHostV2*h){if(!h||h->abiVersion!=OAL_DRIVER_ABI_V2||h->structSize<sizeof(OalDriverHostV2))return nullptr;host=*h;return &api;}

#include "oal/driver_api.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace {
OalDriverHostV2 host{};
std::atomic_bool cameraConnected{false}, mountConnected{false}, focuserConnected{false};
std::atomic_bool exposureAbort{false}, mountAbort{false}, focusAbort{false};
std::mutex stateMutex;
double mountRa=83.822083, mountDec=-5.391111;
bool mountTracking=true, mountParked=false;
int focusPosition=20000;

char *copyString(const std::string &s) {
    auto *p = static_cast<char *>(host.allocate(host.hostContext, s.size() + 1));
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
std::string q(const std::string &s) {
    std::string o="\"";
    for(char c:s){if(c=='\\'||c=='\"')o+='\\';o+=c;}
    return o+'\"';
}
double number(const std::string &json,const std::string &key,double fallback){
    const auto p=json.find("\""+key+"\"");if(p==std::string::npos)return fallback;
    const auto c=json.find(':',p);if(c==std::string::npos)return fallback;
    char*end=nullptr;double v=std::strtod(json.c_str()+c+1,&end);return end==json.c_str()+c+1?fallback:v;
}
bool boolean(const std::string &json,const std::string &key,bool fallback){
    const auto p=json.find("\""+key+"\"");if(p==std::string::npos)return fallback;
    const auto c=json.find(':',p);if(c==std::string::npos)return fallback;
    const auto t=json.find_first_not_of(" \t\r\n",c+1);if(t==std::string::npos)return fallback;
    if(json.compare(t,4,"true")==0)return true;if(json.compare(t,5,"false")==0)return false;return fallback;
}
std::string stringValue(const std::string &json,const std::string &key,const std::string &fallback={}){
    const auto p=json.find("\""+key+"\"");if(p==std::string::npos)return fallback;const auto c=json.find(':',p);if(c==std::string::npos)return fallback;
    const auto a=json.find('"',c+1);if(a==std::string::npos)return fallback;const auto b=json.find('"',a+1);if(b==std::string::npos)return fallback;return json.substr(a+1,b-a-1);
}
const char *ok(const std::string &data="{}") { return copyString("{\"ok\":true,\"data\":"+data+"}"); }
const char *fail(const std::string &code,const std::string &message){return copyString("{\"ok\":false,\"error\":{\"code\":"+q(code)+",\"message\":"+q(message)+"}}");}
void event(const char *device,const std::string &type,const std::string &payload="{}"){
    if(!host.emitEvent)return;const std::string e="{\"type\":"+q(type)+",\"payload\":"+payload+"}";
    host.emitEvent(host.hostContext,"oal.simulated",device,e.c_str());
}

bool start(void*,const char*){return true;}
void stop(void*){cameraConnected=false;mountConnected=false;focuserConnected=false;}
const char *manifest(void*){
    return copyString(R"({"driverId":"oal.simulated","name":"OpenAstroLink native reference simulator","version":"0.2.10.25","abiVersion":2,"threadModel":"per-device-serial","capabilityModel":"typed-json-v1"})");
}
const char *devices(void*){
    return copyString(R"([
{"id":"sim-camera","type":"camera","name":"Native OAL simulated star camera","transport":{"kind":"virtual"}},
{"id":"sim-mount","type":"mount","name":"Native OAL simulated equatorial mount","transport":{"kind":"virtual"}},
{"id":"sim-focuser","type":"focuser","name":"Native OAL simulated absolute focuser","transport":{"kind":"virtual"}}
])");
}
const char *caps(void*,const char *device){
    const std::string d=device?device:"";
    if(d=="sim-camera") return copyString(R"({"schemaVersion":"1.0","camera":{"sensor":{"widthPx":1280,"heightPx":960,"nativeBits":16},"exposure":{"minSec":0.001,"maxSec":3600,"abortSupported":true},"gain":{"min":0,"max":500,"step":1},"offset":{"min":0,"max":255,"step":1},"binning":{"modes":[[1,1],[2,2]]},"roi":{"supported":true},"streaming":{"supported":false},"frameTransport":["host-frame-v2"]}}})");
    if(d=="sim-mount") return copyString(R"({"schemaVersion":"1.0","mount":{"slew":{"supported":true,"abortSupported":true},"sync":{"supported":true},"tracking":{"supported":true},"park":{"supported":true},"pulseGuide":{"supported":true,"maxDurationMs":5000},"pierSide":{"supported":true},"limits":{"decMinDeg":-90,"decMaxDeg":90}}})");
    if(d=="sim-focuser") return copyString(R"({"schemaVersion":"1.0","focuser":{"absolute":{"supported":true,"minPosition":0,"maxPosition":100000,"step":1},"relative":{"supported":true},"halt":{"supported":true},"temperature":{"supported":true},"backlash":{"supported":false}}})");
    return copyString("{}");
}
const char *health(void*,const char *device){
    const std::string d=device?device:"";bool c=d=="sim-camera"?cameraConnected.load():d=="sim-mount"?mountConnected.load():d=="sim-focuser"?focuserConnected.load():false;
    return copyString(std::string("{\"state\":\"")+(c?"ok":"disconnected")+"\",\"connected\":"+(c?"true":"false")+"}");
}

const char *invoke(void*,const char *device,const char *method,const char *request,const OalDriverCallV2*){
    const std::string d=device?device:"",m=method?method:"",r=request?request:"{}";
    if(m=="device.connect"){
        if(d=="sim-camera")cameraConnected=true;else if(d=="sim-mount")mountConnected=true;else if(d=="sim-focuser")focuserConnected=true;else return fail("DEVICE_NOT_FOUND","Unknown simulated device");
        event(device,"device.connected");return ok();
    }
    if(m=="device.disconnect"){
        if(d=="sim-camera")cameraConnected=false;else if(d=="sim-mount")mountConnected=false;else if(d=="sim-focuser")focuserConnected=false;else return fail("DEVICE_NOT_FOUND","Unknown simulated device");
        event(device,"device.disconnected");return ok();
    }
    if(d=="sim-camera"){
        if(!cameraConnected)return fail("DEVICE_DISCONNECTED","Camera is not connected");
        if(m=="camera.abortExposure"){exposureAbort=true;return ok();}
        if(m=="camera.capture"){
            const double sec=std::clamp(number(r,"exposureSec",1.0),0.001,3600.0);const int gain=int(number(r,"gain",0));
            int binX=std::max(1,int(number(r,"binX",1))),binY=std::max(1,int(number(r,"binY",1)));
            int w=1280/binX,h=960/binY;exposureAbort=false;
            const auto until=std::chrono::steady_clock::now()+std::chrono::milliseconds(std::max(1,int(sec*1000.0)));
            while(std::chrono::steady_clock::now()<until){if(exposureAbort)return fail("CANCELLED","Exposure cancelled");std::this_thread::sleep_for(std::chrono::milliseconds(20));}
            std::vector<std::uint16_t> pixels(std::size_t(w)*std::size_t(h),std::uint16_t(600));
            std::mt19937 rng(42+int(focusPosition));std::uniform_int_distribution<int> px(20,std::max(21,w-21)),py(20,std::max(21,h-21));
            const double blur=1.2+std::abs(focusPosition-22000)/1400.0;
            for(int s=0;s<80;++s){int cx=px(rng),cy=py(rng);double amp=12000.0+(s%9)*3500.0;int rad=std::min(9,std::max(2,int(std::ceil(blur*3))));for(int yy=-rad;yy<=rad;++yy)for(int xx=-rad;xx<=rad;++xx){int x=cx+xx,y=cy+yy;if(x<0||x>=w||y<0||y>=h)continue;double val=amp*std::exp(-(xx*xx+yy*yy)/(2*blur*blur));auto &v=pixels[std::size_t(y)*w+x];v=std::uint16_t(std::min(65535.0,double(v)+val));}}
            const auto now=std::chrono::system_clock::now();const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count();
            const std::string frameId="sim-"+std::to_string(ns);OalFrameDescriptorV2 f{};f.structSize=sizeof(f);f.frameIdUtf8=frameId.c_str();f.width=w;f.height=h;f.strideBytes=w*2;f.pixelFormat=OAL_PIXEL_MONO16;f.bitsPerSample=16;f.channels=1;f.capturedUnixNs=ns;f.exposureSec=sec;f.gain=gain;f.data=reinterpret_cast<const std::uint8_t*>(pixels.data());f.dataBytes=pixels.size()*sizeof(std::uint16_t);f.metadataJsonUtf8="{\"synthetic\":true}";
            const auto token=host.publishFrame?host.publishFrame(host.hostContext,"oal.simulated",device,&f):0;if(!token)return fail("FRAME_PUBLISH_FAILED","Host rejected simulated frame");
            event(device,"camera.frameReady","{\"frameToken\":"+std::to_string(token)+"}");return ok("{\"frameToken\":"+std::to_string(token)+",\"frameId\":"+q(frameId)+"}");
        }
    }
    if(d=="sim-mount"){
        if(!mountConnected)return fail("DEVICE_DISCONNECTED","Mount is not connected");
        std::lock_guard<std::mutex> lock(stateMutex);
        if(m=="mount.status")return ok("{\"raDeg\":"+std::to_string(mountRa)+",\"decDeg\":"+std::to_string(mountDec)+",\"tracking\":"+(mountTracking?"true":"false")+",\"slewing\":false,\"parked\":"+(mountParked?"true":"false")+",\"pierSide\":\"unknown\"}");
        if(m=="mount.slew"){mountAbort=false;mountRa=std::fmod(number(r,"raDeg",mountRa)+360.0,360.0);mountDec=std::clamp(number(r,"decDeg",mountDec),-90.0,90.0);event(device,"mount.position","{\"raDeg\":"+std::to_string(mountRa)+",\"decDeg\":"+std::to_string(mountDec)+"}");return ok();}
        if(m=="mount.abort"){mountAbort=true;return ok();}
        if(m=="mount.sync"){mountRa=number(r,"raDeg",mountRa);mountDec=number(r,"decDeg",mountDec);return ok();}
        if(m=="mount.setTracking"){mountTracking=boolean(r,"enabled",mountTracking);return ok();}
        if(m=="mount.park"){mountParked=boolean(r,"parked",mountParked);if(mountParked)mountTracking=false;return ok();}
        if(m=="mount.pulseGuide")return ok();
    }
    if(d=="sim-focuser"){
        if(!focuserConnected)return fail("DEVICE_DISCONNECTED","Focuser is not connected");
        if(m=="focuser.status")return ok("{\"position\":"+std::to_string(focusPosition)+",\"moving\":false,\"temperatureC\":12.5}");
        if(m=="focuser.moveAbsolute"){focusPosition=std::clamp(int(number(r,"position",focusPosition)),0,100000);event(device,"focuser.position","{\"position\":"+std::to_string(focusPosition)+"}");return ok();}
        if(m=="focuser.moveRelative"){focusPosition=std::clamp(focusPosition+int(number(r,"delta",0)),0,100000);return ok();}
        if(m=="focuser.halt"){focusAbort=true;return ok();}
    }
    return fail("NOT_IMPLEMENTED","Method is not implemented by reference simulator");
}

bool cancel(void*,const char *device,const char*){const std::string d=device?device:"";if(d=="sim-camera")exposureAbort=true;else if(d=="sim-mount")mountAbort=true;else if(d=="sim-focuser")focusAbort=true;else return false;return true;}
void releaseString(void*,const char *p){if(p)host.deallocate(host.hostContext,const_cast<char*>(p));}
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_FRAME_PUBLISH|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,
                "oal.simulated","OpenAstroLink native reference simulator","0.2.10.25",nullptr,&manifest,&start,&stop,&devices,&caps,&health,&invoke,&cancel,&releaseString};
} // namespace

extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *h){if(!h||h->abiVersion!=OAL_DRIVER_ABI_V2||h->structSize<sizeof(OalDriverHostV2))return nullptr;host=*h;return &api;}

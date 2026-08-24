#include "oal/driver_api.h"
#include <ASICamera2.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <mutex>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

namespace {
OalDriverHostV2 host{};

struct CameraState {
    int cameraId{-1};
    ASI_CAMERA_INFO info{};
    std::atomic_bool connected{false};
    std::atomic_bool cancelRequested{false};
    std::mutex operationMutex;
};

struct DriverState {
    std::mutex mutex;
    std::unordered_map<int, std::unique_ptr<CameraState>> cameras;
} state;

char *copyString(const std::string &s) {
    auto *p = static_cast<char *>(host.allocate(host.hostContext, s.size() + 1));
    std::memcpy(p, s.c_str(), s.size() + 1);
    return p;
}
std::string quote(const std::string &s) {
    std::string o = "\"";
    for (char c : s) { if (c == '\\' || c == '\"') o += '\\'; o += c; }
    return o + '"';
}
double number(const std::string &json, const std::string &key, double fallback) {
    const auto p = json.find("\"" + key + "\""); if (p == std::string::npos) return fallback;
    const auto c = json.find(':', p); if (c == std::string::npos) return fallback;
    char *e = nullptr; const double v = std::strtod(json.c_str() + c + 1, &e);
    return e == json.c_str() + c + 1 ? fallback : v;
}
bool objectNumber(const std::string &json,const std::string &obj,const std::string &key,double &out) {
    const auto p=json.find("\""+obj+"\""); if(p==std::string::npos)return false;
    const auto b=json.find('{',p),e=json.find('}',b); if(b==std::string::npos||e==std::string::npos)return false;
    const auto chunk=json.substr(b,e-b+1); const double sentinel=-9.87654321e307;
    const auto v=number(chunk,key,sentinel); if(v==sentinel)return false; out=v; return true;
}
const char *ok(const std::string &data = "{}") { return copyString("{\"ok\":true,\"data\":" + data + "}"); }
const char *fail(const std::string &code, const std::string &message) {
    return copyString("{\"ok\":false,\"error\":{\"code\":" + quote(code) + ",\"message\":" + quote(message) + "}}");
}
void event(const std::string &device, const std::string &type, const std::string &payload = "{}") {
    if (!host.emitEvent) return;
    const std::string e = "{\"type\":" + quote(type) + ",\"payload\":" + payload + "}";
    host.emitEvent(host.hostContext, "oal.zwo.asi", device.c_str(), e.c_str());
}
std::string errText(ASI_ERROR_CODE rc) { return "ASI SDK error " + std::to_string(int(rc)); }

int idFromDevice(const std::string &device) {
    const std::string prefix = "zwo-asi:";
    if (device.rfind(prefix, 0) != 0) return -1;
    char *end = nullptr; const long v = std::strtol(device.c_str()+prefix.size(), &end, 10);
    return (end && *end == '\0') ? int(v) : -1;
}
CameraState *cameraForId(int id) {
    std::lock_guard<std::mutex> lock(state.mutex);
    auto it = state.cameras.find(id); if (it != state.cameras.end()) return it->second.get();
    auto p = std::make_unique<CameraState>(); p->cameraId = id;
    ASIGetCameraPropertyByID(id, &p->info);
    auto *ret = p.get(); state.cameras.emplace(id, std::move(p)); return ret;
}
CameraState *camera(const std::string &device) { const int id=idFromDevice(device); return id<0?nullptr:cameraForId(id); }

bool hasImageType(const ASI_CAMERA_INFO &i, ASI_IMG_TYPE t) {
    for (auto v : i.SupportedVideoFormat) { if (v == ASI_IMG_END) break; if (v == t) return true; }
    return false;
}
ASI_IMG_TYPE preferredImageType(const ASI_CAMERA_INFO &i, int requestedBits) {
    if (requestedBits > 8 && hasImageType(i, ASI_IMG_RAW16)) return ASI_IMG_RAW16;
    if (hasImageType(i, ASI_IMG_RAW8)) return ASI_IMG_RAW8;
    if (hasImageType(i, ASI_IMG_Y8)) return ASI_IMG_Y8;
    if (hasImageType(i, ASI_IMG_RGB24)) return ASI_IMG_RGB24;
    return ASI_IMG_END;
}
bool setControl(int id, ASI_CONTROL_TYPE type, long value, std::string &error) {
    const ASI_ERROR_CODE rc = ASISetControlValue(id, type, value, ASI_FALSE);
    if (rc != ASI_SUCCESS) { error = errText(rc); return false; }
    return true;
}
std::string controlRange(CameraState *c, ASI_CONTROL_TYPE type, const char *unit = nullptr) {
    if (!c || !c->connected) return "null";
    int n = 0; if (ASIGetNumOfControls(c->cameraId, &n) != ASI_SUCCESS) return "null";
    for (int i=0;i<n;++i) {
        ASI_CONTROL_CAPS cap{}; if (ASIGetControlCaps(c->cameraId, i, &cap) != ASI_SUCCESS) continue;
        if (cap.ControlType != type) continue;
        std::ostringstream o; o << "{\"supported\":true,\"min\":" << cap.MinValue << ",\"max\":" << cap.MaxValue
                                << ",\"default\":" << cap.DefaultValue << ",\"writable\":" << (cap.IsWritable?"true":"false");
        if (unit) o << ",\"unit\":" << quote(unit);
        o << '}';
        return o.str();
    }
    return "null";
}

bool start(void *, const char *) { return true; }
void stop(void *) {
    std::lock_guard<std::mutex> lock(state.mutex);
    for (auto &kv : state.cameras) if (kv.second->connected) { ASIStopExposure(kv.first); ASIStopVideoCapture(kv.first); ASICloseCamera(kv.first); kv.second->connected=false; }
}
const char *manifest(void *) {
    return copyString(R"({"driverId":"oal.zwo.asi","name":"OpenAstroLink native ZWO ASI camera driver","version":"0.2.10.11","abiVersion":2,"threadModel":"per-device-serial","transport":"ZWO ASI SDK"})");
}
const char *devices(void *) {
    const int n = ASIGetNumOfConnectedCameras(); std::ostringstream o; o << '['; bool first=true;
    for (int i=0;i<n;++i) {
        ASI_CAMERA_INFO info{}; if (ASIGetCameraProperty(&info, i) != ASI_SUCCESS) continue;
        auto *c=cameraForId(info.CameraID); c->info=info;
        if(!first) o << ',';
        first=false;
        o << "{\"id\":" << quote("zwo-asi:"+std::to_string(info.CameraID)) << ",\"type\":\"camera\",\"name\":" << quote(info.Name)
          << ",\"vendor\":\"ZWO\",\"transport\":{\"kind\":\"vendor-sdk\",\"sdk\":\"ASI\",\"cameraId\":" << info.CameraID << "}}";
    }
    o << ']'; return copyString(o.str());
}
const char *caps(void *, const char *device) {
    auto *c=camera(device?device:""); if(!c)return copyString("{}"); const auto &i=c->info;
    std::ostringstream bins; bins << '['; bool first=true; for(int b:i.SupportedBins){if(!b)break;if(!first)bins<<',';first=false;bins<<b;} bins<<']';
    std::ostringstream fmts; fmts << '['; first=true; for(auto f:i.SupportedVideoFormat){if(f==ASI_IMG_END)break;if(!first)fmts<<',';first=false;const char*name=f==ASI_IMG_RAW8?"raw8":f==ASI_IMG_RAW16?"raw16":f==ASI_IMG_RGB24?"rgb24":"y8";fmts<<quote(name);}fmts<<']';
    std::ostringstream o; o << "{\"schemaVersion\":\"1.0\",\"identity\":{\"vendor\":\"ZWO\",\"model\":" << quote(i.Name) << ",\"cameraId\":" << i.CameraID << "},\"camera\":{"
      << "\"sensor\":{\"widthPx\":" << i.MaxWidth << ",\"heightPx\":" << i.MaxHeight << ",\"pixelSizeUm\":" << i.PixelSize << ",\"nativeBits\":" << i.BitDepth << ",\"color\":" << (i.IsColorCam?"true":"false") << "},"
      << "\"binning\":{\"supported\":true,\"values\":" << bins.str() << "},\"formats\":" << fmts.str() << ","
      << "\"gain\":" << controlRange(c,ASI_GAIN) << ",\"offset\":" << controlRange(c,ASI_OFFSET) << ","
      << "\"exposure\":{\"supported\":true,\"abortSupported\":true,\"sdkUnit\":\"microsecond\"},"
      << "\"cooling\":{\"supported\":" << (i.IsCoolerCam?"true":"false") << "},\"st4\":{\"supported\":" << (i.ST4Port?"true":"false") << "},"
      << "\"roi\":{\"supported\":true},\"singleFrame\":{\"supported\":true,\"abortSupported\":true},\"streaming\":{\"supported\":true,\"sdkMode\":\"ASI video capture\"},\"frameTransport\":[\"host-frame-v2\"]}}";
    return copyString(o.str());
}
const char *health(void *, const char *device) { auto*c=camera(device?device:""); return copyString(std::string("{\"state\":\"")+(c&&c->connected?"ok":"disconnected")+"\",\"connected\":"+(c&&c->connected?"true":"false")+"}"); }

const char *invoke(void *, const char *device, const char *method, const char *request, const OalDriverCallV2 *call) {
    const std::string dev=device?device:"", m=method?method:"", r=request?request:"{}"; auto*c=camera(dev); if(!c)return fail("INVALID_DEVICE","Unknown ZWO ASI camera");
    if(m=="device.connect") {
        std::lock_guard<std::mutex> guard(c->operationMutex); if(c->connected)return ok();
        auto rc=ASIOpenCamera(c->cameraId); if(rc!=ASI_SUCCESS)return fail("ASI_OPEN_FAILED",errText(rc));
        rc=ASIInitCamera(c->cameraId); if(rc!=ASI_SUCCESS){ASICloseCamera(c->cameraId);return fail("ASI_INIT_FAILED",errText(rc));}
        ASIGetCameraPropertyByID(c->cameraId,&c->info); c->cancelRequested=false; c->connected=true; event(dev,"device.connected"); return ok();
    }
    if(m=="device.disconnect") {
        c->cancelRequested=true; ASIStopExposure(c->cameraId); ASIStopVideoCapture(c->cameraId); std::lock_guard<std::mutex> guard(c->operationMutex);
        if(c->connected) ASICloseCamera(c->cameraId);
        c->connected=false; event(dev,"device.disconnected"); return ok();
    }
    if(!c->connected)return fail("DEVICE_DISCONNECTED","ZWO ASI camera is not connected");
    if(m=="camera.abortExposure") { c->cancelRequested=true; const auto rc=ASIStopExposure(c->cameraId); return (rc==ASI_SUCCESS||rc==ASI_ERROR_INVALID_SEQUENCE)?ok():fail("ASI_ERROR",errText(rc)); }
    if(m=="camera.capture") {
        std::lock_guard<std::mutex> guard(c->operationMutex); c->cancelRequested=false;
        const int bin=std::max(1,int(number(r,"binX",1))); if(int(number(r,"binY",bin))!=bin)return fail("UNSUPPORTED_BINNING","ASI SDK uses symmetric binning in this driver profile");
        int x=0,y=0,w=int(c->info.MaxWidth)/bin,h=int(c->info.MaxHeight)/bin; double v=0;
        if(objectNumber(r,"roi","x",v)) x=std::max(0,int(v));
        if(objectNumber(r,"roi","y",v)) y=std::max(0,int(v));
        if(objectNumber(r,"roi","width",v)) w=std::max(1,int(v));
        if(objectNumber(r,"roi","height",v)) h=std::max(1,int(v));
        w=std::min(w,int(c->info.MaxWidth)/bin-x); h=std::min(h,int(c->info.MaxHeight)/bin-y);
        const int requestedBits=int(number(r,"bitsPerSample",c->info.BitDepth)); const ASI_IMG_TYPE fmt=preferredImageType(c->info,requestedBits); if(fmt==ASI_IMG_END)return fail("UNSUPPORTED_PIXEL_FORMAT","No supported ASI output format");
        auto rc=ASISetROIFormat(c->cameraId,w,h,bin,fmt); if(rc!=ASI_SUCCESS)return fail("ASI_ROI_FAILED",errText(rc));
        rc=ASISetStartPos(c->cameraId,x,y); if(rc!=ASI_SUCCESS && (x||y))return fail("ASI_ROI_FAILED",errText(rc));
        std::string e; const double expSec=std::max(0.000001,number(r,"exposureSec",1.0)); const long expUs=long(std::llround(expSec*1e6));
        if(!setControl(c->cameraId,ASI_EXPOSURE,expUs,e)||!setControl(c->cameraId,ASI_GAIN,long(number(r,"gain",0)),e)||!setControl(c->cameraId,ASI_OFFSET,long(number(r,"offset",0)),e))return fail("ASI_CONTROL_FAILED",e);
        rc=ASIStartExposure(c->cameraId,ASI_FALSE); if(rc!=ASI_SUCCESS)return fail("ASI_EXPOSURE_FAILED",errText(rc));
        ASI_EXPOSURE_STATUS status=ASI_EXP_WORKING;
        while(status==ASI_EXP_WORKING) {
            if(c->cancelRequested || (host.isCancellationRequested && call && call->operationIdUtf8 && host.isCancellationRequested(host.hostContext,call->operationIdUtf8))) { ASIStopExposure(c->cameraId); return fail("CANCELLED","ASI exposure cancelled"); }
            std::this_thread::sleep_for(std::chrono::milliseconds(10)); rc=ASIGetExpStatus(c->cameraId,&status); if(rc!=ASI_SUCCESS)return fail("ASI_EXPOSURE_FAILED",errText(rc));
        }
        if(status!=ASI_EXP_SUCCESS)return fail("ASI_EXPOSURE_FAILED","ASI exposure did not complete successfully");
        const int channels=fmt==ASI_IMG_RGB24?3:1; const int bytesPerSample=fmt==ASI_IMG_RAW16?2:1; const long bytes=long(w)*h*channels*bytesPerSample; std::vector<unsigned char> buffer((std::size_t(bytes)));
        rc=ASIGetDataAfterExp(c->cameraId,buffer.data(),bytes); if(rc!=ASI_SUCCESS)return fail("ASI_READOUT_FAILED",errText(rc));
        const auto now=std::chrono::system_clock::now(); const auto ns=std::chrono::duration_cast<std::chrono::nanoseconds>(now.time_since_epoch()).count(); const std::string frameId="zwo-asi-"+std::to_string(c->cameraId)+"-"+std::to_string(ns);
        std::uint32_t pf=OAL_PIXEL_UNKNOWN; if(fmt==ASI_IMG_RGB24)pf=OAL_PIXEL_RGB8; else if(fmt==ASI_IMG_RAW16)pf=c->info.IsColorCam?OAL_PIXEL_BAYER16:OAL_PIXEL_MONO16; else pf=c->info.IsColorCam?OAL_PIXEL_BAYER8:OAL_PIXEL_MONO8;
        OalFrameDescriptorV2 f{}; f.structSize=sizeof(f); f.frameIdUtf8=frameId.c_str(); f.width=w; f.height=h; f.strideBytes=w*channels*bytesPerSample; f.pixelFormat=pf; f.bitsPerSample=bytesPerSample*8; f.channels=channels; f.capturedUnixNs=ns; f.exposureSec=expSec; f.gain=number(r,"gain",0); f.data=buffer.data(); f.dataBytes=buffer.size(); f.metadataJsonUtf8="{\"vendor\":\"ZWO\",\"sdk\":\"ASI\"}";
        const auto token=host.publishFrame?host.publishFrame(host.hostContext,"oal.zwo.asi",dev.c_str(),&f):0; if(!token)return fail("FRAME_PUBLISH_FAILED","OAL host rejected ASI frame"); event(dev,"camera.frameReady","{\"frameToken\":"+std::to_string(token)+"}"); return ok("{\"frameToken\":"+std::to_string(token)+",\"frameId\":"+quote(frameId)+"}");
    }
    return fail("NOT_IMPLEMENTED","Method is not implemented by native ZWO ASI driver");
}
bool cancel(void *, const char *device, const char *) { auto*c=camera(device?device:""); if(!c||!c->connected)return false; c->cancelRequested=true; const auto rc=ASIStopExposure(c->cameraId); return rc==ASI_SUCCESS||rc==ASI_ERROR_INVALID_SEQUENCE; }
void releaseString(void *, const char *p) { if(p)host.deallocate(host.hostContext,const_cast<char*>(p)); }
OalDriverV2 api{OAL_DRIVER_ABI_V2,sizeof(OalDriverV2),OAL_DRIVER_FEATURE_EVENTS|OAL_DRIVER_FEATURE_FRAME_PUBLISH|OAL_DRIVER_FEATURE_CANCELLATION|OAL_DRIVER_FEATURE_HEALTH,
                "oal.zwo.asi","OpenAstroLink native ZWO ASI camera driver","0.2.10.11",nullptr,&manifest,&start,&stop,&devices,&caps,&health,&invoke,&cancel,&releaseString};
}
extern "C" OAL_DRIVER_EXPORT const OalDriverV2 *oalCreateDriverV2(const OalDriverHostV2 *h) { if(!h||h->abiVersion!=OAL_DRIVER_ABI_V2||h->structSize<sizeof(OalDriverHostV2))return nullptr; host=*h; return &api; }
